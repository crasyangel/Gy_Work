#ifndef _NGOD_GW_GETCFG_C_
#define _NGOD_GW_GETCFG_C_

#include "vod_app_api.h"
#include "ngod_gw_qamname.h"
#include "ngod_gw_getcfg.h"
#include "ngod_gw_a7proxy.h"
#include "ngod_gw_bisortT.h"

/********************** Local define *****************************************/

/*3383 为大端系统 7430 为小端系统*/
/*所有整形的传递需要rocme_porting_socket_htonl转换*/
#define SUMAGW_MSG_LEN  (512)
#define SUMAGW_MSG_DHCP (0x00102001)	/*获取option151~155中服务器url命令*/

/*HTTP socket 发送和接收缓冲区大小*/
#define SUMA_HTTP_SEND_BUF	1024
#define SUMA_HTTP_RECV_BUF  8196

//其他
#define ONE_LINE_LENGTH 512
#define SRV_PORT 7759
#define MAX_TIME_WAIT 5*60*1000
#define FAIL_TIME_BEGIN 4*1000


/*默认使用的HTTP头*/
#define HTTP_HEADER_FIRST    "GET"
#define HTTP_HEADER_VER      "HTTP/1.1"
#define HTTP_ACCEPT          "Accept: text/plain"
#define HTTP_USER_AGENT      "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) " \
     "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/28.0.1500.95 Safari/537.36 SE 2.X MetaSr 1.0"
#define HTTP_LANGUAGE        "Accept-Language: en-us,en;q=0.5"
#define HTTP_CONNECTION      "Connection: keep-alive"


#define NGOD_VOD_QAMNAME_SRV_ADDR "127.0.0.1"        //服务器地址
#define NGOD_VOD_QAMNAME_SRV_PORT 55455              //服务端端口号
#define NGOD_VOD_QAMNAME_TIMEOUT  6*1000               //超时时间

//http://portalIP:80/tsids.cfg
#define QAMNAME_CFG_NAME "/tsids.cfg"

//屏蔽循环打印消息
//#define PRINT_ALWAYS_MSG
#ifdef PRINT_ALWAYS_MSG
	#define VOD_EXT_ALWAYS(x) VOD_EXT_INFO(x)
#else
	#define VOD_EXT_ALWAYS(x)
#endif

//第一个字符不是数字，则置该字符串为空，首字符为NULL即可
//以后的字符无法判断
#define NOT_DIGIT_RESET(str)					\
{												\
	if(!isdigit(*str)) 							\
	{											\
		*str = 0;								\
	}											\
}

//********************** struct typedef *************************************

typedef struct {
    int  msg;                 /*控制3383的消息 SUMAGW_MSG_DHCP 获取option60字段 SUMAGW_MSG_REST 复位3383*/
    int  ret;                 /*0命令执行成功，非0 命令执行失败*/
    int  len;                 /*arg 的长度*/
    char arg[SUMAGW_MSG_LEN]; /*命令执行成功或失败的描述*/
}sumagw_msg_t;

/*get_line函数中所用的结构体*/
typedef struct {
    size_t len;
    CHAR_T *current;
    CHAR_T *buf;
}cbuf_t;

typedef struct {
    CHAR_T  cscfg_md5[32];
    CHAR_T 	portal_addr[32];   
    CHAR_T 	portal_port[16];   
    CHAR_T 	ntp_addr[32];
    CHAR_T 	ntp_port[16];
    CHAR_T 	acs_addr[32];
    CHAR_T 	acs_port[16];
    CHAR_T 	sessionserver_addr[64];
    CHAR_T 	sessionserver_port[16];
    CHAR_T 	serverid[32];
    CHAR_T 	serverid_port[16];
}cscfg_info_t;


//********************** static globle variable *************************************

//portal_server和portal_http的同步事件/消息
static UINT32_T portal_msg;
enum{ MSG_NONE, MSG_URL_GET_SUCCESS };

//全局变量，用于存储3383发过来的信息和是否改变的flag
static sumagw_msg_t getcfg_share;

//静态读写锁，用于读写	getcfg_share	
static UINT32_T getcfg_lock;

//全局变量，用于临时存储配置文件的内容
static cscfg_info_t cscfg_save;

//静态读写锁，用于读写	cscfg_save			
static UINT32_T savecfg_lock;

//条件变量，用于标志需要保存新的配置文件
static struct {
	UINT32_T mutex;
	UINT32_T cond;
	INT8_T nready;
}cscfg_save_ready;


//********************** Local Functions *************************************

/*
TCP服务器，监听3383的TCP连接；
1、可以用于多客户端通信
2、信息收取成功后，向3383发送单字节，通知3383关闭连接
3、存储新的dhcp option到全局变量getcfg_share中，并设置dhcp_change_flag为真
*/
static void portal_server(void *param)
{
	VOD_EXT_WARN(("TCP server is online...\n"));
	
    int server_socket;
    int client_socket;
	int sockfd, i, maxi, maxfd;
	
    struct Roc_Sock_Addr_In client_addr;
    struct Roc_Sock_Addr_In server_addr;
    socklen_t addr_len = 0;
	
	int nready, client[ROC_FD_SETSIZE];  
	Roc_Fd_Set rset, allset;  

	struct Roc_Time_Val tm; 
	int nbytes;

	int recv_bytes;
    Roc_Queue_Message_t s_msg = {0,0,0,0};
    
    if((server_socket=rocme_porting_socket(ROC_AF_INET, ROC_SOCK_STREAM, 0))<0)
	{
        VOD_EXT_ERRO(("socket create failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
    }
	
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family    = ROC_AF_INET;
    server_addr.sin_addr.addr = rocme_porting_socket_htonl(ROC_INADDR_ANY);
    server_addr.sin_port      = rocme_porting_socket_htons(SRV_PORT);

	const int on = 1;
	rocme_porting_socket_setsockopt(server_socket,ROC_SOL_SOCKET,ROC_SO_REUSEADDR,&on,sizeof(on)); 
    if((rocme_porting_socket_bind(server_socket, (struct Roc_Sock_Addr*)&server_addr, sizeof(server_addr))) < 0)
	{
        VOD_EXT_ERRO(("socket bind failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
    }
    
    if((rocme_porting_socket_listen(server_socket, 8)) < 0)
	{
        VOD_EXT_ERRO(("socket listen failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
    }

	maxfd = server_socket;
	maxi = -1;
	for(i=0; i<ROC_FD_SETSIZE; i++)
		client[i] = -1;
	
	rocme_porting_socket_fd_zero(&allset);
	rocme_porting_socket_fd_set(server_socket, &allset);

    sumagw_msg_t sumagw_msg_server;
	
    while(1)
	{
		VOD_EXT_ALWAYS(("waiting select and accept ...\n"));
		
		rset = allset;
        nready = rocme_porting_socket_select( maxfd+1, &rset, NULL, NULL, NULL);
		
        if(rocme_porting_socket_fd_isset(server_socket, &rset)) 
		{
			addr_len = sizeof(client_addr);
			client_socket = rocme_porting_socket_accept(server_socket,(struct Roc_Sock_Addr*)&client_addr,(socklen_t*)&addr_len);

			for(i=0; i<ROC_FD_SETSIZE; i++)
			{
				if(client[i] < 0)
				{
					client[i] = client_socket;
					break;
				}
			}

			if(i == ROC_FD_SETSIZE)
			{
				VOD_EXT_ERRO(("reach maximum connections, close all client socket\n"));

				for(i=0; i<ROC_FD_SETSIZE; i++)
				{
					rocme_porting_socket_close(client[i]);
					rocme_porting_socket_fd_clr(client[i], &allset);
					client[i] = -1;
				}
				continue;
			}
			
			rocme_porting_socket_fd_set(client_socket, &allset);
			if(client_socket > maxfd) 
				maxfd = client_socket;
			if(i > maxi)	
				maxi = i;

			if ( --nready <= 0 ) 
			{  
				VOD_EXT_DBUG((" no more readable fd\n"));
				continue;
			}
        }

		for(i=0; i<=maxi; i++)
		{
			if((sockfd = client[i]) < 0)
				continue;
			if(rocme_porting_socket_fd_isset (sockfd, &rset))
			{
				memset(&sumagw_msg_server, 0x00, sizeof(sumagw_msg_server));
				recv_bytes = 0;
				
				tm.tv_sec  = 3;
				tm.tv_usec = 0;
				rocme_porting_socket_setsockopt(sockfd,ROC_SOL_SOCKET,ROC_SO_RCVTIMEO,&tm,sizeof(tm)); 
				
				nbytes = rocme_porting_socket_recv(sockfd, &sumagw_msg_server, sizeof(sumagw_msg_server), 0);
				if( nbytes < 0 ) 
				{
					VOD_EXT_ERRO((" client %d: read error: %d!\n",rocme_porting_socket_get_last_errno()));
				}
				else if (0 == nbytes)
				{
					VOD_EXT_DBUG((" client %d: read EOF signal, close sockfd\n", i));
					rocme_porting_socket_close(sockfd);
					rocme_porting_socket_fd_clr(sockfd, &allset);
					client[i] = -1;
				}
				else
				{
					if(nbytes <= 12)	
					{
						VOD_EXT_ERRO((" client %d: read less than 12 bytes, cannot parse the length\n", i));
						continue;
					}
					recv_bytes = nbytes;
					while(recv_bytes < (sizeof(sumagw_msg_server)+rocme_porting_socket_ntohl(sumagw_msg_server.len)-SUMAGW_MSG_LEN))
					{
						rocme_porting_task_msleep(5); 
						nbytes = rocme_porting_socket_recv(sockfd, &sumagw_msg_server+recv_bytes, 
												sizeof(sumagw_msg_server)-recv_bytes, 0);
						if( nbytes < 0 ) 
						{
							VOD_EXT_ERRO((" client %d: read error: %d!\n",rocme_porting_socket_get_last_errno()));
						}
						else if (0 == nbytes)
						{
							VOD_EXT_DBUG((" client %d: read EOF signal, close sockfd\n", i));
							rocme_porting_socket_close(sockfd);
							rocme_porting_socket_fd_clr(sockfd, &allset);
							client[i] = -1;
							break;
						}
						else
						{
							recv_bytes += nbytes;
						}
					}
								
					switch(rocme_porting_socket_ntohl(sumagw_msg_server.msg)) 
					{
						case SUMAGW_MSG_DHCP:
						{
							if(1 != rocme_porting_socket_send(sockfd, "", 1, 0))
							{
								VOD_EXT_ERRO((" client %d: write error: %d!\n",rocme_porting_socket_get_last_errno()));
							}else
							{
								rocme_porting_mutex_lock(getcfg_lock);
								getcfg_share.msg = rocme_porting_socket_ntohl(sumagw_msg_server.msg);
								getcfg_share.ret = rocme_porting_socket_ntohl(sumagw_msg_server.ret);
								getcfg_share.len = rocme_porting_socket_ntohl(sumagw_msg_server.len);
								strncpy(getcfg_share.arg, sumagw_msg_server.arg, sizeof(getcfg_share.arg));
								*(getcfg_share.arg + sizeof(getcfg_share.arg) - 1) = 0;
								
								VOD_EXT_DBUG(("msg=0x%x\n ret=%d\n len=%d\n arg=%s\n", 
										getcfg_share.msg, getcfg_share.ret, 
										getcfg_share.len, getcfg_share.arg));
								rocme_porting_mutex_unlock(getcfg_lock);

								s_msg.q1stWordOfMsg = MSG_URL_GET_SUCCESS;
								rocme_porting_queue_send(portal_msg, &s_msg);
							}
							break;
						}
						default:
							break;
					}
				}	
				
				if ( --nready <= 0 ) 
				{	
					VOD_EXT_DBUG((" no more readable fd\n"));
					break;
				}
			}
		}
    }
    
	rocme_porting_socket_close(server_socket);
	rocme_porting_queue_destroy(portal_msg);
	portal_msg = 0;
	
	VOD_EXT_INFO(("exit %s \n", __FUNCTION__));
	return;
}

#if 0
/*
判断3383与7429通信的socketselect状态
*/
static int isready(int fd, int rw, int timeout/*ms*/ )  
{  
    int rc;  
    fd_set fds;  
    struct timeval tv;  

    FD_ZERO(&fds);  
    FD_SET (fd, &fds);  

    tv.tv_sec  = timeout/1000;
    tv.tv_usec = (timeout%1000)*1000;  
    if(rw == 0 ) 
    {
        rc = select( fd+1, &fds, NULL, NULL, &tv);  
    }
    else 
    {
        rc = select( fd+1, NULL, &fds, NULL, &tv);  
    }

    VOD_EXT_DBUG(("select rc = %d\n", rc));
    if(rc < 0) return 0;

    return 1;
}



/*
实现7429从3383中获取dhcp option中的服务器URL字段的功能
*/

static int hgw_ctl_3383(int type, int send_timeout, int recv_timeout, sumagw_msg_t *sumagw_msg)
{
    int hsocket;
    struct sockaddr_in server_addr;

    int n = 0;
    int i = 0;
    int r = 0;
    int f = 0;
    socklen_t len = sizeof(int); 

    if((hsocket=socket(PF_INET,SOCK_STREAM,0))<0) 
    {
        perror("[VOD EXT]socket error:");
        return -1;
    }
  
    memset((void*)&server_addr, 0x00, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.88.2");
    server_addr.sin_port        = rocme_porting_socket_htons(SRV_PORT);
    if((r=connect(hsocket, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
       perror("[VOD EXT]connect error:");
       return -1;
    }
    
    VOD_EXT_DBUG(("connect r=%d errno=%d\n", r, errno));
    
    sumagw_msg->msg = rocme_porting_socket_htonl(type);
    sumagw_msg->ret = rocme_porting_socket_htonl(0);
    sumagw_msg->len = rocme_porting_socket_htonl(0);
    strcpy(sumagw_msg->arg, "cscfg:URL=http://10.1.0.4/vod.cfg&cfgmd5=123456");
    sumagw_msg->len = strlen(sumagw_msg->arg);
       
    if( 1 != isready(hsocket, 1, send_timeout) ) 
    {
        close(hsocket);
        return -1;
    }
    
    if((n=write(hsocket, sumagw_msg, sizeof(sumagw_msg_t)-SUMAGW_MSG_LEN+sumagw_msg->len)) < 0) 
    {
        perror("[VOD EXT]write error:");
        close(hsocket);
        return -1;
    }
    VOD_EXT_DBUG(("write nbyte=%d\n", n));

    close(hsocket);
    
    return 0;
}
#endif


/*
去除字符串中多余的空字符
*/
static void trimspace(CHAR_T* pStr)  
{  
    CHAR_T *pTmp = pStr;  
      
    while (*pStr != '\0')   
    {  
        if (!isspace(*pStr))  
        {  
            *pTmp++ = *pStr;  
        }  
        ++pStr;  
    }  
    *pTmp = '\0';  
}  



/*
每次从内存中读一行字符串，跳过只包含空字符的行
*/
static void get_line( cbuf_t *cbuf, CHAR_T *psz_string)
{
    size_t iLen = 0;
    CHAR_T *pByte = cbuf->current;
    size_t iRemDataLen = cbuf->len - (cbuf->current - cbuf->buf);

    if((NULL == cbuf->buf) || (NULL == cbuf->current))
    {
        return;
    }

    while (iLen <= iRemDataLen) 
    {
        if (!isspace(*pByte)) break;
        pByte++;
    }
    cbuf->current = pByte;
    
    while (iLen <= iRemDataLen) 
    {
        if (*pByte == '\n' || *pByte == '\r') break;	//MAC系统中把'\r'作为1行的结束，而没有'\n'
        iLen++;
        pByte++;
    }
    
	if(iLen >= ONE_LINE_LENGTH)
	{
        VOD_EXT_ERRO(("the line is too long!!!\n"));
        return;
	}

	if(NULL != psz_string) 
    {
        memcpy(psz_string, cbuf->current, iLen);
        *(psz_string+iLen) = '\0';
    }

    trimspace(psz_string);
    cbuf->current += iLen;
    return;
}


/*
从URL解析出来的IP地址:端口中获取URL中指定的配置文件
*/
static INT32_T get_http
	(const CHAR_T *ipaddr, const INT32_T port, CHAR_T *PayloadBuf, const INT32_T nPayloadBuf, const CHAR_T *cfgname)
{
    INT32_T sockfd=0, ret=-1, i=0, h=0, len=0;
    CHAR_T *ptmp = NULL;
    INT32_T ntmp = 0;
    INT32_T http_resp_code = 0;
    INT32_T content_len = 0;
    CHAR_T tmpBuf[256] = {0};
    CHAR_T sendBuf[SUMA_HTTP_SEND_BUF] = {0};
    INT32_T nPreRecvBuff;
    Roc_Fd_Set t_set1;
    struct Roc_Sock_Addr_In servaddr;
    struct Roc_Time_Val tv;

    VOD_EXT_WARN(("enter %s \n",__FUNCTION__));

/*
建立socket连接
*/	
    sockfd = rocme_porting_socket(ROC_AF_INET, ROC_SOCK_STREAM, 0);
    if(sockfd < 0) 
    {
        VOD_EXT_ERRO(("socket() failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
        goto auth_req_portal_exit;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = ROC_AF_INET;
    servaddr.sin_port = rocme_porting_socket_htons(port);
    if (rocme_porting_socket_inet_aton(ipaddr, &servaddr.sin_addr) <= 0 )
    {
        VOD_EXT_ERRO(("ipaddr inet_aton error!\n"));
        goto auth_req_portal_exit;
    }

    struct Roc_Time_Val tm = {1,0};
    INT32_T opt_recv = 20*1024;

    rocme_porting_socket_setsockopt(sockfd,ROC_SOL_SOCKET,ROC_SO_SNDTIMEO,&tm,sizeof(tm)); 
    rocme_porting_socket_setsockopt(sockfd,ROC_SOL_SOCKET,ROC_SO_RCVBUF,&opt_recv,sizeof(opt_recv)); 

    if (rocme_porting_socket_connect(sockfd, (struct Roc_Sock_Addr*)&servaddr, sizeof(servaddr)) < 0)
    {
        VOD_EXT_ERRO(("connect() failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
        goto auth_req_portal_exit;
    }
    VOD_EXT_DBUG(("socket connect success! \n"));

    memset(sendBuf, 0, SUMA_HTTP_SEND_BUF);
    
/*
拼接HTTP头
*/
    memset(tmpBuf, 0, 256);
    strcat(tmpBuf, HTTP_HEADER_FIRST);
    strcat(tmpBuf, " ");
    strcat(tmpBuf, cfgname);
    strcat(tmpBuf, " ");
    strcat(tmpBuf, HTTP_HEADER_VER);
    strcat(tmpBuf, "\r\n");
    strcat(sendBuf, tmpBuf);
    memset(tmpBuf, 0, 256);
    sprintf(tmpBuf, "Host: %s:%d\r\n",ipaddr, port);
    strcat(sendBuf, tmpBuf);
    strcat(sendBuf, HTTP_USER_AGENT);
    strcat(sendBuf, "\r\n");
    strcat(sendBuf, HTTP_ACCEPT);
    strcat(sendBuf, "\r\n");
    strcat(sendBuf, HTTP_LANGUAGE);
    strcat(sendBuf, "\r\n");
    strcat(sendBuf, HTTP_CONNECTION);
    strcat(sendBuf, "\r\n");
    strcat(sendBuf, "\r\n\r\n");
    
/*
发送HTTP头
*/
    VOD_EXT_ALWAYS(("HTTP get(portal), sendBuf len=%d, sendBuf=\n%s\n", strlen(sendBuf), sendBuf));
    len = rocme_porting_socket_send(sockfd,sendBuf,strlen(sendBuf),0);
    if (len < 0) 
    {
        VOD_EXT_ERRO(("send() failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
        goto auth_req_portal_exit;
    } else 
    {
        VOD_EXT_DBUG(("send success, send %d bytes !\n", len));
    }

/*
select为阻塞式recv提供超时机制，循环recv可以多次接收数据
如果sockfd中已经没有数据，则退出循环
*/
    len = 0;
    while(1) 
    {
   	 	rocme_porting_socket_fd_zero(&t_set1);
    	rocme_porting_socket_fd_set(sockfd, &t_set1);
        tv.tv_sec= 3;
        tv.tv_usec= 0;
        h = 0;
        h = rocme_porting_socket_select(sockfd +1, &t_set1, NULL, NULL, &tv);
        
        if (h < 0) 
        {  
            VOD_EXT_ERRO(("socket select error, errno=%d !\n",rocme_porting_socket_get_last_errno()));
            continue;
        }

        if(0 == h) 
        {  
            VOD_EXT_DBUG(("waiting over time, the socket is not ready, keep waiting!\n"));
            continue;
        }

        if(h > 0) 
        {
            nPreRecvBuff = 0;  
            rocme_porting_socket_ioctl(sockfd, ROC_FIONREAD, &nPreRecvBuff);
            if(0 == nPreRecvBuff) 
            {
                VOD_EXT_DBUG(("nPreRecvBuff=0, no more data, quit loop!\n"));
                break;
            }

            i = rocme_porting_socket_recv(sockfd, PayloadBuf + len, nPreRecvBuff, 0);
            if (nPreRecvBuff != i) 
            {
                VOD_EXT_ERRO(("recv() failed, errno=%d !\n",rocme_porting_socket_get_last_errno()));
                goto auth_req_portal_exit;
            }
            len += i;
            
            *(PayloadBuf + len) = 0;
			VOD_EXT_ALWAYS(("HTTP PayloadBuf=\n%s\n", PayloadBuf));
			
			ptmp = strstr(PayloadBuf, "HTTP/1.1");
			if(NULL == ptmp) 
			{
				VOD_EXT_ERRO(("bad HTTP response !\n"));
				break;	
			}
			
			http_resp_code = atoi(ptmp+strlen("HTTP/1.1"));
			if(200 != http_resp_code) 
			{
				VOD_EXT_ERRO(("HTTP response code is %d !\n", http_resp_code));
				break;	
			}
			
			ptmp = strstr(PayloadBuf,"Content-Length:");
			if(NULL == ptmp) 
			{
				VOD_EXT_ERRO(("no content length !\n"));
				break;	
			}
			
			content_len = atoi(ptmp+strlen("Content-Length:"));
			VOD_EXT_DBUG(("content_len=%d\n",content_len));

			if(SUMA_HTTP_RECV_BUF < content_len)
			{
				VOD_EXT_ERRO(("content_len is too long\n"));
				break;
			}
			
			ptmp = strstr(PayloadBuf,"\r\n\r\n");
			ptmp += strlen("\r\n\r\n");
			if(content_len == strlen(ptmp))
			{
                VOD_EXT_DBUG(("recv data over, quit loop!\n"));
				*(PayloadBuf + nPayloadBuf -1) = 0;
				INT32_T skiplen = ptmp - PayloadBuf;
				while(*PayloadBuf)
				{
					*PayloadBuf = *(PayloadBuf + skiplen);
					PayloadBuf++;
				}
				ret = 0;
				break;
			}
			else
			{
                VOD_EXT_DBUG(("need to recv more data, continue loop!\n"));
			}

            rocme_porting_task_msleep(5); 
        }
    }

/*
程序退出
*/	
auth_req_portal_exit:
    rocme_porting_socket_close(sockfd);

    VOD_EXT_WARN(("leave %s, ret=%d \n",__FUNCTION__, ret));
    return ret;
}

/*
将新值替换旧值
并拼接成新的字段
*/
static INT32_T rewrite_Itemvalue(CHAR_T *jsonStr, const CHAR_T *location, CHAR_T *newvalue)
{
	CHAR_T *p = NULL;
	CHAR_T *p1 = NULL;
	INT32_T pos1 = 0;
	CHAR_T *p2 = NULL;
	INT32_T pos2 = 0;
	
    CHAR_T tempStr[128] = {0};
    CHAR_T head[1024] = {0};
	CHAR_T tail[1024] = {0};
	
	p = strstr(jsonStr, location);
	p1 = strchr(p, ':');
	++p1;	//exclude colon ':'
	
	if (NULL != p1)
	{
		pos1 = p1 - jsonStr;
		p2 = strchr(p1, ',');
		if (NULL != p2)
		{
			pos2 = p2 - p1;
	
		}
		else	//reach the end of json
		{
			p2 = strchr(p1, '}');
			pos2 = p2 - p1;
		}
	}
	else
	{
		VOD_EXT_ERRO(("location not found! \n"));
		return -1;
	}

	strncpy(head, jsonStr, pos1);
	*(head + sizeof(head) -1) = 0;
	
	strncpy(tempStr, p1, pos2);
	*(tempStr + sizeof(tempStr) -1) = 0;
	
	strncpy(tail, p2, strlen(p2));
	*(tail + sizeof(tail) -1) = 0;
	
	VOD_EXT_ALWAYS(("head: %s \n%s: %s \ntail: %s \n", head, location, tempStr, tail));
	
	memset(jsonStr, 0, strlen(jsonStr));
	sprintf(jsonStr, "%s\"%s\"%s", head, newvalue, tail);
	
	VOD_EXT_ALWAYS(("newstr after rewrite:\n %s \n", jsonStr));
	
	return 0;
}

/*
读取配置文件中的内容
*/
static INT32_T Sys_Prop_Item_Get
	(const CHAR_T* sys_prop_key, const CHAR_T *name, CHAR_T *value, const INT32_T valuelen)
{
    INT32_T rc = 0;
    
    cJSON *pRoot = NULL;
    cJSON *pValue = NULL;
    CHAR_T jsonStr[1024] = {0};

	if(!sys_prop_key || !name || !value)
	{
		VOD_EXT_ERRO(("some param given is NULL!!!\n"));
		return -1;
	}

	if(-1 == SYS_Prop_Get(sys_prop_key, jsonStr, sizeof(jsonStr)))
	{
		VOD_EXT_ERRO(("SYS_Prop_Get_%s failed.\n", sys_prop_key));
		return -1;
	}
	*(jsonStr + sizeof(jsonStr) -1) = 0;
	
    VOD_EXT_DBUG(("jsonStr_%s(get):\n%s\n", name, jsonStr));
	
	pRoot = cJSON_Parse(jsonStr);
    if(NULL == pRoot) 
    {
        VOD_EXT_ERRO(("%s cJSON_Parse failed !!!\n", name));
		return -1;
    }

	pValue = cJSON_GetObjectItem(pRoot, name);
	if((NULL != pValue)&&(cJSON_String == pValue->type)) 
	{
	    strncpy(value, pValue->valuestring, valuelen);
        VOD_EXT_DBUG(("get %s value is %s\n", name, value));
	}
	else
	{
		VOD_EXT_ERRO(("cJSON_GetObjectItem %s failed!!!\n", name));
		rc = -1;
	}
	
	if(NULL != pRoot) 
	{
		cJSON_Delete(pRoot);
	}
	return rc;
}

/*
将新的sessionServer 和serverId值替换从SYS_PROP_KEY_VODChannel获取的旧值
并拼接成新的SYS_PROP_KEY_VODConfig字段
*/
static INT32_T Sys_Prop_Item_save
	(const CHAR_T* sys_prop_key, bool total_save, const CHAR_T *name, CHAR_T *newvalue)
{
    INT32_T rc = 0;
    
    cJSON *pRoot = NULL;
    cJSON *pValue = NULL;
    CHAR_T jsonStr[1024] = {0};

	if(!sys_prop_key || !name || !newvalue)
	{
		VOD_EXT_ERRO(("some param given is NULL!!!\n"));
		return -1;
	}

    if(0 == strcmp(name, "sessionServer")) {
        Roc_VOD_IOCtl_Ex(0, VOD_IOCTL_CMD_NGOD_SM_ADDR_SET, newvalue, NULL);
    }
	
	if(-1 == SYS_Prop_Get(sys_prop_key, jsonStr, sizeof(jsonStr)))
	{
		VOD_EXT_ERRO(("SYS_Prop_Get_%s failed.\n", sys_prop_key));
		return -1;
	}
	*(jsonStr + sizeof(jsonStr) -1) = 0;
	
    VOD_EXT_DBUG(("jsonStr_%s(get):\n%s\n", name, jsonStr));
	VOD_EXT_DBUG(("jsonStr_%s newvalue: %s\n", name, newvalue));
	
	if(total_save)
	{
		if (0 == strcmp(jsonStr, newvalue)) 
    	{
			VOD_EXT_DBUG(("the %s is not change!\n", name));
            return 0;
    	}

		if(-1 == SYS_Prop_Set(sys_prop_key, newvalue))
        {
			VOD_EXT_ERRO(("SYS_Prop_Set %s failed!\n", name));
            return -1;
        } 
        
		if(-1 == SYS_Prop_Save(sys_prop_key))
        {
            VOD_EXT_ERRO(("SYS_Prop_Save %s failed!\n", name));
            return -1;
        }      
    	
        VOD_EXT_WARN(("SYS_Prop_Save %s success!\n", name));
		return 0;
	}
    
	pRoot = cJSON_Parse(jsonStr);
    if(NULL == pRoot) 
    {
        VOD_EXT_ERRO(("%s cJSON_Parse failed !!!\n", name));
		return -1;
    }

	pValue = cJSON_GetObjectItem(pRoot, name);
	if((NULL != pValue)&&(cJSON_String == pValue->type)) 
	{
		if((0 != strcmp(pValue->valuestring, newvalue)))
		{
			if(ROC_OK != rewrite_Itemvalue(jsonStr, name, newvalue))
			{
				VOD_EXT_ERRO(("rewrite new %s value failed \n", name));
				rc = -1;
				goto END;
			}
		}
		else
		{
			VOD_EXT_WARN(("the %s is not change!\n", name));
		}
	}
	else
	{
		VOD_EXT_ERRO(("cJSON_GetObjectItem %s failed!!!\n", name));
		rc = -1;
		goto END;
	}
	
    VOD_EXT_DBUG(("%s(set&save):\n%s\n", name, jsonStr));

    if(-1 == SYS_Prop_Set(sys_prop_key, jsonStr))
    {
        VOD_EXT_ERRO(("SYS_Prop_Set %s failed!\n", name));
		rc = -1;
		goto END;
    } 
    
    if(-1 == SYS_Prop_Save(sys_prop_key))
    {
        VOD_EXT_ERRO(("SYS_Prop_Save %s failed!\n", name));
		rc = -1;
		goto END;
    }      
    
    VOD_EXT_WARN(("SYS_Prop_Save %s success!\n", name));
		
END:
	
	if(NULL != pRoot) 
	{
		cJSON_Delete(pRoot);
	}
	return rc;
}

/*
程序主函数，实现以下功能：
1、测试从3383获取到的URL是否符合规范规定；
2、如果符合规范规定，获取URL中包含的MD5，并与保存在本地的MD5字段对比
	如果相同，则URL和配置文并无变化，sleep一小时；
3、从URL中获取服务器的地址:端口和配置文件路径、名称等信息，如果端口缺省，指定端口为80；
4、向获取到的地址:端口请求指定的配置文件，并保存到本地；当配置文件中的内容更新完毕，最后更新新的MD5值；
5、读取dhcp_change_flag，如果为真则更新portal服务器反馈；如果为假，跳转到循环末尾；
6、更新完md5之后，即更新完全部信息后，置dhcp_change_flag为假
*/
static void portal_http_task(void *param)
{
	char 	sumagw_arg[SUMAGW_MSG_LEN] = {0};

    cJSON 	*pRoot_md5 			= NULL;
    cJSON 	*pValue_md5 		= NULL;
    CHAR_T 	jsonStr_md5[1024] 	= {0};
    CHAR_T 	newstr[1024] 		= {0};
    
    CHAR_T* p = NULL;
    cbuf_t 	payload_buf 		= {0, NULL, NULL};
    CHAR_T 	tmp[32] 			= {0};
    CHAR_T  str_tmp[ONE_LINE_LENGTH] = {0};
    
    char 	cfg_name[64] 		= {0};
    CHAR_T 	http_req[64] 		= {0};
    CHAR_T 	http_req_host[32] 	= {0}; 
    INT32_T	http_req_port   	= 0;
    CHAR_T 	name[32] 			= {0};
	cscfg_info_t cscfg_get;
	
    Roc_Queue_Message_t p_msg = {0,0,0,0};
	UINT32_T time_wait = FAIL_TIME_BEGIN;

	//too big array will cause thread stack segment fault
	INT32_T nPayloadBuf = SUMA_HTTP_RECV_BUF*sizeof(CHAR_T);
	CHAR_T *payloadbuf = malloc(nPayloadBuf);
	memset(payloadbuf, 0, nPayloadBuf);

    VOD_EXT_WARN(("enter %s \n",__FUNCTION__));
    
    INT32_T md5_len = sizeof(cscfg_save.cscfg_md5);
    if (0 != Sys_Prop_Item_Get(SYS_PROP_KEY_VODChannel, "MD5", cscfg_save.cscfg_md5, md5_len))
    {
        VOD_EXT_ERRO(("Sys_Prop_Item_Get MD5 failed\n"));
    }

    while(1)
    {
		#ifdef LOCAL_TEST
		sumagw_msg_t sumagw_portal;
        hgw_ctl_3383(SUMAGW_MSG_DHCP, 3000, 3000, &sumagw_portal);
		continue;
		#endif

		if(0 == portal_msg)
		{
			VOD_EXT_ERRO(("portal_server has exit, we cannot go further, just exit!!!\n"));
			break;
		}
		
		memset(&p_msg, 0, sizeof(Roc_Queue_Message_t));
		if(ROC_OS_QUEUE_SEM_FAILURE == 
				rocme_porting_queue_recv(portal_msg, &p_msg, ROC_TIMEOUT_INFINITY))
		{
			VOD_EXT_ERRO(("msg recv failed!!!\n"));
			continue;
		}
		else if (MSG_URL_GET_SUCCESS != p_msg.q1stWordOfMsg)
		{
			VOD_EXT_ERRO(("recv msg is wrong!!!\n"));
			continue;
		}
		else
		{
			VOD_EXT_ALWAYS(("get new url!!!\n"));
		}
		
		rocme_porting_mutex_lock(getcfg_lock);
		memset(sumagw_arg, 0, sizeof(sumagw_arg));
		strncpy(sumagw_arg, getcfg_share.arg, getcfg_share.len);
		*(sumagw_arg+sizeof(sumagw_arg)-1) = 0;
		rocme_porting_mutex_unlock(getcfg_lock);

		if(0 == strlen(sumagw_arg))
		{
			VOD_EXT_ERRO(("socket msg is empty!!!\n"));
			continue;
		}
		
		VOD_EXT_DBUG(("portal url: \n%s\n", sumagw_arg));

		memset(tmp, 0, sizeof(tmp));
        strncpy(tmp, sumagw_arg, strlen("cscfg"));
        *(tmp+sizeof(tmp)-1) = 0;
		
        /*判断URL是否非空，并且符合规范*/   
        if((0 != strcmp(tmp, "cscfg")) || (NULL == strstr(sumagw_arg, "http://")))
        {
            VOD_EXT_ERRO(("portal url format error, one example is \"" \
                          "cscfg:URL=http://10.1.0.4/vod.cfg&cfgmd5=123456\"\n"));
            continue;
        }
    
        p = strstr(sumagw_arg, "cfgmd5=");
        if(NULL == p)
        {
            VOD_EXT_ERRO(("the url has no cfgmd5=\n"));
            continue;
        }
        
		 /*获取临时存储的MD5值*/
		rocme_porting_mutex_lock(savecfg_lock);
		strncpy(cscfg_get.cscfg_md5, cscfg_save.cscfg_md5, sizeof(cscfg_get.cscfg_md5));
        *(cscfg_get.cscfg_md5+sizeof(cscfg_get.cscfg_md5)-1) = 0;
        
		rocme_porting_mutex_unlock(savecfg_lock);
 		VOD_EXT_DBUG(("the old md5: %s\n", cscfg_get.cscfg_md5));

		memset(tmp, 0, sizeof(tmp));
        sscanf(p,"%*[^=]=%[a-zA-Z0-9]",tmp);
        tmp[sizeof(tmp)-1] = 0;
        VOD_EXT_DBUG(("the new md5: %s\n", tmp));
		
		/*如果MD5值没有变化，则认为portal和NTP地址未变化*/
        if((strlen(cscfg_get.cscfg_md5) == strlen(tmp)) && (0 == strcmp(tmp,cscfg_get.cscfg_md5)))/*MD5 */
        {
            VOD_EXT_DBUG(("MD5: [%s] is same as local!\n",tmp));
            continue;
        }
        
		//cover the old value
		memset(cscfg_get.cscfg_md5, 0, sizeof(cscfg_get.cscfg_md5));
		strncpy(cscfg_get.cscfg_md5, tmp, sizeof(cscfg_get.cscfg_md5));
        *(cscfg_get.cscfg_md5+sizeof(cscfg_get.cscfg_md5)-1) = 0;
        
		/*获取URL中的IP地址、端口和配置文件路径*/
        p = strstr(sumagw_arg, "http://");
        sscanf(p,"%*[^/]//%[^/]",http_req);
        
        sscanf(http_req,"%[^:]",http_req_host);
        if (NULL == strstr(http_req, ":")) 
        {
            http_req_port = 80;
        }
        else
        {
			memset(tmp, 0, sizeof(tmp));
            sscanf(http_req,"%*[^:]:%[0-9]",tmp);
            http_req_port = atoi(tmp);
        }
        VOD_EXT_DBUG(("http_req is %s:%d\n", http_req_host, http_req_port));

        sscanf(p,"%*[^/]//%*[^/]%[^&]",cfg_name);
        VOD_EXT_DBUG(("cfg_name=%s\n", cfg_name));

        if(0 != get_http(http_req_host, http_req_port, payloadbuf, nPayloadBuf, cfg_name)) 
        {
              VOD_EXT_ERRO(("get_http failed !!!\n"));
			  goto FAILED_RESEND;
        }
        
		/*
		写入从配置文件获取的portal和NTP地址到本地，最后更新MD5
		防止portal和NTP中途更新失败，却更新了MD5  
		*/
        payload_buf.buf = payloadbuf;
        payload_buf.current = payloadbuf;
        payload_buf.len = strlen(payloadbuf);
        VOD_EXT_DBUG(("HTTP get(portal), payload_buf len=%d, payload_buf=\n%s\n",
        				payload_buf.len, payload_buf.buf));

        while ((payload_buf.len != (payload_buf.current - payload_buf.buf)) &&
        		(0 != *(payload_buf.current))) 
        {
            memset(str_tmp,0,sizeof(str_tmp));
            memset(name,0,sizeof(name));
            get_line(&payload_buf, str_tmp);

            if(0 == strlen(str_tmp))
            {
				VOD_EXT_DBUG(("str_tmp is empty!\n"));
				continue;
            }
            VOD_EXT_ALWAYS(("the line is %s\n", str_tmp));
            
            sscanf(str_tmp, "%[^=]", name);
            if (0 != strlen(name)) 
            {
                if (0 == strcasecmp(name, "PortalAddress")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.portal_addr); 
                    NOT_DIGIT_RESET(cscfg_get.portal_addr);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.portal_addr));
                    continue;
                }
                if(0 == strcasecmp(name, "PortalPort")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.portal_port); 
                    NOT_DIGIT_RESET(cscfg_get.portal_port);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.portal_port));
                    continue;
                }
                if (0 == strcasecmp(name, "NTPAddress")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.ntp_addr); 
                    NOT_DIGIT_RESET(cscfg_get.ntp_addr);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.ntp_addr));
                    continue;
                }
                if(0 == strcasecmp(name, "NTPPort")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.ntp_port); 
                    NOT_DIGIT_RESET(cscfg_get.ntp_port);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.ntp_port));
                    continue;
                }
                if (0 == strcasecmp(name, "ACSAddress")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.acs_addr); 
                    NOT_DIGIT_RESET(cscfg_get.acs_addr);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.acs_addr));
                    continue;
                }
                if(0 == strcasecmp(name, "ACSPort")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.acs_port); 
                    NOT_DIGIT_RESET(cscfg_get.acs_port);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.acs_port));
                    continue;
                }
                if(0 == strcasecmp(name, "SessionServerAddr")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.sessionserver_addr); 
                    NOT_DIGIT_RESET(cscfg_get.sessionserver_addr);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.sessionserver_addr));
                    continue;
                }
                if(0 == strcasecmp(name, "SessionServerPort")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.sessionserver_port); 
                    NOT_DIGIT_RESET(cscfg_get.sessionserver_port);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.sessionserver_port));
                    continue;
                }
                if(0 == strcasecmp(name, "ServerId")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.serverid); 
                    NOT_DIGIT_RESET(cscfg_get.serverid);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.serverid));
                    continue;
                }
                if(0 == strcasecmp(name, "ServerIdPort")) 
                {
                    sscanf(str_tmp, "%*[^=]=%s", cscfg_get.serverid_port); 
                    NOT_DIGIT_RESET(cscfg_get.serverid_port);
                    VOD_EXT_DBUG(("%s is %s\n", name, cscfg_get.serverid_port));
                    continue;
                }
            }
        }

		rocme_porting_mutex_lock(savecfg_lock);
		memcpy(&cscfg_save, &cscfg_get, sizeof(cscfg_info_t));
		rocme_porting_mutex_unlock(savecfg_lock);

		//每次更新都使portal_save线程保存新更新的字段
		bool dosignal;
		rocme_porting_mutex_lock(cscfg_save_ready.mutex);
		dosignal = (0 == cscfg_save_ready.nready);
		cscfg_save_ready.nready++;
		rocme_porting_mutex_unlock(cscfg_save_ready.mutex);

		if(dosignal)
		{
			rocme_porting_cond_signal(cscfg_save_ready.cond);
		}
		
		time_wait = FAIL_TIME_BEGIN;
		continue;
		
FAILED_RESEND:
		p_msg.q1stWordOfMsg = MSG_URL_GET_SUCCESS;
		rocme_porting_queue_send(portal_msg, &p_msg);

		//exponential backoff
		if(time_wait >= MAX_TIME_WAIT)
		{
			time_wait = FAIL_TIME_BEGIN;
		}
		time_wait *= 2;
		rocme_porting_task_msleep(time_wait);
	}
  
  free(payloadbuf);
  payloadbuf = NULL;
  
  rocme_porting_mutex_destroy(getcfg_lock);
  getcfg_lock = 0;
  
  VOD_EXT_INFO(("exit %s \n", __FUNCTION__));
}

static void portal_save(void *param)
{
    INT32_T status = 0;
    INT32_T portal_save_flag = 0;
	UINT32_T time_wait = FAIL_TIME_BEGIN;
	INT8_T nready_temp = 0;
	
    cscfg_info_t cscfg_save_temp;
    CHAR_T total_save[64] = {0};
	CHAR_T sessionserver_addr_save[32] = {0};
	CHAR_T sessionserver_save[64] = {0};
	CHAR_T serverid_save[64] = {0};
	
	BiTree binary_sort_tree = NULL;
	
	INT32_T nPayloadBuf = SUMA_HTTP_RECV_BUF*sizeof(CHAR_T);
	CHAR_T *payloadbuf = malloc(nPayloadBuf);
	memset(payloadbuf, 0, nPayloadBuf);
	
	while(1)
	{
		if(0 == getcfg_lock)
		{
			VOD_EXT_ERRO(("portal_http_task has exit, we cannot go further, just exit!!!\n"));
			break;
		}
		
		rocme_porting_mutex_lock(cscfg_save_ready.mutex);
		while(0 == cscfg_save_ready.nready)
		{
			rocme_porting_cond_wait(cscfg_save_ready.cond, cscfg_save_ready.mutex, ROC_TIMEOUT_INFINITY);
		}
		nready_temp = cscfg_save_ready.nready;
		rocme_porting_mutex_unlock(cscfg_save_ready.mutex);
		
		VOD_EXT_ALWAYS(("start to save!!!\n"));
		
		rocme_porting_mutex_lock(savecfg_lock);
		memcpy(&cscfg_save_temp, &cscfg_save, sizeof(cscfg_info_t));
		rocme_porting_mutex_unlock(savecfg_lock);
		
		status = 0;
		portal_save_flag = -1;
		if ((0 != strlen(cscfg_save_temp.portal_addr)) && (0 != strlen(cscfg_save_temp.portal_port)))
		{
			vod_update_a7_with_cfgfile(cscfg_save_temp.portal_addr, cscfg_save_temp.portal_port);

			memset(total_save, 0, sizeof(total_save));

			//这里使用atoi和%d防止port字段最后有非数字的情况
	        sprintf(total_save, "{\"address\":\"%s\",\"port\":\"%d\"}", 
	        						cscfg_save_temp.portal_addr, atoi(cscfg_save_temp.portal_port));
	        if (-1 == Sys_Prop_Item_save(SYS_PROP_KEY_Portal, true, "Portal", total_save))
	        {
				VOD_EXT_ERRO(("PORTAL save failed\n"));
	        	status = -1;
	        	portal_save_flag = -1;
	        }
	        else
	        {
	        	portal_save_flag = 0;
	        }
        }
        
		if ((0 != strlen(cscfg_save_temp.ntp_addr)) && (0 != strlen(cscfg_save_temp.ntp_port)))
		{
			memset(total_save, 0, sizeof(total_save));
	        sprintf(total_save, "{\"address\":\"%s\",\"port\":\"%d\"}", 
	        						cscfg_save_temp.ntp_addr, atoi(cscfg_save_temp.ntp_port));
	        if (-1 == Sys_Prop_Item_save(SYS_PROP_KEY_NTP, true, "NTP", total_save))
	        {
				VOD_EXT_ERRO(("NTP save failed\n"));
	        	status = -1;
	        }
		}
        
		if ((0 != strlen(cscfg_save_temp.acs_addr)) && (0 != strlen(cscfg_save_temp.acs_port)))
		{
			memset(total_save, 0, sizeof(total_save));
	        sprintf(total_save, "{\"address\":\"%s\",\"port\":\"%d\"}", 
	        						cscfg_save_temp.acs_addr, atoi(cscfg_save_temp.acs_port));
	        if (-1 == Sys_Prop_Item_save(SYS_PROP_KEY_ACS, true, "ACS", total_save))
	        {
				VOD_EXT_ERRO(("ACS save failed\n"));
	        	status = -1;
	        }
		}
        
		if ((0 != strlen(cscfg_save_temp.sessionserver_addr)))
		{
			CHAR_T sessionserver_addr1[32] = {0};
			CHAR_T sessionserver_addr2[32] = {0};
			
			//判断sessionserver是否有两个
			if(strchr(cscfg_save_temp.sessionserver_addr, ';'))
			{
				sscanf(cscfg_save_temp.sessionserver_addr,"%[^;]%*[;]%[^;]",sessionserver_addr1,sessionserver_addr2);
				
				//sessionserver_addr1,即cscfg_save_temp.sessionserver_addr之前已经判断
				if(0 != strlen(sessionserver_addr2)) 
				{
					//sessionserver确实有两个
					//如果portal获取失败则无法更新qamname
					if(0 == portal_save_flag)
					{
						INT32_T qam_name = 0;
						//请求qamname
						if(0 == gwdtv_vod_get_qamname_gy(&qam_name, NGOD_VOD_QAMNAME_TIMEOUT))
						{
							VOD_EXT_WARN(("!!!!!!!!! Get qamname: %d !!!!!!!!!\n", qam_name));
							
							//请求http://portalIP:80/tsids.cfg 区域码配置文件
							if(0 == get_http(cscfg_save_temp.portal_addr, atoi(cscfg_save_temp.portal_port), 
									payloadbuf,	nPayloadBuf, QAMNAME_CFG_NAME)) 
							{
								//判断区域码是否在区间内
								trimspace(payloadbuf);
								VOD_EXT_DBUG(("qam payloadbuf=\n%s\n", payloadbuf));
								
								if(CreateQamNameBiTree(&binary_sort_tree, payloadbuf))
								{
									VOD_EXT_ALWAYS(("binary_sort_tree create success!!!\n"));
									
									#ifdef PRINT_ALWAYS_MSG
									DisplayBST(binary_sort_tree);
									#endif
									
									if(!SearchBST_Key(binary_sort_tree, qam_name))
									{
										VOD_EXT_DBUG(("%d is not in region, use first sessionserver before \";\"!!\n", qam_name));
										strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
										*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
									}
									else
									{
										VOD_EXT_DBUG(("%d is in region, use second sessionserver after \";\"!!\n", qam_name));
										strncpy(sessionserver_addr_save, sessionserver_addr2, sizeof(sessionserver_addr_save));
										*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
									}

									VOD_EXT_ALWAYS(("binary_sort_tree ready to delete!!\n"));
									DeleteBST(&binary_sort_tree);
								}
								else
								{
									VOD_EXT_ERRO(("binary_sort_tree create failed, use first sessionserver before \";\"!!\n"));
									strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
									*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
									status = -1;
								}
							}
							else
							{
								VOD_EXT_ERRO(("get_http failed, use the first sessionserver!!!\n"));
								strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
								*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
								status = -1;
							}
						}
						else
						{
							VOD_EXT_ERRO(("get qamname failed, use first sessionserver before \";\"!!\n"));
							strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
							*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
							status = -1;
						}
					}
					else
					{
						VOD_EXT_ERRO(("PORTAL save failed, cannot get the qamname, use first sessionserver before \";\"!!\n"));
						strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
						*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
						status = -1;
					}
				}
				else //格式错误，只有1个
				{
					//防止后面多出一个";"
					//此种错误可能会出现在在配置文件中删除一个sessionserver的情况
					VOD_EXT_ERRO(("only one sessionserver address, but you have forgot to delete \";\"!!\n"));
					strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
					*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
				}
			}
			else //只有1个
			{
				VOD_EXT_INFO(("only one sessionserver address!!\n"));
				sscanf(cscfg_save_temp.sessionserver_addr,"%[^;]",sessionserver_addr1);
				strncpy(sessionserver_addr_save, sessionserver_addr1, sizeof(sessionserver_addr_save));
				*(sessionserver_addr_save+sizeof(sessionserver_addr_save)-1) = 0;
			}
			
			if(0 != strlen(cscfg_save_temp.sessionserver_port))
			{
				//保存SessionServer的address和port
				memset(sessionserver_save, 0, sizeof(sessionserver_save));
				sprintf(sessionserver_save, "%s:%d", sessionserver_addr_save, atoi(cscfg_save_temp.sessionserver_port));
			}
			else //sessionserverport字段为空，只保存SessionServeraddr
			{
				memset(sessionserver_save, 0, sizeof(sessionserver_save));
				sprintf(sessionserver_save, "%s", sessionserver_addr_save);
			}
			
	        if (-1 == Sys_Prop_Item_save(SYS_PROP_KEY_VODConfig, false, "sessionServer", sessionserver_save))
	        {
				VOD_EXT_ERRO(("SessionServer save failed\n"));
	        	status = -1;
	        }
		}
		
		if ((0 != strlen(cscfg_save_temp.serverid)))
		{
			if(0 != strlen(cscfg_save_temp.serverid_port))
			{
				sprintf(serverid_save, "%s:%d", cscfg_save_temp.serverid, atoi(cscfg_save_temp.serverid_port));
			}
			else //serveridport字段为空，只保存serverid的addr
			{
				sprintf(serverid_save, "%s", cscfg_save_temp.serverid);
			}
			
	        if (-1 == Sys_Prop_Item_save(SYS_PROP_KEY_VODConfig, false, "serverId", serverid_save))
	        {
				VOD_EXT_ERRO(("serverId save failed\n"));
	        	status = -1;
	        }
		}

        if (0 == status) 
        {
			if ((0 != strlen(cscfg_save_temp.cscfg_md5)))
			{
				if (-1 == Sys_Prop_Item_save(SYS_PROP_KEY_VODChannel, false, "MD5", cscfg_save_temp.cscfg_md5))
				{
					VOD_EXT_ERRO(("New MD5 save failed\n"));
					goto FAILED_RETRY;
				}
            }
			VOD_EXT_WARN(("New MD5 save succeed\n"));
			
			rocme_porting_mutex_lock(cscfg_save_ready.mutex);
			if(nready_temp != cscfg_save_ready.nready)
			{
				VOD_EXT_WARN(("new cscfg has come when we were saving old one\n"));
				cscfg_save_ready.nready--;
			}
			else
			{
				VOD_EXT_INFO(("no cscfg come when we were saving old one, "
				"we can empty nready, no matter what is it's value, "
				"because we have saved the newest one!!!\n"));
				cscfg_save_ready.nready = 0;
			}
			rocme_porting_mutex_unlock(cscfg_save_ready.mutex);
			
			time_wait = FAIL_TIME_BEGIN;
			continue;
        }
        
FAILED_RETRY:
		
		//exponential backoff
		if(time_wait >= MAX_TIME_WAIT)
		{
			time_wait = FAIL_TIME_BEGIN;
		}
		time_wait *= 2;
		rocme_porting_task_msleep(time_wait);
	}
	
	free(payloadbuf);
	payloadbuf = NULL;
	
	rocme_porting_mutex_destroy(savecfg_lock);
	savecfg_lock = 0;
	
	rocme_porting_mutex_destroy(cscfg_save_ready.mutex);
	cscfg_save_ready.mutex = 0;
	rocme_porting_cond_destroy(cscfg_save_ready.cond);
	cscfg_save_ready.cond = 0;
	cscfg_save_ready.nready = 0;
	
	VOD_EXT_INFO(("exit %s \n", __FUNCTION__));
}

/*
程序入口函数
*/
void ngod_gw_portal_http_init()
{
    INT32_T h_authtask = 0;

    VOD_EXT_WARN(("%s enter! \n", __FUNCTION__));

    //use msg queue to notify portal_http_task, semaphore is not appropriate
	portal_msg 		= rocme_porting_queue_create(NULL, 10, ROC_TASK_WAIT_FIFO);
	getcfg_lock		= rocme_porting_mutex_create(ROC_MUTEX_TIMED_NP);
	savecfg_lock	= rocme_porting_mutex_create(ROC_MUTEX_TIMED_NP);
	cscfg_save_ready.mutex 	= rocme_porting_mutex_create(ROC_MUTEX_TIMED_NP);
	cscfg_save_ready.cond  	= rocme_porting_cond_create(); 
	cscfg_save_ready.nready = 0;

    //create task to active auth and get portal list
    h_authtask = rocme_porting_task_create("portal_server", portal_server, NULL, ROC_TASK_PRIO_LEVEL_1, 1024*4);
    if(0 == h_authtask) 
    {
        VOD_EXT_ERRO(("create portal_server failed !!!\n"));
    }
    
    h_authtask = rocme_porting_task_create("portal_http", portal_http_task, NULL, ROC_TASK_PRIO_LEVEL_1, 1024*8);
    if(0 == h_authtask) 
    {
        VOD_EXT_ERRO(("create portal_http_task failed !!!\n"));
    }
    
    h_authtask = rocme_porting_task_create("portal_save", portal_save, NULL, ROC_TASK_PRIO_LEVEL_1, 1024*8);
    if(0 == h_authtask) 
    {
        VOD_EXT_ERRO(("create portal_save failed !!!\n"));
    }
    
    VOD_EXT_INFO(("%s leave! \n", __FUNCTION__));
    return;
}

#endif  /*_NGOD_GW_GETCFG_C_*/
