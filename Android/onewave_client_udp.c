#include "onewave_client_udp.h"

/******************************************************************************************************
1、因为服务器缺陷而做出修改的地方是:
服务器返回的直播时间比实际时间慢3分钟
在onewave_vod_response_nfy_play对直播时间的处理上

2、因为页面缺陷而做出修改的地方是，在直播页面中:
onewave_vod_response_nfy_announce中case 2101和case2102中页面响应我们传给它的消息后
未提示任何信息，直接退出到ip直播首页，这里到头后恢复直播正常倍速播放

3、快进快退时爆音，前端服务器并没有去掉音频数据
********************************************************************************************************/

//缺陷控制宏OTHER_BUG_REPAIR
//如果页面或者服务器有缺陷，打开该宏会在VOD这边屏蔽该缺陷；
//当然这是不合理的，关闭该宏VOD撒手不管
#define OTHER_BUG_REPAIR 

static ArrayOnewaveMgr *g_arrayOnewaveMgr = NULL;
static INT32_T g_onewave_port  = 54000; //ip流点播时传给服务器的默认端口，程序运行后
										//会在54001~64000中随机选一个端口作为
										//浏览器生命周期中的唯一端口
										//这个地方用随机端口是因为直接重启不teardown的话
										//如果端口未变而且上次播的是点播流
										//而用户又在超时时间2分钟内立即打开VOD
										//因为是udp方式，服务器并不知道客户端已断开
										//而且此时超时时间还未过
										//则服务器还会把缓存里的剩余流继续推下来
										//造成播放混论
static INT32_T mp_handle = 0;


//随机端口，一个浏览器生命周期中仅用一个端口
//如果该端口已经占用，服务器会找到一个能连接的客户端的端口，一般向上偏移
static UINT32_T GetRand(UINT32_T seed_me, UINT32_T range) 
{
    srandom(time(NULL)+seed_me);
	return 1 + range * (random() / (RAND_MAX + 1.0));
}


static OnewaveMgr *OnewaveVod_getMgr(INT32_T client_index)
{
    if((client_index >= 0) && (client_index < MAX_RTSP_CLIENT_NUM))
    {
        return &g_arrayOnewaveMgr->array_client[client_index];
    }

    return NULL;
}

INT32_T OnewaveVod_Udp_Init( VODProject *project,rtsp_mgr *mgr )
{
    VODProject *me = (VODProject*)project;
    rtsp_client client[1];
	
    FAILED_RETURNX( !me, G_FAILURE );
    memset(client, 0, sizeof(rtsp_client)*1);

    g_arrayOnewaveMgr = (ArrayOnewaveMgr *)calloc(1, sizeof(ArrayOnewaveMgr));
    FAILED_RETURNX( !g_arrayOnewaveMgr, G_FAILURE );
    g_arrayOnewaveMgr->copy_project = me;

    VOD_INFO(("leave %s \n",__FUNCTION__));
    return G_SUCCESS;
}

INT32_T  OnewaveVod_Udp_UnInit( VODProject *project )
{
    vod_struct *me = NULL;
    FAILED_RETURNX( !project, G_FAILURE );
    me = (vod_struct *)project->hProjectIF->handle;
    rtsp_mgr_unregister_client(me->copy_mgr,0);
    CFREE(me);

    return G_SUCCESS;
}

/***********************************************************************************************************
目前咸宁广电的UT服务器支持以下消息命令
Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, GET_PARAMETER
**************************************************************************************************************/

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
//除response之外，其他onewave_vod_describe_xxx都是组包函数，这些函数默认成功
//rtsp_client_main中认为只有ret>0才成功，这里让这7个组包函数全部返回1 ！！！
//sessionid是在setup之后生成的，所以describe和setup消息中都没有这个值
STATIC INT32_T onewave_vod_describe_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me = (OnewaveMgr *)mgr;
    rtsp_request *connect = NULL;
	
    FAILED_RETURNX( !me, G_FAILURE );

    connect = rtsp_mgr_get_request(me->copy_mgr,0);
    FAILED_RETURNX( !connect, G_FAILURE );

    rtsp_request_add_cseq(connect);
    rtsp_request_add_field(connect,"Accept: application/sdp\r\n",strlen("Accept: application/sdp\r\n"));
    rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
    rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_setup_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    rtsp_info  *info 		= NULL;
    rtsp_request *connect 	= NULL;
    INT8_T transport[256] 	= {0};
    INT8_T buf[64] 			= {0};
	INT8_T onewave_addr[32] = {0};
	
    FAILED_RETURNX( !me, G_FAILURE );

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX( !info, G_FAILURE );
	
    connect = rtsp_mgr_get_request(me->copy_mgr,0);
    FAILED_RETURNX( !connect, G_FAILURE );

	//在这里设置是因为rstp_mgr_open时，可能根据url设置成错误的类型
	rtsp_info_set_property(info,RTSP_INFO_PROP_TRANSPORT,0); //ip stream

	//获取地址，这里放空，因为服务器可以自己判断客户端地址
	//我们自己获取，存在获取"ppp0"还是"eth0"哪个网卡的问题
	//如果多个本地网卡，例如"eth0"、"eth1"、"eth2"，更加无法判断
	//让服务器去判断，我们播的时候流已经到我们指定的端口上了
	//直接用0.0.0.0:port就行了
	if(0 == strlen(onewave_addr)) 
	{
		//提供两个端口给服务器，让服务器去选择合适的端口
		//g_onewave_port作为唯一端口，mplayer对多端口支持不好
		sprintf(transport, "Transport: MP2T/RTP/UDP;unicast;client_port=%d", g_onewave_port);
		strcat(transport, ",");
		sprintf(buf, "MP2T/UDP;unicast;client_port=%d", g_onewave_port);
		strcat(transport, buf);
		strcat(transport, "\r\n");
	}
	else
	{
		sprintf(transport, "Transport: MP2T/RTP/UDP;unicast;destination=%s;client_port=%d",
					onewave_addr, g_onewave_port);
		strcat(transport, ",");
		sprintf(buf, "MP2T/UDP;unicast;destination=%s;client_port=%d",
					onewave_addr, g_onewave_port);
		strcat(transport, buf);
		strcat(transport, "\r\n");
	}

	//describe和setup的时候没有sessionid，这个sessionid只是服务器回复的setup响应里面获得的
	//也就是说setup成功了才会生成一个sessionid，标志着打开成功，服务器已经分配资源
    rtsp_request_add_cseq(connect);
    rtsp_request_add_field(connect,transport,strlen(transport));
    rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
    rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_play_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    rtsp_info  *info 		= NULL;
    rtsp_request *connect 	= NULL;
    INT8_T  buffer[256] 	= { 0 };
    TIME_INFO time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));
	
    FAILED_RETURNX( !me || !me->copy_mgr, G_FAILURE );

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX( !info, G_FAILURE );
	
    connect = rtsp_mgr_get_request(me->copy_mgr,0);
    FAILED_RETURNX( !connect, G_FAILURE );
	
    rtsp_request_add_cseq(connect);
    rtsp_request_add_sessionId(connect);

	//request_speed和时间的设置统一到play接口中
	//其他地方只获取，不设置
	//这个地方不必考虑当前速率，这个只是组包程序
	//根据当前环境组包，不进行其他任何判断和消息事件发送
    INT32_T request_speed = 0;
    rtsp_info_get_property(info,RTSP_INFO_PROP_REQUEST_SPEED,&request_speed);
	sprintf(buffer, "Scale: %d.0\r\n", request_speed);
	rtsp_request_add_field(connect,buffer,strlen(buffer));

	//时间的设置不必考虑主动暂停恢复的情况，当然如果考虑应该在play接口中
	//主动暂停恢复的时候，想正常播放、快进快退、选时随便，和正常情况一致
    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
	sprintf(buffer, "Range: %s\r\n",time_info->seektime);
	rtsp_request_add_field(connect, buffer,strlen(buffer));

    rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
    rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));
	
    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者            ：
******************************************************/
//以下close、pause、get_parameter、option都不用发送
//除sessionid、cseq、user-agent之外其他的功能信息，所以他们完全一样
//目前分割成不同的接口是为了以后扩展方便
STATIC INT32_T onewave_vod_close_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me = (OnewaveMgr *)mgr;
    rtsp_request *connect = NULL;
	
    FAILED_RETURNX( !me, G_FAILURE );

    connect = rtsp_mgr_get_request(me->copy_mgr,0);
    FAILED_RETURNX( !connect, G_FAILURE );

    rtsp_request_add_cseq(connect);
    rtsp_request_add_sessionId(connect);
    rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
    rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_pause_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
	OnewaveMgr *me = (OnewaveMgr *)mgr;
	rtsp_request *connect = NULL;
	
	FAILED_RETURNX( !me, G_FAILURE );

	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );

	rtsp_request_add_cseq(connect);
	rtsp_request_add_sessionId(connect);
	rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
	rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}


/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_get_parameter_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
	OnewaveMgr *me = (OnewaveMgr *)mgr;
	rtsp_request *connect = NULL;
	
	FAILED_RETURNX( !me, G_FAILURE );

	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );

	rtsp_request_add_cseq(connect);
	rtsp_request_add_sessionId(connect);
	rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
	rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}


/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_option_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
	OnewaveMgr *me = (OnewaveMgr *)mgr;
	rtsp_request *connect = NULL;
	
	FAILED_RETURNX( !me, G_FAILURE );

	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );

	rtsp_request_add_cseq(connect);
	rtsp_request_add_sessionId(connect);
	rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
	rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //必须大于0。原因见rtsp_client_main.c中对event的处理，如果你不确定，这个地方不能改
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_response_nfy_describe(void *mgr,INT32_T p1,INT32_T p2)
{
    INT32_T ret 			= G_SUCCESS;
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    rtsp_info *info			= NULL;
	rtsp_request *connect 	= NULL;
	INT8_T	*p_field 		= NULL;
	INT8_T	*p 				= NULL;
	INT32_T  i 				= 0;
    TIME_INFO time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));
	
	sdp_info p_sdp[1];
	memset(p_sdp, 0, sizeof(sdp_info));

    FAILED_RETURNX(!me || !me->copy_mgr, G_FAILURE);
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE );
	
	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );
	
	p_field = rtsp_request_get_field(connect, "Connection", ':');
	if(p_field)
	{
		if(0 == strncasecmp(p_field, "close", 5))
		{
			//如果describe/setup响应中带有Connection: Close，机顶盒需要关闭底层TCP连接。
			VOD_INFO(("VOD server close the connection"));
			return G_FAILURE;
		}
	}

	ret = rtsp_info_get_property(info,RTSP_INFO_PROP_SDP_INFO,(void*)&p_sdp);
	for(; G_SUCCESS == ret && i < p_sdp->line_num; ++i)
	{
		switch ( p_sdp->line_tag[i] )
		{
		case 'a':
			// a=range:npt=0-2308.000000
			p_field = strstr((INT8_T*)p_sdp->sdp_line[i], "range:");
			if ( p_field )
			{
				rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
				if ( (p=strstr(p_field, "npt=")) != NULL )
				{
					p += strlen("npt=");
					time_info->start_time->type = TIME_TYPE_NPT;
					time_info->end_time->type = TIME_TYPE_NPT;
					time_info->present_time->type = TIME_TYPE_NPT;
	
					time_info->start_time->u.npt_time = (INT32_T)(atof(p));
	
					if ( (p=strstr(p, "-")) != NULL )
					{
						p += strlen("-");
						time_info->end_time->u.npt_time = (INT32_T)(atof(p));
						time_info->duration = 
							time_info->end_time->u.npt_time 
							- time_info->start_time->u.npt_time;
					}
					else
					{
						time_info->duration = 0; //直播流
					}
					
					if(time_info->duration > 0)
					{
						me->flag_not_live = 1;
					}
					else//终止时间小于开始时间也表示直播流
					{
						time_info->duration = 0; //直播流时把该值都变为0
					}
	
					VOD_DBUG(("clock start time is %d, end time is %d, duration is %d\n", 
						time_info->start_time->u.npt_time, time_info->end_time->u.npt_time, time_info->duration));
				}
				// a=range:clock=20141014T091518.00Z-20141014T091517.00Z
				else if ( (p=strstr(p_field, "clock=")) != NULL )
				{
					p += strlen("clock=");
					time_info->start_time->type = TIME_TYPE_YMDHMS;
					time_info->end_time->type = TIME_TYPE_YMDHMS;
					time_info->present_time->type = TIME_TYPE_YMDHMS;
					
					INT8_T start_tmp[20] = {0};
					INT8_T end_tmp[20] = {0};
					sscanf(p, "%[^-]-%s", start_tmp, end_tmp);
					start_tmp[19] = end_tmp[19] = 0;
					
					string_to_time_fmt(start_tmp,time_info->start_time);
					me->start_T = (UINT32_T)os_mktime(time_info->start_time->u.ymdhms_time);
	
					if (0 != strlen(end_tmp))
					{
						string_to_time_fmt(end_tmp,time_info->end_time);
						me->end_T = (UINT32_T)os_mktime(time_info->end_time->u.ymdhms_time);
						time_info->duration = (INT64_T)me->end_T - (INT64_T)me->start_T;
					}
					else
					{
						time_info->duration = 0; //直播流
					}
	
					if(time_info->duration > 0)
					{
						me->flag_not_live = 1;
					}
					else//终止时间小于开始时间也表示直播流
					{
						time_info->duration = 0; //直播流时把该值都变为0
					}
					
					VOD_DBUG(("clock start time is %u, end time is %u, duration is %d\n", 
						me->start_T, me->end_T, time_info->duration));
				}
				rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)(time_info));
			}
			//其他情况默认直播
			break;
			
		default:
			break;
		}
	}

    return ret;
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_response_nfy_setup(void *mgr,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    INT32_T ret 			= G_SUCCESS;
    rtsp_info *info 		= NULL;
	rtsp_request *connect 	= NULL;
    INT8_T *p_field 		= NULL;
	INT8_T	*p 				= NULL;
	CHAR_T	url[48]  		= {0};
	CHAR_T	port_tmp[16] 	= {0};
	UINT32_T port 			= 0;
	
    FAILED_RETURNX(!me || !me->copy_mgr, G_FAILURE);
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE );
	
	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );
	
	p_field = rtsp_request_get_field(connect, "Connection", ':');
	if(p_field)
	{
		if(0 == strncasecmp(p_field, "close", 5))
		{
			//如果describe/setup响应中带有Connection: Close，机顶盒需要关闭底层TCP连接。
			VOD_INFO(("VOD server close the connection"));
			return G_FAILURE;
		}
	}

	p_field = rtsp_request_get_field(connect, "Session", ':');
	if( p_field )
	{
		rtsp_info_set_property(info,RTSP_INFO_PROP_SESSIONID,(INT32_T)p_field);
	}

	p_field = rtsp_request_get_field(connect, "Transport", ':');
	if( p_field )
	{	
		if(strstr(p_field, "MP2T/RTP/UDP"))//RTP方式，给MP发送rtp://0.0.0.0:port
		{
			p = strstr(p_field, "destination=");
			if(p)
			{
				sscanf(p+strlen("destination="),"%[^;]",url);
				VOD_DBUG(("The url server send is %s\n", url));

				if(strstr(url, ":"))
				{
					sscanf(url,"%*[^:]:%[^;]",port_tmp);
					port = atoi(port_tmp);
				}
			}
			
			//如果服务器发送过来了端口，必须使用服务器的端口
			//因为端口占用或者其他异常情况，服务器可能不会使用客户端传给他的端口
			//而是选择一个新的端口，一般是默认端口向上偏移
			if(0 == port) 
			{
				VOD_WARN(("server does not send port, use default port!!\n"));
				//内部播放只需要知道端口即可
				//流已经到了网卡上，直接使用0.0.0.0
				sprintf(info->mp_url,"rtp://0.0.0.0:%d", g_onewave_port);
			}
			else
			{
				VOD_WARN(("server has sent port!!\n"));
				sprintf(info->mp_url,"rtp://0.0.0.0:%d", port);
			}
			
		}
		else if(strstr(p_field, "MP2T/UDP"))//UDP方式，给MP发送udp://0.0.0.0:port
		{
			p = strstr(p_field, "destination=");
			if(p)
			{
				sscanf(p+strlen("destination="),"%[^;]",url);
				VOD_DBUG(("The url server send is %s\n", url));

				if(strstr(url, ":"))
				{
					sscanf(url,"%*[^:]:%[^;]",port_tmp);
					port = atoi(port_tmp);
				}
			}

			if(0 == port)
			{
				VOD_WARN(("server does not send port, use default port!!\n"));
				//内部播放只需要知道端口即可
				//流已经到了网卡上，直接使用0.0.0.0
				sprintf(info->mp_url,"udp://0.0.0.0:%d", g_onewave_port);
			}
			else
			{
				VOD_WARN(("server has sent port!!\n"));
				sprintf(info->mp_url,"udp://0.0.0.0:%d", port);
			}
			
		}
		else
		{	
			VOD_ERRO(("Transport protocol not support!!\n"));
			ret = G_FAILURE;
		}
	}
	else
	{
		VOD_ERRO(("no Transport come!!\n"));
		ret = G_FAILURE;
	}
	
	//经测试，不发心跳包的情况下，可以持续播放2分钟多一些
	//因为一般把间隔设置为30-40秒， 现在把心跳包的发送间隔设置为40s
	//断网时间长了mplayer无法自动恢复，这个地方设置再小也没用
	//但是可以通过暂停恢复、快进快退、选时恢复，这个是页面的缺陷，VOD无能为力
	rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_OUT,(INT32_T)1000*40);
	rtsp_mgr_send_event(me->copy_mgr,RTSP_EVENT_HEARTEBEAT,0,0);

	return ret;
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_response_nfy_play(void *mgr,INT32_T p1,INT32_T p2)
{
    INT32_T ret 			= G_SUCCESS;
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    rtsp_info *info			= NULL;
	rtsp_request *connect 	= NULL;
	INT8_T	*p_field 		= NULL;
	INT8_T	*p 				= NULL;
    Roc_Queue_Message_t q_msg = {0,0,0,0};
    TIME_INFO time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));
	
    FAILED_RETURNX(!me || !me->copy_mgr, G_FAILURE);
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE);
	
	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE);

	//在这里统一设置当前倍速和当前时间
	//获取当前时间
	p_field = rtsp_request_get_field(connect, "Range", ':');
	if(p_field)
	{
		rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);

		if (time_info->start_time->type == TIME_TYPE_NPT)
		{
			p = strstr(p_field, "npt=");
			if (NULL != p)
			{
				p += strlen("npt=");
				time_info->present_time->u.npt_time = (INT32_T)(atof(p));
				time_info->present_time->last_update_time = time_ms();

				//不用在这里判断当前时间，到头到尾信息在服务器发来的announce消息中判断
			}
			
			VOD_DBUG(("present_time is %u\n", time_info->present_time->u.npt_time));
		}
		else if(time_info->start_time->type == TIME_TYPE_YMDHMS)
		{
			p = strstr(p_field, "clock=");
			if (NULL != p)
			{
				p += strlen("clock=");

				INT8_T present_tmp[20] = {0};
				sscanf(p, "%[^-]-", present_tmp);
				present_tmp[19] = 0;

				if(1 == me->flag_not_live) //不是直播
				{
					string_to_time_fmt(present_tmp,time_info->present_time);
					time_info->present_time->last_update_time = time_ms();
					me->present_T = (UINT32_T)os_mktime(time_info->present_time->u.ymdhms_time);
				}
				else // 0== me->flag_not_live，直播，此时服务器给的总是直播时间
				{
					TIME_FMT live_time[1];
					memset(live_time, 0, sizeof(TIME_FMT));
					live_time->type = TIME_TYPE_YMDHMS;
					
					string_to_time_fmt(present_tmp,live_time);

					me->live_T = (UINT32_T)os_mktime(live_time->u.ymdhms_time);
					
#ifdef OTHER_BUG_REPAIR
					//服务器返回的直播时间比实际时间慢3分钟
					//如果服务器修复该BUG，则OTHER_BUG_REPAIR宏需要关闭
					me->live_T += 3*60;
#endif

					VOD_DBUG(("live_time is %u\n", me->live_T));

					if(0 == time_info->present_time->last_update_time) //第一次进入直播
					{
						VOD_WARN(("we just enter the live first, save the present time as live time!!!\n"));

						//第一次进入，将直播时间赋值给当前时间
						memcpy(time_info->present_time->u.ymdhms_time, live_time->u.ymdhms_time, sizeof(OS_TIME));
						time_info->present_time->last_update_time = time_ms();
						me->present_T = me->live_T;
						
					}
					else
					{
						//因为每次播放要么不是暂停状态，就会先暂停；要么用户主动暂停
						//总之肯定会走到暂停响应里面onewave_vod_response_nfy_pause
						//在那里会更新当前时间，直播的时候就采用那个时间
						//这里更新present_time不太好，统一在response_nfy_pause里面
						
						//判断是否超过直播时间
						me->present_T < me->live_T ? me->present_T : me->live_T;
						memcpy(time_info->present_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&me->present_T), sizeof(OS_TIME));
						time_info->present_time->last_update_time = time_ms();
					}
				}
			}
			VOD_DBUG(("present_time is %u\n", me->present_T));
		}
		//每次set之前先获取最新的time_info，防止某些已保存值被清空
		rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)time_info);
	}

	//获取当前倍速
	INT32_T request_speed = 0;
	INT32_T current_speed = 0;
	rtsp_info_get_property(info,RTSP_INFO_PROP_REQUEST_SPEED,&request_speed);

	p_field= rtsp_request_get_field(connect, "Scale", ':');
	if(p_field)
	{
		current_speed = atoi(p_field);
	}
	else //response has no scale,use request!!
	{
		current_speed = request_speed;
	}
	
	rtsp_info_set_property(info,RTSP_INFO_PROP_CURRENT_SPEED,current_speed);

	VOD_INFO(("current_speed is %d, request_speed is %d\n", current_speed, request_speed));

	//选时、快进快退和正常播放对播放内核的操作是一致的
	//这里比较特殊，是播放的源媒体流变化了，而不是播放模式变化了
	VOD_WARN(("The mp url given to mplayer is %s\n", info->mp_url));
	
#ifdef OTHER_BUG_REPAIR
	//快进播放需要静音，本来这是前端服务器的工作，可惜前端服务器没有处理
	if(1 == me->flag_scale)
	{
		Roc_Audio_Set_Mute(ROC_TRUE); //快进快退设置静音
	}
#endif

	//将是否要重新对mplayer设置url的标志传给rtsp_info_proc，作为参数p1
	//如果该值为1，则表示需要重新设置url
	//选时、快进快退倍率变化、到达边界重新播放会导致这个值变为1
	ret = rtsp_info_proc(info, RTSP_INFO_EVENT_PLAY, me->flag_MpSetUrl, (INT32_T )&mp_handle);

#ifdef OTHER_BUG_REPAIR
	//恢复播放需要取消静音，这个地方有个问题，如果用户主动静音
	//不就恢复了吗，不过目前VOD页面上的静音和播放器的静音不是一个地方
	//页面静音不会被播放器静音或者恢复影响到，页面静音优先级高于播放器
	if(1 != me->flag_scale)
	{
		Roc_Audio_Set_Mute(ROC_FALSE); //其他情况非静音
	}
#endif
	
	VOD_INFO(("after open, mp_handle is %u!\n", mp_handle));

	return ret;
}


/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
STATIC INT32_T onewave_vod_response_nfy_pause(void *mgr,INT32_T p1,INT32_T p2)
{
    INT32_T ret 			= G_SUCCESS;
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    rtsp_info *info			= NULL;
    TIME_INFO time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));
	
    FAILED_RETURNX(!me || !me->copy_mgr, G_FAILURE);
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE );
	
	//暂停的时候必须更新当前时间
	rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
	
	INT32_T speed = 0;
	rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&speed);
	
	if(time_info->present_time->type == TIME_TYPE_NPT)
	{
		//强转原因见Onewave_VOD_GetCurPosition，至于time_ms()的两个值相减是不会有负数的
		//先算的小，后算的大，不可能出现相反的情况
		time_info->present_time->u.npt_time += 
			(INT32_T)speed*((INT32_T)(time_ms() - time_info->present_time->last_update_time)/1000);
		time_info->present_time->last_update_time = time_ms();
	}
	else if(time_info->present_time->type == TIME_TYPE_YMDHMS)
	{
		//直播暂停的时候也要更新当前时间，这个必须强转成INT32_T，而不是UINT32_T
		//因为speed有可能是负的，但是不要担心present_T变成负的，它是从1970开始的秒数
		me->present_T += 
			(INT32_T)speed*((INT32_T)(time_ms() - time_info->present_time->last_update_time)/1000);
		memcpy(time_info->present_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&me->present_T), sizeof(OS_TIME));

		//其实在暂停的时候更新last_update_time是没有用的，对于点播和回看暂停之后再次播放
		//服务器会返回当前时间，那时候我们会更新，并重新覆盖last_update_time
		//直播在play响应返回的时候我们也会更新，last_update_time也会被覆盖
		//获取当前时间的地方，这个地方更新的这个值永远不会用到
		//但是为了符合逻辑，即更新当前时间，必须更新这个值这个规则
		//保留对这个地方last_update_time的更新
		time_info->present_time->last_update_time = time_ms();

		VOD_DBUG(("present_time is %u\n", me->present_T));
	}
		
	//每次对time_info set前，必须先获取
	rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)time_info);

	return ret;
}

/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/
//单独处理announce信息，因为比较多，单列一个函数
//这个和response_nfy_describe、response_nfy_setup、response_nfy_play不一样
//上面那三个是处理服务器返回的信息，并不会向上层返消息
//这个因为信息比较简单，而且返回的消息严重依赖于服务器的消息
//所以给上层的消息在这里一并处理
/*************************************************************************************************
1101 "Playout Cancelled" 录制的播放取消
1102 "Playout Started" 录制的播放开始.
1103 "Playback Stalled" 录制的播放临时不可用
1104 "Playout Resumed" 录制的播放恢复
2101 表示End of Stream；
2102 表示Beginning of Stream；
2103 表示强制退出；
2104 表示定位到当前直播点；
2105 表示该服务未订购，此时流服务器停止播放流见4.6.2；
2400 "Session Expired" Session 的生命期在ticket 检测下到期
2401 "Ticket Expired" Ticket 过期
4401 "Bad File" 请求的文件无法打开
4402 "Missing File" 请求的文件不存在
5201 "Insufficient MDS Bandwidth" MDS 带宽不够
5400 "Server Resource No Longer Available" 资源不可继续使用
5401 表示Downstream failure；
5402 "Client Session Terminated" 客户端被管理员关闭会话
5403 "Server Shutting Down" RTSP server 正在关闭
5500 "Server Error" 未知事件发生
5502 "Internal Server Error" 内部错误导致服务器停止工作
**************************************************************************************************/
STATIC INT32_T onewave_vod_response_nfy_announce(void *mgr,INT32_T p1,INT32_T p2)
{
    INT32_T ret 			= G_SUCCESS;
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    rtsp_info *info			= NULL;
	rtsp_request *connect 	= NULL;
    INT8_T *xnotice			= NULL;
    INT32_T index 			= 0;
    Roc_Queue_Message_t q_msg = {0,0,0,0};
	
    FAILED_RETURNX(!me || !me->copy_mgr, G_FAILURE);
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE );
	
	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );

    xnotice = rtsp_request_get_field(connect, "x-notice", ':');
    if(xnotice)
	{
        index = atoi(xnotice);
        VOD_INFO(("RTSP_ACK_ANNOUNCE_RESPONSE \nx-notice:%s\n",xnotice));
    }
    else
	{
		VOD_INFO(("x-notice not exist!!\n"));
        return G_SUCCESS;
    }

    switch(index)
    {
    case 2101:/*End of Stream*/
		
#ifdef OTHER_BUG_REPAIR
		if(0 == me->flag_not_live)
		{
			//直播流快进到头从当前位置播放，不往页面上返消息
			//因为页面收到消息后会退出当前播放，而且没有任何提示
			//页面缺陷，我们临时补救一下，以后改了页面把这个地方删了就行了
			//返消息是正规流程
			ret = Roc_VOD_Play(me->vhandler, 1, -1);
			break;
		}
#endif
		
		//先主动暂停，但是页面调用Roc_VOD_Play的时候
		//我们的暂停响应可能还未回来，此时必须在Roc_VOD_Pause返回之后
		//即Pause消息发出之后，立马置暂停状态，否则在play的时候还会发一次暂停
		//这样再次发play的时候已经在服务器还未响应的情况下，向服务器发了两条命令
		//所以再次发play命令可能会有问题，此时我们的cseq会有问题
		ret = Roc_VOD_Pause(me->vhandler, ROC_TRUE);
		me->flag_boundary = 1; //我们已经到了流的边界，再次play的时候，不是从now，而是从beginning

		//先设置暂停状态，防止重复暂停
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_PAUSE);

		VOD_INFO(("PLAY to end, send VOD_MSG_PROGRAM_END to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_PROGRAM_END;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
        break;
		
    case 2102:/*Beginning of Stream*/
		
#ifdef OTHER_BUG_REPAIR
		if(0 == me->flag_not_live)
		{
			//直播流快退到尾从当前位置播放，不往页面上返消息
			//如果快退到尾，一般服务器会录制3天以上，32倍速快退
			//480s，即8分钟15s退完，2小时120分钟225s退完，24小时，2700s，即45分钟退完
			// 3天时间135分钟退完，用户花两个多小时快退到头，也真够无聊的了
			//不如直接看回看，也就我们研发调试会这么无聊了:)
			ret = Roc_VOD_Play(me->vhandler, 1, -1);
			break;
		}
#endif
		
		//这个地方保留的原因是，防止页面没有恢复，那么mplayer将没有流
		ret = Roc_VOD_Pause(me->vhandler, ROC_TRUE);
		me->flag_boundary = 1;//我们已经到了流的边界
		
        rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_PAUSE);

		VOD_INFO(("PLAY to start, send VOD_MSG_PROGRAM_BEGIN to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_PROGRAM_BEGIN;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
        break;
		
	case 2103:/*server tell us force to quit*/
	case 2105:/*server stop to push stream, because user has not bought the resource*/
	case 2400:/*Session Expired*/
	case 2401:/*Ticket Expired*/
	case 4401:/*Bad File*/
	case 4402:/*Missing File*/
	case 5201:/*Insufficient MDS Bandwidth*/
	case 5400:/*Server Resource No Longer Available*/
	case 5401:/*Downstream failure*/
	case 5402:/*Client Session Terminated*/
	case 5403:/*Server Shutting Down*/
	case 5500:/*Server Error*/
	case 5502:/*Internal Server Error*/
		ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
		
		VOD_INFO(("Server error, send VOD_MSG_USER_EXCEPTION to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_USER_EXCEPTION;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
		break;
		
    default:
		//下面这几个没处理，不知道具体代表什么含义
		/**********************************************************
		1101 "Playout Cancelled" 录制的播放取消
		1102 "Playout Started" 录制的播放开始.
		1103 "Playback Stalled" 录制的播放临时不可用
		1104 "Playout Resumed" 录制的播放恢复
		2104 表示定位到当前直播点；
		***********************************************************/
        break;
    }
    
    return G_SUCCESS;
}


/******************************************************
函数名称    :
功能        :
输入参数    :

输出参数    :
返回值      :
作者        ：
******************************************************/

//这个函数进行所有的状态设置和像上层返消息
//其他操作出close之外放在上面的各个函数onewave_vod_response_nfy_xxx中
STATIC INT32_T onewave_vod_response_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    INT32_T ret 			= G_SUCCESS;
    rtsp_info *info 		= NULL;
	rtsp_request *connect 	= NULL;
    VOD_STATUS_e pstatus 	= VOD_PLAY_NONE;
	
	//如果某个case用到，最好memset一下
    Roc_Queue_Message_t q_msg = {0,0,0,0};
	
	FAILED_RETURNX( !me || !me->copy_mgr,G_FAILURE );
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE );
	
	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );

    switch( msg )
    {
		case RTSP_ACK_ANNOUNCE_RESPONSE: // 有announce头的announce信息
		case RTSP_ACK_SOCKET_RECV_ERR:	//rtsp命令接收错误，万一服务器给的错误消息是announce信息
		{
			VOD_INFO(("Enter RTSP_ACK_ANNOUNCE_RESPONSE case \n"));
			ret = onewave_vod_response_nfy_announce(me, p1, p2);
		}
		break;
				
    //两个心跳包，这里有个记录心跳包次数的机制
    //发三次心跳服务器无响应，就不再发送了，本来是想这个情况下自动发送DESCRIBE重新连接的
    //每次收到前端服务器发来的任意消息后，包括心跳包的回应消息，都会重置心跳包次数
    //也就是说如果心跳包的接收和发送正常，则会一直把心跳包次数置0，持续下去
    //这个把GET_PARAMETER作为默认心跳包，是因为如果设置的心跳命令服务器不支持的话
    //而此时没有默认心跳包的情况下，超时3次时，心跳包就不进行了，连接会断开
	case RTSP_ACK_GET_PARAMETER: 
		VOD_INFO(("++++++++GET_PARAMETER ACK+++++++\n"));
		break;
		
	case RTSP_ACK_OPTION:
		VOD_INFO(("++++++++OPTION ACK+++++++\n"));
		break;
		
    case RTSP_ACK_OPEN:
	{
        VOD_INFO(("Enter RTSP_ACK_OPEN case \n"));
        ret = rtsp_mgr_send_event(me->copy_mgr,RTSP_EVENT_DESCRIBE,0,0);
		
		if(ret != G_SUCCESS)
		{
			//这里调Roc_VOD_Close，而不是Roc_VOD_Stop
			ret = Roc_VOD_Close(me->vhandler, ROC_FALSE); //出错的时候close mplayer，清空他的handle
			rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
			
			VOD_INFO(("DESCRIBE failed, send VOD_MSG_OPEN_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_OPEN_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
    }
    break;
		
    case RTSP_ACK_DESCRIBE:
	{
		VOD_INFO(("Enter RTSP_ACK_DESCRIBE case \n"));
		
		ret = onewave_vod_response_nfy_describe(me, p1, p2);
		if(G_SUCCESS != ret)
		{
			ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
			rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
			
			VOD_INFO(("DESCRIBE failed, send VOD_MSG_OPEN_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_OPEN_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_OPEN);
		
		VOD_WARN(("ready to setup!!! not_live flag is %d, 0 for live!!\n", me->flag_not_live));
		ret = rtsp_mgr_send_event(me->copy_mgr,RTSP_EVENT_SETUP,0,0);
		
		if(ret != G_SUCCESS)
		{
			ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
			rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
			
			VOD_INFO(("SETUP failed, send VOD_MSG_OPEN_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_OPEN_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
    }
    break;
		
    case RTSP_ACK_SETUP:
	{
		VOD_INFO(("Enter RTSP_ACK_SETUP case \n"));
		
		ret = onewave_vod_response_nfy_setup(me, p1, p2);
		if(G_SUCCESS != ret)
		{
			ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
			rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
			
			VOD_INFO(("SETUP failed, send VOD_MSG_OPEN_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_OPEN_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_READY);
		
		VOD_INFO(("SETUP success, send VOD_MSG_OPEN_SUCCESS to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_OPEN_SUCCESS;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
    }
    break;
		
    case RTSP_ACK_PLAY:
	{
		VOD_INFO(("Enter RTSP_ACK_PLAY case \n"));
		
		ret = onewave_vod_response_nfy_play(me, p1, p2);
		
		INT32_T current_speed = 0;
		rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&current_speed);

		if(G_SUCCESS != ret)
		{
			if(1 == me->flag_seek)
			{
				VOD_INFO(("Seek failed, send VOD_MSG_SEEK_FAILED to ngbplayer \n"));
				memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
				q_msg.q1stWordOfMsg = VOD_MSG_SEEK_FAILED;
				q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
				vod_msg_event_send(&q_msg);
				break;
			}
			
			if(1 == me->flag_scale)
			{
				VOD_INFO(("SetScale failed, send VOD_MSG_SET_SCALE_FAILED to ngbplayer \n"));
				memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
				q_msg.q1stWordOfMsg = VOD_MSG_SET_SCALE_FAILED;
				q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
				vod_msg_event_send(&q_msg);
				break;
			}
			
			VOD_INFO(("PLAY failed, send VOD_MSG_PLAY_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_PLAY_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break; //play失败不用关闭该实例，可能下次播放会成功
		}
		
		//状态判断
		if (current_speed > 1)
		{
			pstatus = VOD_PLAY_FORWARD;
		}
		else if (current_speed < 0)
		{
			pstatus = VOD_PLAY_BACKWARD;
		}
		else //if(current_speed == 1)
		{
			pstatus = VOD_PLAY_NORMAL;
		}
		
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,pstatus);
		
		//区分选时、快进快退的成功消息
		if(1 == me->flag_seek)
		{
			VOD_INFO(("Seek success, send VOD_MSG_SEEK_SUCCESS to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_SEEK_SUCCESS;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
		if(1 == me->flag_scale)
		{
			VOD_INFO(("SetScale success, send VOD_MSG_SET_SCALE_SUCCESS to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_SET_SCALE_SUCCESS;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
		VOD_INFO(("PLAY success, send VOD_MSG_PLAY_SUCCESS to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_PLAY_SUCCESS;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
    }
    break;
		
    case RTSP_ACK_PAUSE: //pause不必进行失败就关闭的操作，就算失败也没什么关系
    {
        VOD_INFO(("Enter RTSP_ACK_PAUSE case \n"));
		
		ret = onewave_vod_response_nfy_pause(me, p1, p2);
		if(G_SUCCESS != ret)
		{
			VOD_INFO(("PAUSE failed, send VOD_MSG_PAUSE_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_PAUSE_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
        rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_PAUSE);
		
		VOD_INFO(("PAUSE success, send VOD_MSG_PAUSE_SUCCESS to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_PAUSE_SUCCESS;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
    }
    break;
		
    case RTSP_ACK_CLOSE:
    {
        VOD_INFO(("Enter RTSP_ACK_CLOSE case \n"));
		
      	INT32_T index = 0;

		//unregister
        index = vod_ctrl_index(me->vhandler);
        VOD_INFO(("index=0x%x, is_using=%d, me->vhandler=0x%x \n", index, me->is_using, me->vhandler));
			
		if((1 == me->is_using) && (0 != me->vhandler))
        {
			rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_STOP);
			
			VOD_INFO(("RELEASE success, send VOD_MSG_RELEASE_SUCCESS to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_RELEASE_SUCCESS;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			
            rtsp_mgr_unregister_client(me->copy_mgr,0);
            rtsp_mgr_delete(me->copy_mgr, index);
            VOD_INFO(("memset array_client[%d] to zero \n", index));
            memset(&g_arrayOnewaveMgr->array_client[index], 0, sizeof(OnewaveMgr));
			
			ret = vod_ctrl_handle_clr(index);
			if(0 != ret)
			{
				VOD_ERRO(("call vod_ctrl_handle_clr failed !\n"));
			}
			break;
        }

		//上面if未执行就会失败，执行就会成功
		VOD_INFO(("RELEASE failed, send VOD_MSG_RELEASE_FAILED to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_RELEASE_FAILED;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
    }
    break;
		
	case RTSP_ACK_REDIRECT:
	{
		INT8_T *p_field = NULL; 
		p_field = rtsp_request_get_field(connect, "Location", ':');
		if(p_field)
		{
			VOD_INFO(("onewave redirect, url = %s \n",p_field));
	
			INT8_T *url = NULL;
			rtsp_info_get_property(info,RTSP_INFO_PROP_RTSP_URL,&url);
	
			memset(url, 0, RTSP_URL_LEN);
			strncpy(url, p_field, RTSP_URL_LEN);
			*(url + RTSP_URL_LEN - 1) = 0; 
	
			ret = rtsp_mgr_send_event(me->copy_mgr,RTSP_EVENT_OPEN,VOD_TYPE_VOD,(INT32_T)url);
			
			if(ret != G_SUCCESS)
			{
				ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
				rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
				
				VOD_INFO(("REDIRECT failed, send VOD_MSG_OPEN_FAILED to ngbplayer \n"));
				memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
				q_msg.q1stWordOfMsg = VOD_MSG_OPEN_FAILED;
				q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
				vod_msg_event_send(&q_msg);
				break;
			}
		}
	}
	break;
	
	//下面这些是socket接收失败的消息，见rtsp_request_reponse函数
	case RTSP_ACK_EXCEPTION:	//这个消息代表本地错误，需要关闭rtsp连接
	{
		ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
		
		VOD_INFO(("Local error, send VOD_MSG_USER_EXCEPTION to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_USER_EXCEPTION;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
	}
	break;
		
   	case RTSP_ACK_ERROR_DESCRIBE_RESPONSE:
	case RTSP_ACK_ERROR_SETUP_RESPONSE:
    {
		ret = Roc_VOD_Close(me->vhandler, ROC_FALSE);
		rtsp_info_set_property(info,RTSP_INFO_PROP_STATUS,VOD_PLAY_IDLE);
		
		VOD_INFO(("DESCRIBE/SETUP error, send VOD_MSG_OPEN_FAILED to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
        q_msg.q1stWordOfMsg = VOD_MSG_OPEN_FAILED;
        q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
        vod_msg_event_send(&q_msg);
    }
    break;

    case RTSP_ACK_ERROR_PLAY_RESPONSE:
    {
		//区分选时、快进快退的失败消息
		if(1 == me->flag_seek)
		{
			VOD_INFO(("Seek failed, send VOD_MSG_SEEK_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_SEEK_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
		if(1 == me->flag_scale)
		{
			VOD_INFO(("SetScale failed, send VOD_MSG_SET_SCALE_FAILED to ngbplayer \n"));
			memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
			q_msg.q1stWordOfMsg = VOD_MSG_SET_SCALE_FAILED;
			q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
			vod_msg_event_send(&q_msg);
			break;
		}
		
		VOD_INFO(("PLAY failed, send VOD_MSG_PLAY_FAILED to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
		q_msg.q1stWordOfMsg = VOD_MSG_PLAY_FAILED;
		q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
		vod_msg_event_send(&q_msg);
		//play失败不用关闭该实例，可能下次播放会成功
    }
    break;

    case RTSP_ACK_ERROR_PAUSE_RESPONSE:
    {
		VOD_INFO(("PAUSE failed, send VOD_MSG_PAUSE_FAILED to ngbplayer \n"));
		memset(&q_msg, 0, sizeof(Roc_Queue_Message_t));
        q_msg.q1stWordOfMsg = VOD_MSG_PAUSE_FAILED;
        q_msg.q4thWordOfMsg = (UINT32_T)me->vhandler;
        vod_msg_event_send(&q_msg);
    }
    break;

	//其他消息不用处理，目前上层无法识别这些消息，我们也没办法处理
    default:
		/***************
		RTSP_ACK_SET_PARAMETER,  				//UT没有SET_PARAMETER和PING方法
		RTSP_ACK_ERROR_SETPARAMETER_RESPONSE,
		RTSP_ACK_PING,
        		RTSP_ACK_ERROR_PING_RESPONSE,
		RTSP_ACK_ERROR_RESPONSE,				//示例错误，没用过
        		RTSP_ACK_ERROR_STOP_RESPONSE,			//teardown错误，无法做更多
		RTSP_ACK_CON_ERROR,					//不知道干什么用的，没用过
		RTSP_ACK_SOCKET_SEND_ERR,				//rtsp命令send错误，无法做更多
		****************/
        break;
    }

    return ret;
}

/**
** @brief
**  打开Onewave VOD客户端。
**
** @param[in]    url             要播放的url地址，以rtsp://开头
** @param[out]   vodHandle       VOD客户端控制句柄
**
** @retval 0  成功
** @retval -1 失败
*/
//在内部调用的时候需要使用Roc_VOD_xxx，而不是Onewave_VOD_xxx
STATIC INT32_T Onewave_VOD_Open(INT8_T *url, INT32_T *vodHandle)
{
    OnewaveMgr *me 	= NULL;
    INT32_T  ret 	= 0;
    INT32_T index 	= 0;
    rtsp_client client[1];
	memset(client, 0, sizeof(rtsp_client));
	
    VOD_INFO(("%s Enter\n",__FUNCTION__));
	
    FAILED_RETURNX(!url,-1);
    FAILED_RETURNX(!vodHandle,-1);
    FAILED_RETURNX(!g_arrayOnewaveMgr, -1);
	
    VOD_INFO(("%s(line%d):url=%s \n",__FUNCTION__,__LINE__,url));

    //生成VOD句柄
    if( -1 == (index=vod_ctrl_handle(vodHandle)) )
    {
        VOD_ERRO(("too many client! index=%d\n", index));
        return -1;
    }

    ret = vod_ctrl_handle_set(index, *vodHandle);
    if(0 != ret)
    {
        VOD_ERRO(("vod_ctrl_handle_set failed! \n"));
        return -1;
    }
	
	VOD_DBUG(("vod handle is 0x%x!\n", *vodHandle));

	if(54000 == g_onewave_port) //只有当退出浏览器再打开的时候才会重置为54000，其他在54001~64000中选一个
	{
		g_onewave_port += GetRand((UINT32_T)vodHandle, 10000);
		VOD_DBUG(("the port we will use in whole browser life is %d!\n", g_onewave_port));
	}

    memset(&g_arrayOnewaveMgr->array_client[index], 0, sizeof(OnewaveMgr));
    g_arrayOnewaveMgr->array_client[index].is_using  = 1;
    g_arrayOnewaveMgr->array_client[index].vhandler  = *vodHandle;
    g_arrayOnewaveMgr->array_client[index].ihandler  = index;
    g_arrayOnewaveMgr->array_client[index].copy_mgr  = rtsp_mgr_new(NULL, index);
    g_arrayOnewaveMgr->array_client[index].copy_mgr->client_id = index;           //关联实例内部ID到rtsp_mgr

    client->handle             = (void *)&g_arrayOnewaveMgr->array_client[index];//handle是下面各个xxx_nfy的第一个参数mgr
    client->heart_event        = RTSP_EVENT_GET_PARAMETER;
    client->default_port       = 554;
    client->rtsp_describe      = onewave_vod_describe_nfy;
    client->rtsp_setup         = onewave_vod_setup_nfy;
    client->rtsp_play          = onewave_vod_play_nfy;
    client->rtsp_pause         = onewave_vod_pause_nfy;
    client->rtsp_close         = onewave_vod_close_nfy;
    client->rtsp_get_parameter = onewave_vod_get_parameter_nfy;
    client->rtsp_option        = onewave_vod_option_nfy;
    client->rtsp_response      = onewave_vod_response_nfy;

    rtsp_mgr_register_client(g_arrayOnewaveMgr->array_client[index].copy_mgr,client);

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me, -1);

	//打开类型为VOD_TYPE_VOD的管理实例
    ret = rtsp_mgr_open(me->copy_mgr,VOD_TYPE_VOD,url,0,*vodHandle);
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  关闭一个VOD客户端。
**
** @param[in]    vodHandle       VOD客户端控制句柄。
** @param[in]    closeMode       关闭模式, ROC_TRUE:关闭时保留最后一帧; ROC_FALSE:关闭时不保留最后一帧。
**
** @retval 0  成功
** @retval -1 失败
*/
STATIC INT32_T Onewave_VOD_Close(INT32_T vodHandle, ROC_BOOL closeMode)
{
    OnewaveMgr *me = NULL;
    INT32_T ret    = 0;
    INT32_T status = 0;
    INT32_T index  = 0;
	
    VOD_INFO(("%s Enter\n",__FUNCTION__));

    VOD_INFO(("%s(line%d):closeMode=%d \n",__FUNCTION__,__LINE__,closeMode));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));
	
#ifdef OTHER_BUG_REPAIR
	//退出前取消静音
	Roc_Audio_Set_Mute(ROC_FALSE);
#endif

	if(!g_arrayOnewaveMgr)//这里不使用FAILED_RETURNX，我们要在退出前关掉mplayer
	{
		VOD_ASSERT(1); 
		ret = -1;
		goto Destory_Mplayer;
	}

    //检查VOD实例是否已经关闭
    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error!\n"));
		ret = -1;
		goto Destory_Mplayer;
    }

    if (0 == g_arrayOnewaveMgr->array_client[index].vhandler)
    {
        VOD_INFO(("already closed!\n"));
        ret = 0; 				//已经关闭，返回关闭成功
		goto Destory_Mplayer; //强制销毁还存在的mplayer
    }

    me = OnewaveVod_getMgr(index);
	if(!me || !me->copy_mgr)//这里不使用FAILED_RETURNX，我们要在退出前关掉mplayer
	{
		VOD_ASSERT(1); 
		ret = -1;
		goto Destory_Mplayer;
	}

    if(closeMode)
	{
		rtsp_info_set_property(me->copy_mgr->client_info,RTSP_INFO_PROP_LAST_FRAME,ROC_VID_STOP_WITH_LAST_PIC);
	}
	else
	{
		rtsp_info_set_property(me->copy_mgr->client_info,RTSP_INFO_PROP_LAST_FRAME,ROC_VID_STOP_WITH_BLACK_PIC);
	}
	
	rtsp_info_get_property(me->copy_mgr->client_info,RTSP_INFO_PROP_STATUS,&status);
	VOD_INFO(("%s:current status is %d \n",__FUNCTION__,status));
    if(status < VOD_PLAY_OPEN)//这个地方不用强行销毁mplayer，因为还未打开
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_CLOSE,0,0);
    }
    else //close时传入0，销毁mplayer；stop时不销毁
    {
        ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_CLOSE,0,0);

		if(G_FAILURE == ret) //rtsp_mgr_proc_event可能在FAILED_RETURNX( !me ,ret )返回失败，此时需要强制关闭mplayer
		{					//其他失败情况mplayer肯定已经关闭了，这里不做区分处理了
			VOD_ERRO(("rtsp_mgr_proc_event RTSP_EVENT_CLOSE failed!\n"));
			goto Destory_Mplayer; //mplayer已经关闭的情况下，再次关闭会报ROC_MP_ERR_INVALID_HANDLE错误，但不影响流程
		}
    }
	
	//mplayer销毁后，将onewave使用的静态handle置0
	//在这里使用，而不在上面那个else中
	//是因为close之后无论出现什么异常状况都要将静态handle置0
	//否则下一次播放肯定会失败，mplayer会提示ROC_MP_ERR_INVALID_HANDLE
	mp_handle = 0;
	
Destory_Mplayer: 

	if(0 != mp_handle) //在详情页面这里总会走到，VOD的实例已经关闭，但是mplayer还在，把它关了
	{
		//如果是正常流程，此时在上面mp_handle已经置为0了
		//if(0 != mp_handle)仅在上面mp_handle = 0未执行时强制关闭mplayer
		//切不可把这个地方挪到mp_handle = 0前面，因为正常关闭流程是在RTSP_EVENT_CLOSE里面做的
		//仅在失败的情况下重新关闭，成功关闭之后再次关闭没什么好处，可能会带来问题
		INT32_T mp_ret = Roc_MP_Close(mp_handle);
		if(0 != mp_ret)
		{
			VOD_ERRO(("Roc_MP_Close failed! mp_ret=%d\n",mp_ret));
			//这里不做处理，关闭失败也没什么办法
		}
		else
		{
			VOD_INFO(("Roc_MP_Close success!\n"));
		}
		mp_handle = 0; //重置为0，因为这个handle已不再可用
	}
	
	//close时重置全局端口为默认端口
	g_onewave_port = 54000;
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  VOD播放接口。
**
** @param[in]    vodHandle       VOD客户端控制句柄。
** @param[in]    scale           播放倍速。scale=1:正常播放; scale<0:快退; scale>1:快进; 0<scale<1:慢进。
** @param[in]    npt             播放时间偏移，单位为秒
**                               npt>=0:从开始位置偏移npt时间后播放;
**                               npt<0:从当前位置播放;
**
** @retval 0  成功
** @retval -1 失败
*/
//将所有的选时和倍速参数设置放在play函数中，本来应该放在ioctl中，这个功能还未实现
//到头到尾发送消息放在response_play中
STATIC INT32_T Onewave_VOD_Play(INT32_T vodHandle, INT32_T scale, INT32_T npt)
{
    OnewaveMgr *me           = NULL;
    INT32_T     ret          = 0;
    INT32_T     status       = 0;
    INT32_T     index        = 0;
    rtsp_info  *info         = NULL;
	TIME_INFO time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));
	
    VOD_INFO(("%s Enter\n",__FUNCTION__));

    VOD_INFO(("%s(line%d):scale=%d,npt=%d \n",__FUNCTION__,__LINE__,scale,npt));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);

	//设置为UDP流，这里是因为open的时候可能会根据url设置成错误的协议类型
	//这里设置可以在rtsp_info_proc中执行正确的播放模式
    rtsp_info_set_property(info,RTSP_INFO_PROP_PROTOCOLS,VOD_PROTOCOL_IP_TS);

    rtsp_info_get_property(info,RTSP_INFO_PROP_STATUS,&status);
    VOD_INFO(("Roc_VOD_Play: status is %d\n",status ));
	
    if(status < VOD_PLAY_READY)       //播放状态在正常以下，调用play接口肯定是返回失败的
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_ERROR_PLAY_RESPONSE,0,0);
        return -1;
    }
	
    //这里每次进入play先把seek和scale标志置0，如果npt>=0会置1，标志选时或者快进快退操作
    //为了方便维护，不要在这个函数之外的任何地方赋值！！！
    //将对mplayer重新设置url的标志位重新置0
    me->flag_MpSetUrl = 0;
	me->flag_seek = 0;
	me->flag_scale = 0;
	
	if(npt >= 0)
	{
		me->flag_seek = 1;
	}
	if(scale != 1)
	{
		me->flag_scale = 1;
	}
	
	//如果该值为1，则表示需要重新设置url
	//选时、到达边界重新播放会导致这个值变为1
	//快进快退不再重新设置url，因为这个会导致mplayer播放时延加大
	if(me->flag_seek || me->flag_boundary)
	{
		me->flag_MpSetUrl = 1;
	}
	
	/*每次重新播放，如果不是暂停状态，就先发暂停*/
	//第一次播放，这个状态肯定是VOD_PLAY_READY，不用特殊处理第一次
	//这里是为了服务器不暂停发流，直接操作正在发送的流可能有问题
	//这个地方会在rtsp包发送完成返回
	//此时会继续执行play，两个包发送时间时间不会超过1s
	//我们收到响应包肯定也是有时延的，不必再主动堵塞时间
	if(status >= VOD_PLAY_NORMAL && status != VOD_PLAY_PAUSE)
	{
		ret = Roc_VOD_Pause(vodHandle, ROC_TRUE); //暂停失败存在发出命令失败和响应失败，对于后者，不能从返回值判断
	}
	
    //处理时间
	rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);

	//这个地方比较奇葩，直播快退恢复的时候，服务器维护的当前流的位置是快退到的位置
	//但是返回的时间确实是直播时间，不是快退回去的时间
	//所以直播直接用"npt=now-"就可以，服务器维护的媒体流的当前位置是对的
	//直播不支持选时，如果要选时看直播，直接用回看就行
	
	if(0 == me->flag_not_live) //直播从当前位置播放，不支持选时操作，但支持快进快退
	{
		//注意，直播不支持beginning、end、now字段
		//缺省位置，对于直播节目为媒体服务器当前接收的媒体流位置
		sprintf(time_info->seektime, "npt=now-");
	}
	else
	{
		//小于0代表从当前位置开始播放，该时间为服务器维护
		/************************************************************************************
		这里UT的规范提及:
		刚开始播放时，play的range字段，NTP时间使用beginning-、now-、beginning-end或now-end
		暂停、快退或快进之后再正常播放时，play的range字段，NTP时间使用now-或now-end
		统一起来能用的只有now-和now-end，这两个是一样
		到达边界恢复播放，最好不要使用beginning-、beginning-end，直接传入时间点
		************************************************************************************/
	    if(npt < 0 && 0 == me->flag_boundary) //没有到达边界从当前位置开始播 
	    {
			VOD_WARN(("will play from now!!!\n"));
			sprintf(time_info->seektime, "npt=now-");
	    }
	    else //大于等于0，表示从偏移时间npt开始播，等于0即表示最开头
	    {
	    	UINT32_T current_T;
			if(time_info->start_time->type == TIME_TYPE_YMDHMS)
			{
				//注意对时间超过时长的判断
				//此时页面传入的npt是相对于1970年至今的秒数，不是偏移时间
				//os_mktime出来的值是GMT+8时区的值
				//os_gmtime需要的也是GMT+8时区的值
				//有点晕? 就是这样的，进出的都是GMT+0的clock字符串，比如20141208T012600.00Z
				//GMT+0的clock-->os_mktime-->GMT+8对应的秒数(UTC)-->os_gmtime-->GMT+0的clock
				//所以os_mktime、os_gmtime是自统一的，考虑了本地时区
				//time.h中的mktime、gmtime中所用的tm结构体中也可以指定本地时区
				//如果指定了，则是本地时区的UTC时间，未指定则是GMT+0的UTC时间
				
				if(1 == me->flag_boundary) //包括npt < 0 && 1 == me->flag_boundary的情况，再往下不再有npt<0的情况
				{
					VOD_WARN(("Beginning or End of Stream Reached,"
						" and someone call me to play again, will play form beginning!!!\n"));

					current_T = me->start_T + 1; //到达边界恢复后从最开始后1s开始播
					me->flag_boundary = 0; //重新置该标志为0，如果是暂停、快进快退还是从当前位置播
				}
				else
				{
					current_T = (UINT32_T)npt; 	//上层给我们的值也调整为GMT+8时区的值，不用再加8小时
				}

				VOD_DBUG(("clock start time is %u, end time is %u, current_T is %u\n", 
					me->start_T, me->end_T, current_T));
				
				if(current_T <= me->start_T)
				{
					current_T = me->start_T + 1; //seek到头时，从视频头部后1秒处播放
				}
				else if(current_T >= me->end_T)
				{
					current_T = me->end_T - 3; //seek到尾时，从视频尾部前3秒处播放
				}

				VOD_DBUG(("current_T - me->start_T = %u", current_T - me->start_T));
				
				TIME_FMT current_time[1];
				memset(current_time, 0, sizeof(TIME_FMT));
				current_time->type = TIME_TYPE_YMDHMS;

				//由于服务器采用的UTC时间，需要使用os_gmtime，而不是os_localtime
				memcpy(current_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&current_T), sizeof(OS_TIME));
				
				INT8_T current_time_string[20] = {0};
				ret = time_fmt_to_string(current_time, current_time_string);
				current_time_string[19] = 0;

				//此时time_fmt_to_string获得的字符串类型于20141207T230800
				//按照UT规范，应该是20141207T230800.00Z，年月日T时分秒.毫秒Z的格式
				sprintf(time_info->seektime, "clock=%s.00Z-", current_time_string);
				
			}
			else if(time_info->start_time->type == TIME_TYPE_NPT)
			{
				if(1 == me->flag_boundary)
				{
					VOD_WARN(("Beginning or End of Stream Reached,"
						" and someone call me to play again, will play form beginning!!!\n"));

					current_T = time_info->start_time->u.npt_time + 1; //到达边界恢复后从最开始后1s开始播
					me->flag_boundary = 0; //重新置该标志为0，如果是暂停、快进快退还是从当前位置播
				}
				else
				{
					//此时页面传入的是相对于片头的时间
					current_T = time_info->start_time->u.npt_time + npt;
				}
				
				VOD_DBUG(("clock start time is %u, end time is %u, current_T is %u\n", 
					time_info->start_time->u.npt_time, time_info->end_time->u.npt_time, current_T));

				//注意npt==0不是代表从当前位置开始播，而是表示从最开始播
				//这个地方小于的情况其实不存在，是为了统一和上面的写法
				if(current_T <= time_info->start_time->u.npt_time) 
				{
					current_T = time_info->start_time->u.npt_time + 1;//seek到头时，从视频头部后1秒处播放
				}
 				else if(current_T >= time_info->end_time->u.npt_time)
				{
					current_T = time_info->end_time->u.npt_time - 3;//seek到尾时，从视频尾部前3秒播放
				}
				
				sprintf(time_info->seektime, "npt=%u-", current_T);
			}
			else
			{
				VOD_WARN(("time type not support, seek will not work\n"));
				sprintf(time_info->seektime, "npt="); //不带参数，缺省位置，由服务器决定
			}
	    }
	}
	//保存时间设置
	rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)time_info);
	
	//直播也可以快退了，这里只是保存倍速，倍速的处理在setscale函数中
	rtsp_info_set_property(info,RTSP_INFO_PROP_REQUEST_SPEED,scale);
	
	ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_PLAY,scale,npt);
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  VOD播放接口。
**
** @param[in]    vodHandle       VOD客户端控制句柄。
** @param[in]    stopMode        停止模式, ROC_TRUE:停止时保留最后一帧; ROC_FALSE:停止时不保留最后一帧。
**
** @retval 0  成功
** @retval -1 失败
*/

/******
重要说明:VOD的流程是open、play、close，一般播放器的流程是open、play、stop、close
这里把stop当成close来使用，这样用没什么问题?肯定是有的，但是目前也只能这么用，如下:
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

因为VOD只有setup(open)、play、teardown（close）命令，而没有stop对应的命令。
这个地方我们就是添加了，stop也不能实现什么功能。
VOD的播放功能，就是发送流控命令，接收流控命令的过程
这个stop问题是rtsp协议本身的问题
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

那这里就要注意了:在stop变成close的过程中，一个很重要的问题就是调用播放内核的顺序
这里在close中调用mp_close之前先调用mp_stop。这里不在ack_event_close中调用mp_stop和mp_close的原因是:
如果因为网络问题还是服务器问题，导致没有收到反馈，那么我们会一直占用mplayer的资源
会导致下次播放失败，这个地方必须同步调用。
或者弄一个复杂的方式，等待接收，接收超时强制调用
但这会增加系统的复杂度和多项目代码同步问题，需要评估
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

当然，我们也不是在open、ack_event_open或者play时中调用的mp_open
而是在ack_event_play收到服务器play流控命令的反馈时才调用的mp_open和mp_play，才是服务器已经开始推流
这个问题的原因是我们只有发送play命令之后，前端才能给我们发流，有流mp_open才能成功
这个问题是mplayer本身实现的问题，或者是底层ffmpeg的问题、或者是硬件解码层的问题
这个地方可能也有没有流却占用系统资源的考虑，底层的实现也是经过深入思考的
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

一般播放器的流程应该是:
1、open，申请系统资源，初始化播放线程
2、play，获取媒体流，进行解复用、解码，这一步在mp_open里面做了，有问题的
3、stop，停止解复用解码，停止获取媒体流
4、close，退出播放线程，释放系统资源
5、ioctl，控制播放线程，和其他操作，这个不影响流程
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

rtsp协议是这么做的:
1、describe、setup，获取服务器媒体流信息，发送一个端口号申请服务器资源，相当于open
2、play，通知服务器开始推流，这个和正常play一样的
3、stop，无对应命令
4、teardown，通知服务器停止推流，服务器那边销毁该客户端对应的资源。。。
这里应该分成两个命令，而在play的时候通知服务器选择不同资源，可能有困难。。
这样的话每次不用销毁该客户端占用的端口等系统资源
5、其他流控命令，相当于ioctl

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
这个地方和close区分开来是因为在stop中不销毁mplayer，频繁销毁会有问题
只有当close的时候才销毁
*****/

STATIC INT32_T Onewave_VOD_Stop(INT32_T vodHandle, ROC_BOOL stopMode)
{
    OnewaveMgr *me = NULL;
    INT32_T ret    = 0;
    INT32_T status = 0;
    INT32_T index  = 0;
	
    VOD_INFO(("%s Enter\n",__FUNCTION__));

    VOD_INFO(("%s(line%d):closeMode=%d \n",__FUNCTION__,__LINE__,stopMode));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));
	
#ifdef OTHER_BUG_REPAIR
	//退出前取消静音
	Roc_Audio_Set_Mute(ROC_FALSE);
#endif

    FAILED_RETURNX(!g_arrayOnewaveMgr, -1);

    //检查VOD实例是否已经关闭
    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    if (0 == g_arrayOnewaveMgr->array_client[index].vhandler)
    {
        VOD_INFO(("already closed! \n"));
        return 0;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    if(stopMode)
	{
		rtsp_info_set_property(me->copy_mgr->client_info,RTSP_INFO_PROP_LAST_FRAME,ROC_VID_STOP_WITH_LAST_PIC);
	}
	else
	{
		rtsp_info_set_property(me->copy_mgr->client_info,RTSP_INFO_PROP_LAST_FRAME,ROC_VID_STOP_WITH_BLACK_PIC);
	}
	
	rtsp_info_get_property(me->copy_mgr->client_info,RTSP_INFO_PROP_STATUS,&status);
	VOD_INFO(("%s:current status is %d \n",__FUNCTION__,status));
    if(status < VOD_PLAY_OPEN)
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_CLOSE,0,0);
    }
    else //上面与close函数均一致，不同的地方在这里，此时把onewave维护的mp_handle传给rtsp_mgr_proc_event一个参数，让其销毁
    {
    	//只有当传入的参数和rtsp_mgr维护的一致时，才不会销毁
    	//实现stop不销毁mplayer，close时销毁，又不影响原有功能
        ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_CLOSE,0,(INT32_T )&mp_handle);
		VOD_INFO(("after close, mp_handle is %u!\n", mp_handle));
    }
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  VOD播放接口。
**
** @param[in]    vodHandle       VOD客户端控制句柄。
** @param[in]    pauseMode       暂停模式, ROC_TRUE:暂停时保留最后一帧; ROC_FALSE:暂停时不保留最后一帧。
**
** @retval 0  成功
** @retval -1 失败
*/
STATIC INT32_T Onewave_VOD_Pause(INT32_T vodHandle, ROC_BOOL pauseMode)
{
    OnewaveMgr *me      = NULL;
    INT32_T     ret     = 0;
    INT32_T     index   = 0;
    INT32_T     status  = 0;
    rtsp_info  *info    = NULL;

    VOD_INFO(("%s(line%d):pauseMode=%d, onewave udp only support ROC_TRUE, as keep last pic\n",
				__FUNCTION__,__LINE__,pauseMode));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);

    rtsp_info_get_property(info, RTSP_INFO_PROP_STATUS,&status);
    if(status < VOD_PLAY_NORMAL) //还没开始播放，不能暂停，直播可以暂停
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_ERROR_PAUSE_RESPONSE,0,0);
        return 0;
    }

    ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_PAUSE,0,0);

    return ret;
}

//设置播放倍率
STATIC INT32_T Onewave_VOD_SetScale(INT32_T vodHandle, INT32_T scale)
{
    OnewaveMgr *me        = NULL;
    INT32_T     ret       = 0;
    INT32_T     index     = 0;
    INT32_T     status    = 0;
    rtsp_info  *info      = NULL;

    VOD_INFO(("%s(line%d):scale=%d \n",__FUNCTION__,__LINE__,scale));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);
	
    rtsp_info_get_property(info, RTSP_INFO_PROP_STATUS,&status);
    if(status < VOD_PLAY_NORMAL) //还没开始播放，不能快进快退，但是直播可以快进快退
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_ERROR_PLAY_RESPONSE,0,0);
        return 0;
    }
	
	//设置要求的scale，但是UT支持1.0、2.0、4.0、8.0、16.0、32.0；-2.0、-4.0、-8.0、-16.0 、-32.0
	if(scale > 32)					scale = 32;
	if(scale < -32) 				scale = -32;
	if(0 == scale || -1 == scale)	scale = 1;
	if(scale>2 && scale<4)			scale = 2;
	if(scale>4 && scale<8)			scale = 4;
	if(scale>8 && scale<16) 		scale = 8;
	if(scale>16 && scale<32)		scale = 16;
	if(scale>-4 && scale<-2)		scale = -2;
	if(scale>-8 && scale<-4)		scale = -4;
	if(scale>-16 && scale<-8)		scale = -8;
	if(scale>-32 && scale<-16)		scale = -16;
	
	//-1表示从当前位置开始快进快退
    ret = Roc_VOD_Play(vodHandle, scale, -1);

    return ret;
}

//获取播放倍率
STATIC INT32_T Onewave_VOD_GetScale(INT32_T vodHandle, INT32_T *scale)
{
    OnewaveMgr *me        = NULL;
    INT32_T     index     = 0;
    INT32_T     status    = 0;
    INT32_T     scale_tmp = 0;
    rtsp_info  *info      = NULL;

    VOD_INFO(("%s(line%d):Enter! \n",__FUNCTION__,__LINE__));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    if(NULL == scale)
    {
        VOD_ERRO(("scale=NULL! \n"));
        return -1;
    }

    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);
	
    rtsp_info_get_property(info, RTSP_INFO_PROP_STATUS,&status);
    if(status < VOD_PLAY_NORMAL) //还没开始播放不能获取当前倍速，但是也可以快进快退
    {
        return -1;
    }

	//直播流也可以设置倍速
    rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&scale_tmp);

    *scale = scale_tmp;

    return 0;
}

//获取片源时长 单位：毫秒
STATIC INT32_T Onewave_VOD_GetDuration(INT32_T vodHandle, INT64_T *duration)
{
    OnewaveMgr *me           = NULL;
    INT32_T     index        = 0;
    INT32_T     status  	 = 0;
    rtsp_info  *info         = NULL;
    TIME_INFO   time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));

    VOD_DBUG(("%s(line%d):Enter! \n",__FUNCTION__,__LINE__));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    if(NULL == duration)
    {
        VOD_ERRO(("duration=NULL! \n"));
        return -1;
    }

    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);
	
    rtsp_info_get_property(info, RTSP_INFO_PROP_STATUS,&status);
	
	//还未打开或者是直播流，不能获取当前时长
    if(status < VOD_PLAY_OPEN || 0 == me->flag_not_live) 
    {
        VOD_ERRO(("vod is not open, or we are in live state!!!\n"));
        return -1;
    }

    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);

	*duration = time_info->duration < 0 ? 0 : 1000 * time_info->duration;
    VOD_INFO(("leave %s, duration=%lld ms \n",__FUNCTION__,*duration));

    return 0;
}


//获取当前播放时间
//onewave不支持UPDATE_TIME命令
STATIC INT32_T Onewave_VOD_GetCurPosition(INT32_T vodHandle, INT64_T *cur_position)
{
    OnewaveMgr *me           = NULL;
    INT32_T     ret          = 0;
    INT32_T     index        = 0;
    INT64_T     tmp          = 0; 
    rtsp_info  *info         = NULL;
    INT32_T     status       = 0;
    INT32_T     speed        = 0;
    TIME_INFO   time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));

    VOD_DBUG(("%s(line%d):Enter! \n",__FUNCTION__,__LINE__));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    if(NULL == cur_position)
    {
        VOD_ERRO(("cur_position=NULL! \n"));
        return -1;
    }

    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);
	
    rtsp_info_get_property(info, RTSP_INFO_PROP_STATUS,&status);

	//还未播放，不能获取当前位置，直播流也可以获取当前位置
    if(status < VOD_PLAY_NORMAL)
    {
        VOD_ERRO(("vod is not open!!!\n"));
        return -1;
    }
	
    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
    rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&speed);

	if(time_info->present_time->type == TIME_TYPE_NPT)
	{
		//TIME_TYPE_NPT返回的是已播放时间，是相对于片头的毫秒数
		if(status != VOD_PLAY_PAUSE)
	    {
	    	//这里有必要解释一下，为什么强转，以及在运算之前就需要强转的原因
	    	//首先gcc/g++对于UINT和INT的四则运算的结果是保存在一个UINT的临时变量中的
	    	//如果在运算结果出来之后再强转可能会溢出
	    	//这是因为如果算出来的是负值，它会保存在一个UINT型中
	    	//它内存中是以补码的形式存在的，变为UINT，最高位符号位也将变为值的一部分
	    	//如果结果是-3，就会变成2^32-3，是补码对应的无符号值
			tmp = (INT64_T)time_info->present_time->u.npt_time 
			 	   + (INT64_T)speed*(INT64_T)((time_ms() - time_info->present_time->last_update_time)/1000)
			 	   - (INT64_T)time_info->start_time->u.npt_time;
	    }
	    else //暂停时，必须在pause response中更新当前时间
	    {
	    	tmp = (INT64_T)time_info->present_time->u.npt_time - (INT64_T)time_info->start_time->u.npt_time;
	    }

		//npt需要判断是否超过起止点
		tmp = tmp < 0 ? 0 : tmp;
        tmp = tmp < (INT64_T)time_info->start_time->u.npt_time ? (INT64_T)time_info->start_time->u.npt_time : tmp;
        tmp = tmp > (INT64_T)time_info->end_time->u.npt_time ? (INT64_T)time_info->end_time->u.npt_time : tmp;
	}	
	else if(time_info->present_time->type == TIME_TYPE_YMDHMS)
	{
		//TIME_TYPE_YMDHMS返回的是绝对时间 ，表示1970年至今的秒数(UTC)
		//注意是GMT+8的UTC时间
		if(status != VOD_PLAY_PAUSE)
	    {
			tmp = (INT64_T)me->present_T 
					+ (INT64_T)speed*(INT64_T)((time_ms() - time_info->present_time->last_update_time)/1000);
		}
	    else //暂停时，必须在pause response中更新当前时间
	    {
	    	tmp = (INT64_T)me->present_T;
	    }

		//回看、直播不必判断终止点，流是连续的，可能会超过左右边界
		
		TIME_FMT tmp_time[1];
		memset(tmp_time, 0, sizeof(TIME_FMT));
		tmp_time->type = TIME_TYPE_YMDHMS;

		//os_gmtime算出来的是GMT+0标准时间的struct tm，转化成clock字符串后类似于20141208T012600.00Z
		memcpy(tmp_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&tmp), sizeof(OS_TIME));
		
		INT8_T tmp_time_string[20] = {0};
		ret = time_fmt_to_string(tmp_time, tmp_time_string);
		tmp_time_string[19] = 0;
    	VOD_DBUG(("hold UTC should be %s.00Z\n", tmp_time_string));
	}
	
	//必须等先把tmp转化为INT64_T
	//如果tmp是int型，直接用tmp*1000，得到的是一个int型
	//可能会溢出，溢出之后再转为long long 还是负值
	*cur_position = 1000 * (INT64_T)tmp;
	
    VOD_INFO(("leave %s, cur_position=%lld ms\n", __FUNCTION__, *cur_position));

    return ret;
}


//获取当前播放器信息
//本应该是static，可是临时方案，需要开放给上层调用
//把声明放在vod_onewave_client.h中
INT32_T Onewave_VOD_GetPlayerInfo(INT32_T vodHandle, void* data)
{
    OnewaveMgr *me           = NULL;
    INT32_T     ret          = 0;
    INT32_T     index        = 0;
    rtsp_info  *info         = NULL;
    INT32_T     status       = 0;
    INT32_T     speed        = 0;
    TIME_INFO   time_info[1];
	memset(time_info, 0, sizeof(TIME_INFO));

    VOD_DBUG(("%s(line%d):Enter! \n",__FUNCTION__,__LINE__));
	VOD_DBUG(("vod handle is 0x%x!\n", vodHandle));

    if(NULL == data)
    {
        VOD_ERRO(("playerinfo=NULL! \n"));
        return -1;
    }

	Roc_MP_PlayerInfo_t *playerinfo;
	playerinfo = (Roc_MP_PlayerInfo_t*)data;
	memset(playerinfo, 0, sizeof(Roc_MP_PlayerInfo_t));
		
    index = vod_ctrl_index(vodHandle);
    if(-1 == index )
    {
        VOD_ERRO(("vodHandle error! \n"));
        return -1;
    }

    me = OnewaveVod_getMgr(index);
    FAILED_RETURNX(!me || !me->copy_mgr, -1);

    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, -1);
	
    rtsp_info_get_property(info, RTSP_INFO_PROP_STATUS,&status);
	switch(status)
	{
		case VOD_PLAY_IDLE:
			playerinfo->State = ROC_MP_STATE_DEINIT;
			break;
		case VOD_PLAY_CLOSED:
		case VOD_PLAY_STOP:
			playerinfo->State = ROC_MP_STATE_STOP;
			break;
		case VOD_PLAY_OPEN:
			playerinfo->State = ROC_MP_STATE_PREPARING;
			break;
		case VOD_PLAY_READY:
			playerinfo->State = ROC_MP_STATE_PREPARED;
			break;
		case VOD_PLAY_NORMAL:
		case VOD_PLAY_BACKWARD:
		case VOD_PLAY_FORWARD:
			playerinfo->State = ROC_MP_STATE_PLAY;
			break;
		case VOD_PLAY_PAUSE:
			playerinfo->State = ROC_MP_STATE_PAUSE;
			break;
		default:
			if(me->flag_seek)
			{
				playerinfo->State = ROC_MP_STATE_SEEKING;
			}
			break;
	}

    if(status < VOD_PLAY_NORMAL) 
    {
        VOD_INFO(("vod is not play!!!\n"));
        return 0; //返回0，是因为还未播放，获取当前状态就可以了
    }

	ret = Roc_VOD_GetScale(vodHandle, &speed);
	if(0 != ret)
	{
		VOD_ERRO(("Roc_VOD_GetScale failed \n"));
		return ret;
	}
	
    switch(speed)
    {
        case 1:
            playerinfo->Speed = ROC_MP_SPEED_NORMAL;
            break;
        case 2:
            playerinfo->Speed = ROC_MP_SPEED_FF2X;
            break;
        case 4:
            playerinfo->Speed = ROC_MP_SPEED_FF4X;
            break;
        case 8:
            playerinfo->Speed = ROC_MP_SPEED_FF8X;
            break;
        case 16:
            playerinfo->Speed = ROC_MP_SPEED_FF16X;
            break;
        case 32:
            playerinfo->Speed = ROC_MP_SPEED_FF32X;
            break;
        case -2:
            playerinfo->Speed = ROC_MP_SPEED_BF2X;
            break;
        case -4:
            playerinfo->Speed = ROC_MP_SPEED_BF4X;
            break;
        case -8:
            playerinfo->Speed = ROC_MP_SPEED_BF8X;
            break;
        case -16:
            playerinfo->Speed = ROC_MP_SPEED_BF16X;
            break;
        case -32:
            playerinfo->Speed = ROC_MP_SPEED_BF32X;
            break;
        default:
            playerinfo->Speed = ROC_MP_SPEED_NORMAL;
            break;
    }
	
	ret = Roc_VOD_GetCurPosition(vodHandle, (INT64_T*)&playerinfo->TimePlayedInMS);
	if(0 != ret)
	{
		VOD_ERRO(("Roc_VOD_GetDuration failed \n"));
		return ret;
	}
	
	//直播流不能获取播放百分比
    if(0 == me->flag_not_live) 
    {
        VOD_INFO(("vod is in live state!!!\n"));
        return 0; //返回0，是因为对于直播获取上面这些信息就是成功了
    }

    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);

	double progress = 0.0;

	//npt获取的当前位置是已播放时间
	if(time_info->present_time->type == TIME_TYPE_NPT)
	{
		if(0 == time_info->duration)
		{
			VOD_ERRO(("time_info duration is 0 \n"));
		}
		else
		{
			progress = (double)(playerinfo->TimePlayedInMS/1000)/(double)(time_info->duration);
		}
	}
	
	//clock获取的当前位置是UTC时间
	if(time_info->present_time->type == TIME_TYPE_YMDHMS)
	{
		if(0 == time_info->duration)
		{
			VOD_ERRO(("time_info duration is 0, vod is in live, MUST not reach here!!!!\n"));
		}
		else
		{
			progress = ((double)(playerinfo->TimePlayedInMS/1000) - (double)(me->start_T))/(double)(time_info->duration);
		}
	}

	playerinfo->Progress = (UINT32_T)(100*progress);

	//这里打印出来的值请到对应的结构体里
	//playerinfo->Speed=0代表正常速度,因为ROC_MP_SPEED_NORMAL = 0
    VOD_INFO(("leave %s\n, playerinfo->State is %u, playerinfo->Speed is %u\n" 
			"playerinfo->TimePlayedInMS/1000 is %u sec, playerinfo->Progress is %u%%\n",
		__FUNCTION__, playerinfo->State, playerinfo->Speed, 
					playerinfo->TimePlayedInMS/1000, playerinfo->Progress));

    return ret;
}



//onewave VOD客户端控制句柄
static vod_app_ctrl_t g_onewave_app_ctrl =
{
    Onewave_VOD_Open,
    Onewave_VOD_Close,
    Onewave_VOD_Play,
    Onewave_VOD_Stop,
    Onewave_VOD_Pause,
    Onewave_VOD_SetScale,
    Onewave_VOD_GetScale,
    Onewave_VOD_GetDuration,
    Onewave_VOD_GetCurPosition,
    NULL,
    NULL,
    NULL
};

/*******************************************************************************
获取vod app播放上下文
参数:
    vod_app_ctrl VOD APP控制句柄。
返回值:
    大于等于0成功，小于0失败。
*******************************************************************************/
INT32_T onewave_udp_ctrl_handle(vod_app_ctrl_t **vod_app_ctrl)
{
    *vod_app_ctrl = &g_onewave_app_ctrl;
    VOD_INFO(("vod app type is onewave! \n"));
    return 0;
}

