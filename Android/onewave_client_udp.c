#include "onewave_client_udp.h"

/******************************************************************************************************
1����Ϊ������ȱ�ݶ������޸ĵĵط���:
���������ص�ֱ��ʱ���ʵ��ʱ����3����
��onewave_vod_response_nfy_play��ֱ��ʱ��Ĵ�����

2����Ϊҳ��ȱ�ݶ������޸ĵĵط��ǣ���ֱ��ҳ����:
onewave_vod_response_nfy_announce��case 2101��case2102��ҳ����Ӧ���Ǵ���������Ϣ��
δ��ʾ�κ���Ϣ��ֱ���˳���ipֱ����ҳ�����ﵽͷ��ָ�ֱ���������ٲ���

3���������ʱ������ǰ�˷�������û��ȥ����Ƶ����
********************************************************************************************************/

//ȱ�ݿ��ƺ�OTHER_BUG_REPAIR
//���ҳ����߷�������ȱ�ݣ��򿪸ú����VOD������θ�ȱ�ݣ�
//��Ȼ���ǲ�����ģ��رոú�VOD���ֲ���
#define OTHER_BUG_REPAIR 

static ArrayOnewaveMgr *g_arrayOnewaveMgr = NULL;
static INT32_T g_onewave_port  = 54000; //ip���㲥ʱ������������Ĭ�϶˿ڣ��������к�
										//����54001~64000�����ѡһ���˿���Ϊ
										//��������������е�Ψһ�˿�
										//����ط�������˿�����Ϊֱ��������teardown�Ļ�
										//����˿�δ������ϴβ����ǵ㲥��
										//���û����ڳ�ʱʱ��2������������VOD
										//��Ϊ��udp��ʽ������������֪���ͻ����ѶϿ�
										//���Ҵ�ʱ��ʱʱ�仹δ��
										//�����������ѻ������ʣ��������������
										//��ɲ��Ż���
static INT32_T mp_handle = 0;


//����˿ڣ�һ����������������н���һ���˿�
//����ö˿��Ѿ�ռ�ã����������ҵ�һ�������ӵĿͻ��˵Ķ˿ڣ�һ������ƫ��
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
Ŀǰ��������UT������֧��������Ϣ����
Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, GET_PARAMETER
**************************************************************************************************************/

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
******************************************************/
//��response֮�⣬����onewave_vod_describe_xxx���������������Щ����Ĭ�ϳɹ�
//rtsp_client_main����Ϊֻ��ret>0�ųɹ�����������7���������ȫ������1 ������
//sessionid����setup֮�����ɵģ�����describe��setup��Ϣ�ж�û�����ֵ
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

    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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

	//��������������Ϊrstp_mgr_openʱ�����ܸ���url���óɴ��������
	rtsp_info_set_property(info,RTSP_INFO_PROP_TRANSPORT,0); //ip stream

	//��ȡ��ַ������ſգ���Ϊ�����������Լ��жϿͻ��˵�ַ
	//�����Լ���ȡ�����ڻ�ȡ"ppp0"����"eth0"�ĸ�����������
	//��������������������"eth0"��"eth1"��"eth2"�������޷��ж�
	//�÷�����ȥ�жϣ����ǲ���ʱ�����Ѿ�������ָ���Ķ˿�����
	//ֱ����0.0.0.0:port������
	if(0 == strlen(onewave_addr)) 
	{
		//�ṩ�����˿ڸ����������÷�����ȥѡ����ʵĶ˿�
		//g_onewave_port��ΪΨһ�˿ڣ�mplayer�Զ�˿�֧�ֲ���
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

	//describe��setup��ʱ��û��sessionid�����sessionidֻ�Ƿ������ظ���setup��Ӧ�����õ�
	//Ҳ����˵setup�ɹ��˲Ż�����һ��sessionid����־�Ŵ򿪳ɹ����������Ѿ�������Դ
    rtsp_request_add_cseq(connect);
    rtsp_request_add_field(connect,transport,strlen(transport));
    rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
    rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));

    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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

	//request_speed��ʱ�������ͳһ��play�ӿ���
	//�����ط�ֻ��ȡ��������
	//����ط����ؿ��ǵ�ǰ���ʣ����ֻ���������
	//���ݵ�ǰ��������������������κ��жϺ���Ϣ�¼�����
    INT32_T request_speed = 0;
    rtsp_info_get_property(info,RTSP_INFO_PROP_REQUEST_SPEED,&request_speed);
	sprintf(buffer, "Scale: %d.0\r\n", request_speed);
	rtsp_request_add_field(connect,buffer,strlen(buffer));

	//ʱ������ò��ؿ���������ͣ�ָ����������Ȼ�������Ӧ����play�ӿ���
	//������ͣ�ָ���ʱ�����������š�������ˡ�ѡʱ��㣬���������һ��
    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
	sprintf(buffer, "Range: %s\r\n",time_info->seektime);
	rtsp_request_add_field(connect, buffer,strlen(buffer));

    rtsp_request_add_field(connect,ONEWAVE_USER_AGENT,strlen(ONEWAVE_USER_AGENT));
    rtsp_request_add_field(connect,END_STRING,strlen(END_STRING));
	
    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����            ��
******************************************************/
//����close��pause��get_parameter��option�����÷���
//��sessionid��cseq��user-agent֮�������Ĺ�����Ϣ������������ȫһ��
//Ŀǰ�ָ�ɲ�ͬ�Ľӿ���Ϊ���Ժ���չ����
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

    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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

    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}


/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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

    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}


/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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

    return 1; //�������0��ԭ���rtsp_client_main.c�ж�event�Ĵ�������㲻ȷ��������ط����ܸ�
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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
			//���describe/setup��Ӧ�д���Connection: Close����������Ҫ�رյײ�TCP���ӡ�
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
						time_info->duration = 0; //ֱ����
					}
					
					if(time_info->duration > 0)
					{
						me->flag_not_live = 1;
					}
					else//��ֹʱ��С�ڿ�ʼʱ��Ҳ��ʾֱ����
					{
						time_info->duration = 0; //ֱ����ʱ�Ѹ�ֵ����Ϊ0
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
						time_info->duration = 0; //ֱ����
					}
	
					if(time_info->duration > 0)
					{
						me->flag_not_live = 1;
					}
					else//��ֹʱ��С�ڿ�ʼʱ��Ҳ��ʾֱ����
					{
						time_info->duration = 0; //ֱ����ʱ�Ѹ�ֵ����Ϊ0
					}
					
					VOD_DBUG(("clock start time is %u, end time is %u, duration is %d\n", 
						me->start_T, me->end_T, time_info->duration));
				}
				rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)(time_info));
			}
			//�������Ĭ��ֱ��
			break;
			
		default:
			break;
		}
	}

    return ret;
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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
			//���describe/setup��Ӧ�д���Connection: Close����������Ҫ�رյײ�TCP���ӡ�
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
		if(strstr(p_field, "MP2T/RTP/UDP"))//RTP��ʽ����MP����rtp://0.0.0.0:port
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
			
			//������������͹����˶˿ڣ�����ʹ�÷������Ķ˿�
			//��Ϊ�˿�ռ�û��������쳣��������������ܲ���ʹ�ÿͻ��˴������Ķ˿�
			//����ѡ��һ���µĶ˿ڣ�һ����Ĭ�϶˿�����ƫ��
			if(0 == port) 
			{
				VOD_WARN(("server does not send port, use default port!!\n"));
				//�ڲ�����ֻ��Ҫ֪���˿ڼ���
				//���Ѿ����������ϣ�ֱ��ʹ��0.0.0.0
				sprintf(info->mp_url,"rtp://0.0.0.0:%d", g_onewave_port);
			}
			else
			{
				VOD_WARN(("server has sent port!!\n"));
				sprintf(info->mp_url,"rtp://0.0.0.0:%d", port);
			}
			
		}
		else if(strstr(p_field, "MP2T/UDP"))//UDP��ʽ����MP����udp://0.0.0.0:port
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
				//�ڲ�����ֻ��Ҫ֪���˿ڼ���
				//���Ѿ����������ϣ�ֱ��ʹ��0.0.0.0
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
	
	//�����ԣ�����������������£����Գ�������2���Ӷ�һЩ
	//��Ϊһ��Ѽ������Ϊ30-40�룬 ���ڰ��������ķ��ͼ������Ϊ40s
	//����ʱ�䳤��mplayer�޷��Զ��ָ�������ط�������СҲû��
	//���ǿ���ͨ����ͣ�ָ���������ˡ�ѡʱ�ָ��������ҳ���ȱ�ݣ�VOD����Ϊ��
	rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_OUT,(INT32_T)1000*40);
	rtsp_mgr_send_event(me->copy_mgr,RTSP_EVENT_HEARTEBEAT,0,0);

	return ret;
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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

	//������ͳһ���õ�ǰ���ٺ͵�ǰʱ��
	//��ȡ��ǰʱ��
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

				//�����������жϵ�ǰʱ�䣬��ͷ��β��Ϣ�ڷ�����������announce��Ϣ���ж�
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

				if(1 == me->flag_not_live) //����ֱ��
				{
					string_to_time_fmt(present_tmp,time_info->present_time);
					time_info->present_time->last_update_time = time_ms();
					me->present_T = (UINT32_T)os_mktime(time_info->present_time->u.ymdhms_time);
				}
				else // 0== me->flag_not_live��ֱ������ʱ��������������ֱ��ʱ��
				{
					TIME_FMT live_time[1];
					memset(live_time, 0, sizeof(TIME_FMT));
					live_time->type = TIME_TYPE_YMDHMS;
					
					string_to_time_fmt(present_tmp,live_time);

					me->live_T = (UINT32_T)os_mktime(live_time->u.ymdhms_time);
					
#ifdef OTHER_BUG_REPAIR
					//���������ص�ֱ��ʱ���ʵ��ʱ����3����
					//����������޸���BUG����OTHER_BUG_REPAIR����Ҫ�ر�
					me->live_T += 3*60;
#endif

					VOD_DBUG(("live_time is %u\n", me->live_T));

					if(0 == time_info->present_time->last_update_time) //��һ�ν���ֱ��
					{
						VOD_WARN(("we just enter the live first, save the present time as live time!!!\n"));

						//��һ�ν��룬��ֱ��ʱ�丳ֵ����ǰʱ��
						memcpy(time_info->present_time->u.ymdhms_time, live_time->u.ymdhms_time, sizeof(OS_TIME));
						time_info->present_time->last_update_time = time_ms();
						me->present_T = me->live_T;
						
					}
					else
					{
						//��Ϊÿ�β���Ҫô������ͣ״̬���ͻ�����ͣ��Ҫô�û�������ͣ
						//��֮�϶����ߵ���ͣ��Ӧ����onewave_vod_response_nfy_pause
						//���������µ�ǰʱ�䣬ֱ����ʱ��Ͳ����Ǹ�ʱ��
						//�������present_time��̫�ã�ͳһ��response_nfy_pause����
						
						//�ж��Ƿ񳬹�ֱ��ʱ��
						me->present_T < me->live_T ? me->present_T : me->live_T;
						memcpy(time_info->present_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&me->present_T), sizeof(OS_TIME));
						time_info->present_time->last_update_time = time_ms();
					}
				}
			}
			VOD_DBUG(("present_time is %u\n", me->present_T));
		}
		//ÿ��set֮ǰ�Ȼ�ȡ���µ�time_info����ֹĳЩ�ѱ���ֵ�����
		rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)time_info);
	}

	//��ȡ��ǰ����
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

	//ѡʱ��������˺��������ŶԲ����ں˵Ĳ�����һ�µ�
	//����Ƚ����⣬�ǲ��ŵ�Դý�����仯�ˣ������ǲ���ģʽ�仯��
	VOD_WARN(("The mp url given to mplayer is %s\n", info->mp_url));
	
#ifdef OTHER_BUG_REPAIR
	//���������Ҫ��������������ǰ�˷������Ĺ�������ϧǰ�˷�����û�д���
	if(1 == me->flag_scale)
	{
		Roc_Audio_Set_Mute(ROC_TRUE); //����������þ���
	}
#endif

	//���Ƿ�Ҫ���¶�mplayer����url�ı�־����rtsp_info_proc����Ϊ����p1
	//�����ֵΪ1�����ʾ��Ҫ��������url
	//ѡʱ��������˱��ʱ仯������߽����²��Żᵼ�����ֵ��Ϊ1
	ret = rtsp_info_proc(info, RTSP_INFO_EVENT_PLAY, me->flag_MpSetUrl, (INT32_T )&mp_handle);

#ifdef OTHER_BUG_REPAIR
	//�ָ�������Ҫȡ������������ط��и����⣬����û���������
	//���ͻָ����𣬲���ĿǰVODҳ���ϵľ����Ͳ������ľ�������һ���ط�
	//ҳ�澲�����ᱻ�������������߻ָ�Ӱ�쵽��ҳ�澲�����ȼ����ڲ�����
	if(1 != me->flag_scale)
	{
		Roc_Audio_Set_Mute(ROC_FALSE); //��������Ǿ���
	}
#endif
	
	VOD_INFO(("after open, mp_handle is %u!\n", mp_handle));

	return ret;
}


/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
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
	
	//��ͣ��ʱ�������µ�ǰʱ��
	rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
	
	INT32_T speed = 0;
	rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&speed);
	
	if(time_info->present_time->type == TIME_TYPE_NPT)
	{
		//ǿתԭ���Onewave_VOD_GetCurPosition������time_ms()������ֵ����ǲ����и�����
		//�����С������Ĵ󣬲����ܳ����෴�����
		time_info->present_time->u.npt_time += 
			(INT32_T)speed*((INT32_T)(time_ms() - time_info->present_time->last_update_time)/1000);
		time_info->present_time->last_update_time = time_ms();
	}
	else if(time_info->present_time->type == TIME_TYPE_YMDHMS)
	{
		//ֱ����ͣ��ʱ��ҲҪ���µ�ǰʱ�䣬�������ǿת��INT32_T��������UINT32_T
		//��Ϊspeed�п����Ǹ��ģ����ǲ�Ҫ����present_T��ɸ��ģ����Ǵ�1970��ʼ������
		me->present_T += 
			(INT32_T)speed*((INT32_T)(time_ms() - time_info->present_time->last_update_time)/1000);
		memcpy(time_info->present_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&me->present_T), sizeof(OS_TIME));

		//��ʵ����ͣ��ʱ�����last_update_time��û���õģ����ڵ㲥�ͻؿ���֮ͣ���ٴβ���
		//�������᷵�ص�ǰʱ�䣬��ʱ�����ǻ���£������¸���last_update_time
		//ֱ����play��Ӧ���ص�ʱ������Ҳ����£�last_update_timeҲ�ᱻ����
		//��ȡ��ǰʱ��ĵط�������ط����µ����ֵ��Զ�����õ�
		//����Ϊ�˷����߼��������µ�ǰʱ�䣬����������ֵ�������
		//����������ط�last_update_time�ĸ���
		time_info->present_time->last_update_time = time_ms();

		VOD_DBUG(("present_time is %u\n", me->present_T));
	}
		
	//ÿ�ζ�time_info setǰ�������Ȼ�ȡ
	rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)time_info);

	return ret;
}

/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
******************************************************/
//��������announce��Ϣ����Ϊ�Ƚ϶࣬����һ������
//�����response_nfy_describe��response_nfy_setup��response_nfy_play��һ��
//�����������Ǵ�����������ص���Ϣ�����������ϲ㷵��Ϣ
//�����Ϊ��Ϣ�Ƚϼ򵥣����ҷ��ص���Ϣ���������ڷ���������Ϣ
//���Ը��ϲ����Ϣ������һ������
/*************************************************************************************************
1101 "Playout Cancelled" ¼�ƵĲ���ȡ��
1102 "Playout Started" ¼�ƵĲ��ſ�ʼ.
1103 "Playback Stalled" ¼�ƵĲ�����ʱ������
1104 "Playout Resumed" ¼�ƵĲ��Żָ�
2101 ��ʾEnd of Stream��
2102 ��ʾBeginning of Stream��
2103 ��ʾǿ���˳���
2104 ��ʾ��λ����ǰֱ���㣻
2105 ��ʾ�÷���δ��������ʱ��������ֹͣ��������4.6.2��
2400 "Session Expired" Session ����������ticket ����µ���
2401 "Ticket Expired" Ticket ����
4401 "Bad File" ������ļ��޷���
4402 "Missing File" ������ļ�������
5201 "Insufficient MDS Bandwidth" MDS ������
5400 "Server Resource No Longer Available" ��Դ���ɼ���ʹ��
5401 ��ʾDownstream failure��
5402 "Client Session Terminated" �ͻ��˱�����Ա�رջỰ
5403 "Server Shutting Down" RTSP server ���ڹر�
5500 "Server Error" δ֪�¼�����
5502 "Internal Server Error" �ڲ������·�����ֹͣ����
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
			//ֱ���������ͷ�ӵ�ǰλ�ò��ţ�����ҳ���Ϸ���Ϣ
			//��Ϊҳ���յ���Ϣ����˳���ǰ���ţ�����û���κ���ʾ
			//ҳ��ȱ�ݣ�������ʱ����һ�£��Ժ����ҳ�������ط�ɾ�˾�����
			//����Ϣ����������
			ret = Roc_VOD_Play(me->vhandler, 1, -1);
			break;
		}
#endif
		
		//��������ͣ������ҳ�����Roc_VOD_Play��ʱ��
		//���ǵ���ͣ��Ӧ���ܻ�δ��������ʱ������Roc_VOD_Pause����֮��
		//��Pause��Ϣ����֮����������ͣ״̬��������play��ʱ�򻹻ᷢһ����ͣ
		//�����ٴη�play��ʱ���Ѿ��ڷ�������δ��Ӧ������£��������������������
		//�����ٴη�play������ܻ������⣬��ʱ���ǵ�cseq��������
		ret = Roc_VOD_Pause(me->vhandler, ROC_TRUE);
		me->flag_boundary = 1; //�����Ѿ��������ı߽磬�ٴ�play��ʱ�򣬲��Ǵ�now�����Ǵ�beginning

		//��������ͣ״̬����ֹ�ظ���ͣ
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
			//ֱ�������˵�β�ӵ�ǰλ�ò��ţ�����ҳ���Ϸ���Ϣ
			//������˵�β��һ���������¼��3�����ϣ�32���ٿ���
			//480s����8����15s���꣬2Сʱ120����225s���꣬24Сʱ��2700s����45��������
			// 3��ʱ��135�������꣬�û���������Сʱ���˵�ͷ��Ҳ�湻���ĵ���
			//����ֱ�ӿ��ؿ���Ҳ�������з����Ի���ô������:)
			ret = Roc_VOD_Play(me->vhandler, 1, -1);
			break;
		}
#endif
		
		//����ط�������ԭ���ǣ���ֹҳ��û�лָ�����ômplayer��û����
		ret = Roc_VOD_Pause(me->vhandler, ROC_TRUE);
		me->flag_boundary = 1;//�����Ѿ��������ı߽�
		
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
		//�����⼸��û������֪���������ʲô����
		/**********************************************************
		1101 "Playout Cancelled" ¼�ƵĲ���ȡ��
		1102 "Playout Started" ¼�ƵĲ��ſ�ʼ.
		1103 "Playback Stalled" ¼�ƵĲ�����ʱ������
		1104 "Playout Resumed" ¼�ƵĲ��Żָ�
		2104 ��ʾ��λ����ǰֱ���㣻
		***********************************************************/
        break;
    }
    
    return G_SUCCESS;
}


/******************************************************
��������    :
����        :
�������    :

�������    :
����ֵ      :
����        ��
******************************************************/

//��������������е�״̬���ú����ϲ㷵��Ϣ
//����������close֮���������ĸ�������onewave_vod_response_nfy_xxx��
STATIC INT32_T onewave_vod_response_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2)
{
    OnewaveMgr *me 			= (OnewaveMgr *)mgr;
    INT32_T ret 			= G_SUCCESS;
    rtsp_info *info 		= NULL;
	rtsp_request *connect 	= NULL;
    VOD_STATUS_e pstatus 	= VOD_PLAY_NONE;
	
	//���ĳ��case�õ������memsetһ��
    Roc_Queue_Message_t q_msg = {0,0,0,0};
	
	FAILED_RETURNX( !me || !me->copy_mgr,G_FAILURE );
	
    info = rtsp_mgr_get_info(me->copy_mgr,0);
    FAILED_RETURNX(!info, G_FAILURE );
	
	connect = rtsp_mgr_get_request(me->copy_mgr,0);
	FAILED_RETURNX( !connect, G_FAILURE );

    switch( msg )
    {
		case RTSP_ACK_ANNOUNCE_RESPONSE: // ��announceͷ��announce��Ϣ
		case RTSP_ACK_SOCKET_RECV_ERR:	//rtsp������մ�����һ���������Ĵ�����Ϣ��announce��Ϣ
		{
			VOD_INFO(("Enter RTSP_ACK_ANNOUNCE_RESPONSE case \n"));
			ret = onewave_vod_response_nfy_announce(me, p1, p2);
		}
		break;
				
    //�����������������и���¼�����������Ļ���
    //��������������������Ӧ���Ͳ��ٷ����ˣ������������������Զ�����DESCRIBE�������ӵ�
    //ÿ���յ�ǰ�˷�����������������Ϣ�󣬰����������Ļ�Ӧ��Ϣ��������������������
    //Ҳ����˵����������Ľ��պͷ������������һֱ��������������0��������ȥ
    //�����GET_PARAMETER��ΪĬ��������������Ϊ������õ����������������֧�ֵĻ�
    //����ʱû��Ĭ��������������£���ʱ3��ʱ���������Ͳ������ˣ����ӻ�Ͽ�
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
			//�����Roc_VOD_Close��������Roc_VOD_Stop
			ret = Roc_VOD_Close(me->vhandler, ROC_FALSE); //�����ʱ��close mplayer���������handle
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
			break; //playʧ�ܲ��ùرո�ʵ���������´β��Ż�ɹ�
		}
		
		//״̬�ж�
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
		
		//����ѡʱ��������˵ĳɹ���Ϣ
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
		
    case RTSP_ACK_PAUSE: //pause���ؽ���ʧ�ܾ͹رյĲ���������ʧ��Ҳûʲô��ϵ
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

		//����ifδִ�оͻ�ʧ�ܣ�ִ�оͻ�ɹ�
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
	
	//������Щ��socket����ʧ�ܵ���Ϣ����rtsp_request_reponse����
	case RTSP_ACK_EXCEPTION:	//�����Ϣ�����ش�����Ҫ�ر�rtsp����
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
		//����ѡʱ��������˵�ʧ����Ϣ
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
		//playʧ�ܲ��ùرո�ʵ���������´β��Ż�ɹ�
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

	//������Ϣ���ô���Ŀǰ�ϲ��޷�ʶ����Щ��Ϣ������Ҳû�취����
    default:
		/***************
		RTSP_ACK_SET_PARAMETER,  				//UTû��SET_PARAMETER��PING����
		RTSP_ACK_ERROR_SETPARAMETER_RESPONSE,
		RTSP_ACK_PING,
        		RTSP_ACK_ERROR_PING_RESPONSE,
		RTSP_ACK_ERROR_RESPONSE,				//ʾ������û�ù�
        		RTSP_ACK_ERROR_STOP_RESPONSE,			//teardown�����޷�������
		RTSP_ACK_CON_ERROR,					//��֪����ʲô�õģ�û�ù�
		RTSP_ACK_SOCKET_SEND_ERR,				//rtsp����send�����޷�������
		****************/
        break;
    }

    return ret;
}

/**
** @brief
**  ��Onewave VOD�ͻ��ˡ�
**
** @param[in]    url             Ҫ���ŵ�url��ַ����rtsp://��ͷ
** @param[out]   vodHandle       VOD�ͻ��˿��ƾ��
**
** @retval 0  �ɹ�
** @retval -1 ʧ��
*/
//���ڲ����õ�ʱ����Ҫʹ��Roc_VOD_xxx��������Onewave_VOD_xxx
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

    //����VOD���
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

	if(54000 == g_onewave_port) //ֻ�е��˳�������ٴ򿪵�ʱ��Ż�����Ϊ54000��������54001~64000��ѡһ��
	{
		g_onewave_port += GetRand((UINT32_T)vodHandle, 10000);
		VOD_DBUG(("the port we will use in whole browser life is %d!\n", g_onewave_port));
	}

    memset(&g_arrayOnewaveMgr->array_client[index], 0, sizeof(OnewaveMgr));
    g_arrayOnewaveMgr->array_client[index].is_using  = 1;
    g_arrayOnewaveMgr->array_client[index].vhandler  = *vodHandle;
    g_arrayOnewaveMgr->array_client[index].ihandler  = index;
    g_arrayOnewaveMgr->array_client[index].copy_mgr  = rtsp_mgr_new(NULL, index);
    g_arrayOnewaveMgr->array_client[index].copy_mgr->client_id = index;           //����ʵ���ڲ�ID��rtsp_mgr

    client->handle             = (void *)&g_arrayOnewaveMgr->array_client[index];//handle���������xxx_nfy�ĵ�һ������mgr
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

	//������ΪVOD_TYPE_VOD�Ĺ���ʵ��
    ret = rtsp_mgr_open(me->copy_mgr,VOD_TYPE_VOD,url,0,*vodHandle);
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  �ر�һ��VOD�ͻ��ˡ�
**
** @param[in]    vodHandle       VOD�ͻ��˿��ƾ����
** @param[in]    closeMode       �ر�ģʽ, ROC_TRUE:�ر�ʱ�������һ֡; ROC_FALSE:�ر�ʱ���������һ֡��
**
** @retval 0  �ɹ�
** @retval -1 ʧ��
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
	//�˳�ǰȡ������
	Roc_Audio_Set_Mute(ROC_FALSE);
#endif

	if(!g_arrayOnewaveMgr)//���ﲻʹ��FAILED_RETURNX������Ҫ���˳�ǰ�ص�mplayer
	{
		VOD_ASSERT(1); 
		ret = -1;
		goto Destory_Mplayer;
	}

    //���VODʵ���Ƿ��Ѿ��ر�
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
        ret = 0; 				//�Ѿ��رգ����عرճɹ�
		goto Destory_Mplayer; //ǿ�����ٻ����ڵ�mplayer
    }

    me = OnewaveVod_getMgr(index);
	if(!me || !me->copy_mgr)//���ﲻʹ��FAILED_RETURNX������Ҫ���˳�ǰ�ص�mplayer
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
    if(status < VOD_PLAY_OPEN)//����ط�����ǿ������mplayer����Ϊ��δ��
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_CLOSE,0,0);
    }
    else //closeʱ����0������mplayer��stopʱ������
    {
        ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_CLOSE,0,0);

		if(G_FAILURE == ret) //rtsp_mgr_proc_event������FAILED_RETURNX( !me ,ret )����ʧ�ܣ���ʱ��Ҫǿ�ƹر�mplayer
		{					//����ʧ�����mplayer�϶��Ѿ��ر��ˣ����ﲻ�����ִ�����
			VOD_ERRO(("rtsp_mgr_proc_event RTSP_EVENT_CLOSE failed!\n"));
			goto Destory_Mplayer; //mplayer�Ѿ��رյ�����£��ٴιرջᱨROC_MP_ERR_INVALID_HANDLE���󣬵���Ӱ������
		}
    }
	
	//mplayer���ٺ󣬽�onewaveʹ�õľ�̬handle��0
	//������ʹ�ã������������Ǹ�else��
	//����Ϊclose֮�����۳���ʲô�쳣״����Ҫ����̬handle��0
	//������һ�β��ſ϶���ʧ�ܣ�mplayer����ʾROC_MP_ERR_INVALID_HANDLE
	mp_handle = 0;
	
Destory_Mplayer: 

	if(0 != mp_handle) //������ҳ�������ܻ��ߵ���VOD��ʵ���Ѿ��رգ�����mplayer���ڣ���������
	{
		//������������̣���ʱ������mp_handle�Ѿ���Ϊ0��
		//if(0 != mp_handle)��������mp_handle = 0δִ��ʱǿ�ƹر�mplayer
		//�в��ɰ�����ط�Ų��mp_handle = 0ǰ�棬��Ϊ�����ر���������RTSP_EVENT_CLOSE��������
		//����ʧ�ܵ���������¹رգ��ɹ��ر�֮���ٴιر�ûʲô�ô������ܻ��������
		INT32_T mp_ret = Roc_MP_Close(mp_handle);
		if(0 != mp_ret)
		{
			VOD_ERRO(("Roc_MP_Close failed! mp_ret=%d\n",mp_ret));
			//���ﲻ�������ر�ʧ��Ҳûʲô�취
		}
		else
		{
			VOD_INFO(("Roc_MP_Close success!\n"));
		}
		mp_handle = 0; //����Ϊ0����Ϊ���handle�Ѳ��ٿ���
	}
	
	//closeʱ����ȫ�ֶ˿�ΪĬ�϶˿�
	g_onewave_port = 54000;
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  VOD���Žӿڡ�
**
** @param[in]    vodHandle       VOD�ͻ��˿��ƾ����
** @param[in]    scale           ���ű��١�scale=1:��������; scale<0:����; scale>1:���; 0<scale<1:������
** @param[in]    npt             ����ʱ��ƫ�ƣ���λΪ��
**                               npt>=0:�ӿ�ʼλ��ƫ��nptʱ��󲥷�;
**                               npt<0:�ӵ�ǰλ�ò���;
**
** @retval 0  �ɹ�
** @retval -1 ʧ��
*/
//�����е�ѡʱ�ͱ��ٲ������÷���play�����У�����Ӧ�÷���ioctl�У�������ܻ�δʵ��
//��ͷ��β������Ϣ����response_play��
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

	//����ΪUDP������������Ϊopen��ʱ����ܻ����url���óɴ����Э������
	//�������ÿ�����rtsp_info_proc��ִ����ȷ�Ĳ���ģʽ
    rtsp_info_set_property(info,RTSP_INFO_PROP_PROTOCOLS,VOD_PROTOCOL_IP_TS);

    rtsp_info_get_property(info,RTSP_INFO_PROP_STATUS,&status);
    VOD_INFO(("Roc_VOD_Play: status is %d\n",status ));
	
    if(status < VOD_PLAY_READY)       //����״̬���������£�����play�ӿڿ϶��Ƿ���ʧ�ܵ�
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_ERROR_PLAY_RESPONSE,0,0);
        return -1;
    }
	
    //����ÿ�ν���play�Ȱ�seek��scale��־��0�����npt>=0����1����־ѡʱ���߿�����˲���
    //Ϊ�˷���ά������Ҫ���������֮����κεط���ֵ������
    //����mplayer��������url�ı�־λ������0
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
	
	//�����ֵΪ1�����ʾ��Ҫ��������url
	//ѡʱ������߽����²��Żᵼ�����ֵ��Ϊ1
	//������˲�����������url����Ϊ����ᵼ��mplayer����ʱ�ӼӴ�
	if(me->flag_seek || me->flag_boundary)
	{
		me->flag_MpSetUrl = 1;
	}
	
	/*ÿ�����²��ţ����������ͣ״̬�����ȷ���ͣ*/
	//��һ�β��ţ����״̬�϶���VOD_PLAY_READY���������⴦���һ��
	//������Ϊ�˷���������ͣ������ֱ�Ӳ������ڷ��͵�������������
	//����ط�����rtsp��������ɷ���
	//��ʱ�����ִ��play������������ʱ��ʱ�䲻�ᳬ��1s
	//�����յ���Ӧ���϶�Ҳ����ʱ�ӵģ���������������ʱ��
	if(status >= VOD_PLAY_NORMAL && status != VOD_PLAY_PAUSE)
	{
		ret = Roc_VOD_Pause(vodHandle, ROC_TRUE); //��ͣʧ�ܴ��ڷ�������ʧ�ܺ���Ӧʧ�ܣ����ں��ߣ����ܴӷ���ֵ�ж�
	}
	
    //����ʱ��
	rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);

	//����ط��Ƚ����⣬ֱ�����˻ָ���ʱ�򣬷�����ά���ĵ�ǰ����λ���ǿ��˵���λ��
	//���Ƿ��ص�ʱ��ȷʵ��ֱ��ʱ�䣬���ǿ��˻�ȥ��ʱ��
	//����ֱ��ֱ����"npt=now-"�Ϳ��ԣ�������ά����ý�����ĵ�ǰλ���ǶԵ�
	//ֱ����֧��ѡʱ�����Ҫѡʱ��ֱ����ֱ���ûؿ�����
	
	if(0 == me->flag_not_live) //ֱ���ӵ�ǰλ�ò��ţ���֧��ѡʱ��������֧�ֿ������
	{
		//ע�⣬ֱ����֧��beginning��end��now�ֶ�
		//ȱʡλ�ã�����ֱ����ĿΪý���������ǰ���յ�ý����λ��
		sprintf(time_info->seektime, "npt=now-");
	}
	else
	{
		//С��0����ӵ�ǰλ�ÿ�ʼ���ţ���ʱ��Ϊ������ά��
		/************************************************************************************
		����UT�Ĺ淶�ἰ:
		�տ�ʼ����ʱ��play��range�ֶΣ�NTPʱ��ʹ��beginning-��now-��beginning-end��now-end
		��ͣ�����˻���֮������������ʱ��play��range�ֶΣ�NTPʱ��ʹ��now-��now-end
		ͳһ�������õ�ֻ��now-��now-end����������һ��
		����߽�ָ����ţ���ò�Ҫʹ��beginning-��beginning-end��ֱ�Ӵ���ʱ���
		************************************************************************************/
	    if(npt < 0 && 0 == me->flag_boundary) //û�е���߽�ӵ�ǰλ�ÿ�ʼ�� 
	    {
			VOD_WARN(("will play from now!!!\n"));
			sprintf(time_info->seektime, "npt=now-");
	    }
	    else //���ڵ���0����ʾ��ƫ��ʱ��npt��ʼ��������0����ʾ�ͷ
	    {
	    	UINT32_T current_T;
			if(time_info->start_time->type == TIME_TYPE_YMDHMS)
			{
				//ע���ʱ�䳬��ʱ�����ж�
				//��ʱҳ�洫���npt�������1970�����������������ƫ��ʱ��
				//os_mktime������ֵ��GMT+8ʱ����ֵ
				//os_gmtime��Ҫ��Ҳ��GMT+8ʱ����ֵ
				//�е���? ���������ģ������Ķ���GMT+0��clock�ַ���������20141208T012600.00Z
				//GMT+0��clock-->os_mktime-->GMT+8��Ӧ������(UTC)-->os_gmtime-->GMT+0��clock
				//����os_mktime��os_gmtime����ͳһ�ģ������˱���ʱ��
				//time.h�е�mktime��gmtime�����õ�tm�ṹ����Ҳ����ָ������ʱ��
				//���ָ���ˣ����Ǳ���ʱ����UTCʱ�䣬δָ������GMT+0��UTCʱ��
				
				if(1 == me->flag_boundary) //����npt < 0 && 1 == me->flag_boundary������������²�����npt<0�����
				{
					VOD_WARN(("Beginning or End of Stream Reached,"
						" and someone call me to play again, will play form beginning!!!\n"));

					current_T = me->start_T + 1; //����߽�ָ�����ʼ��1s��ʼ��
					me->flag_boundary = 0; //�����øñ�־Ϊ0���������ͣ��������˻��Ǵӵ�ǰλ�ò�
				}
				else
				{
					current_T = (UINT32_T)npt; 	//�ϲ�����ǵ�ֵҲ����ΪGMT+8ʱ����ֵ�������ټ�8Сʱ
				}

				VOD_DBUG(("clock start time is %u, end time is %u, current_T is %u\n", 
					me->start_T, me->end_T, current_T));
				
				if(current_T <= me->start_T)
				{
					current_T = me->start_T + 1; //seek��ͷʱ������Ƶͷ����1�봦����
				}
				else if(current_T >= me->end_T)
				{
					current_T = me->end_T - 3; //seek��βʱ������Ƶβ��ǰ3�봦����
				}

				VOD_DBUG(("current_T - me->start_T = %u", current_T - me->start_T));
				
				TIME_FMT current_time[1];
				memset(current_time, 0, sizeof(TIME_FMT));
				current_time->type = TIME_TYPE_YMDHMS;

				//���ڷ��������õ�UTCʱ�䣬��Ҫʹ��os_gmtime��������os_localtime
				memcpy(current_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&current_T), sizeof(OS_TIME));
				
				INT8_T current_time_string[20] = {0};
				ret = time_fmt_to_string(current_time, current_time_string);
				current_time_string[19] = 0;

				//��ʱtime_fmt_to_string��õ��ַ���������20141207T230800
				//����UT�淶��Ӧ����20141207T230800.00Z��������Tʱ����.����Z�ĸ�ʽ
				sprintf(time_info->seektime, "clock=%s.00Z-", current_time_string);
				
			}
			else if(time_info->start_time->type == TIME_TYPE_NPT)
			{
				if(1 == me->flag_boundary)
				{
					VOD_WARN(("Beginning or End of Stream Reached,"
						" and someone call me to play again, will play form beginning!!!\n"));

					current_T = time_info->start_time->u.npt_time + 1; //����߽�ָ�����ʼ��1s��ʼ��
					me->flag_boundary = 0; //�����øñ�־Ϊ0���������ͣ��������˻��Ǵӵ�ǰλ�ò�
				}
				else
				{
					//��ʱҳ�洫����������Ƭͷ��ʱ��
					current_T = time_info->start_time->u.npt_time + npt;
				}
				
				VOD_DBUG(("clock start time is %u, end time is %u, current_T is %u\n", 
					time_info->start_time->u.npt_time, time_info->end_time->u.npt_time, current_T));

				//ע��npt==0���Ǵ���ӵ�ǰλ�ÿ�ʼ�������Ǳ�ʾ���ʼ��
				//����ط�С�ڵ������ʵ�����ڣ���Ϊ��ͳһ�������д��
				if(current_T <= time_info->start_time->u.npt_time) 
				{
					current_T = time_info->start_time->u.npt_time + 1;//seek��ͷʱ������Ƶͷ����1�봦����
				}
 				else if(current_T >= time_info->end_time->u.npt_time)
				{
					current_T = time_info->end_time->u.npt_time - 3;//seek��βʱ������Ƶβ��ǰ3�벥��
				}
				
				sprintf(time_info->seektime, "npt=%u-", current_T);
			}
			else
			{
				VOD_WARN(("time type not support, seek will not work\n"));
				sprintf(time_info->seektime, "npt="); //����������ȱʡλ�ã��ɷ���������
			}
	    }
	}
	//����ʱ������
	rtsp_info_set_property(info,RTSP_INFO_PROP_TIME_INFO,(INT32_T)time_info);
	
	//ֱ��Ҳ���Կ����ˣ�����ֻ�Ǳ��汶�٣����ٵĴ�����setscale������
	rtsp_info_set_property(info,RTSP_INFO_PROP_REQUEST_SPEED,scale);
	
	ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_PLAY,scale,npt);
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  VOD���Žӿڡ�
**
** @param[in]    vodHandle       VOD�ͻ��˿��ƾ����
** @param[in]    stopMode        ֹͣģʽ, ROC_TRUE:ֹͣʱ�������һ֡; ROC_FALSE:ֹͣʱ���������һ֡��
**
** @retval 0  �ɹ�
** @retval -1 ʧ��
*/

/******
��Ҫ˵��:VOD��������open��play��close��һ�㲥������������open��play��stop��close
�����stop����close��ʹ�ã�������ûʲô����?�϶����еģ�����ĿǰҲֻ����ô�ã�����:
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

��ΪVODֻ��setup(open)��play��teardown��close�������û��stop��Ӧ�����
����ط����Ǿ�������ˣ�stopҲ����ʵ��ʲô���ܡ�
VOD�Ĳ��Ź��ܣ����Ƿ����������������������Ĺ���
���stop������rtspЭ�鱾�������
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

�������Ҫע����:��stop���close�Ĺ����У�һ������Ҫ��������ǵ��ò����ں˵�˳��
������close�е���mp_close֮ǰ�ȵ���mp_stop�����ﲻ��ack_event_close�е���mp_stop��mp_close��ԭ����:
�����Ϊ�������⻹�Ƿ��������⣬����û���յ���������ô���ǻ�һֱռ��mplayer����Դ
�ᵼ���´β���ʧ�ܣ�����ط�����ͬ�����á�
����Ūһ�����ӵķ�ʽ���ȴ����գ����ճ�ʱǿ�Ƶ���
���������ϵͳ�ĸ��ӶȺͶ���Ŀ����ͬ�����⣬��Ҫ����
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

��Ȼ������Ҳ������open��ack_event_open����playʱ�е��õ�mp_open
������ack_event_play�յ�������play��������ķ���ʱ�ŵ��õ�mp_open��mp_play�����Ƿ������Ѿ���ʼ����
��������ԭ��������ֻ�з���play����֮��ǰ�˲��ܸ����Ƿ���������mp_open���ܳɹ�
���������mplayer����ʵ�ֵ����⣬�����ǵײ�ffmpeg�����⡢������Ӳ������������
����ط�����Ҳ��û����ȴռ��ϵͳ��Դ�Ŀ��ǣ��ײ��ʵ��Ҳ�Ǿ�������˼����
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

һ�㲥����������Ӧ����:
1��open������ϵͳ��Դ����ʼ�������߳�
2��play����ȡý���������н⸴�á����룬��һ����mp_open�������ˣ��������
3��stop��ֹͣ�⸴�ý��룬ֹͣ��ȡý����
4��close���˳������̣߳��ͷ�ϵͳ��Դ
5��ioctl�����Ʋ����̣߳������������������Ӱ������
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

rtspЭ������ô����:
1��describe��setup����ȡ������ý������Ϣ������һ���˿ں������������Դ���൱��open
2��play��֪ͨ��������ʼ���������������playһ����
3��stop���޶�Ӧ����
4��teardown��֪ͨ������ֹͣ�������������Ǳ����ٸÿͻ��˶�Ӧ����Դ������
����Ӧ�÷ֳ������������play��ʱ��֪ͨ������ѡ��ͬ��Դ�����������ѡ���
�����Ļ�ÿ�β������ٸÿͻ���ռ�õĶ˿ڵ�ϵͳ��Դ
5��������������൱��ioctl

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
����ط���close���ֿ�������Ϊ��stop�в�����mplayer��Ƶ�����ٻ�������
ֻ�е�close��ʱ�������
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
	//�˳�ǰȡ������
	Roc_Audio_Set_Mute(ROC_FALSE);
#endif

    FAILED_RETURNX(!g_arrayOnewaveMgr, -1);

    //���VODʵ���Ƿ��Ѿ��ر�
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
    else //������close������һ�£���ͬ�ĵط��������ʱ��onewaveά����mp_handle����rtsp_mgr_proc_eventһ����������������
    {
    	//ֻ�е�����Ĳ�����rtsp_mgrά����һ��ʱ���Ų�������
    	//ʵ��stop������mplayer��closeʱ���٣��ֲ�Ӱ��ԭ�й���
        ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_CLOSE,0,(INT32_T )&mp_handle);
		VOD_INFO(("after close, mp_handle is %u!\n", mp_handle));
    }
	
    VOD_INFO(("%s Leave\n",__FUNCTION__));

    return ret;
}

/**
** @brief
**  VOD���Žӿڡ�
**
** @param[in]    vodHandle       VOD�ͻ��˿��ƾ����
** @param[in]    pauseMode       ��ͣģʽ, ROC_TRUE:��ͣʱ�������һ֡; ROC_FALSE:��ͣʱ���������һ֡��
**
** @retval 0  �ɹ�
** @retval -1 ʧ��
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
    if(status < VOD_PLAY_NORMAL) //��û��ʼ���ţ�������ͣ��ֱ��������ͣ
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_ERROR_PAUSE_RESPONSE,0,0);
        return 0;
    }

    ret = rtsp_mgr_proc_event(me->copy_mgr,RTSP_EVENT_PAUSE,0,0);

    return ret;
}

//���ò��ű���
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
    if(status < VOD_PLAY_NORMAL) //��û��ʼ���ţ����ܿ�����ˣ�����ֱ�����Կ������
    {
        ret = rtsp_mgr_proc_ack_event(me->copy_mgr,RTSP_ACK_ERROR_PLAY_RESPONSE,0,0);
        return 0;
    }
	
	//����Ҫ���scale������UT֧��1.0��2.0��4.0��8.0��16.0��32.0��-2.0��-4.0��-8.0��-16.0 ��-32.0
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
	
	//-1��ʾ�ӵ�ǰλ�ÿ�ʼ�������
    ret = Roc_VOD_Play(vodHandle, scale, -1);

    return ret;
}

//��ȡ���ű���
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
    if(status < VOD_PLAY_NORMAL) //��û��ʼ���Ų��ܻ�ȡ��ǰ���٣�����Ҳ���Կ������
    {
        return -1;
    }

	//ֱ����Ҳ�������ñ���
    rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&scale_tmp);

    *scale = scale_tmp;

    return 0;
}

//��ȡƬԴʱ�� ��λ������
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
	
	//��δ�򿪻�����ֱ���������ܻ�ȡ��ǰʱ��
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


//��ȡ��ǰ����ʱ��
//onewave��֧��UPDATE_TIME����
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

	//��δ���ţ����ܻ�ȡ��ǰλ�ã�ֱ����Ҳ���Ի�ȡ��ǰλ��
    if(status < VOD_PLAY_NORMAL)
    {
        VOD_ERRO(("vod is not open!!!\n"));
        return -1;
    }
	
    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);
    rtsp_info_get_property(info,RTSP_INFO_PROP_CURRENT_SPEED,&speed);

	if(time_info->present_time->type == TIME_TYPE_NPT)
	{
		//TIME_TYPE_NPT���ص����Ѳ���ʱ�䣬�������Ƭͷ�ĺ�����
		if(status != VOD_PLAY_PAUSE)
	    {
	    	//�����б�Ҫ����һ�£�Ϊʲôǿת���Լ�������֮ǰ����Ҫǿת��ԭ��
	    	//����gcc/g++����UINT��INT����������Ľ���Ǳ�����һ��UINT����ʱ�����е�
	    	//���������������֮����ǿת���ܻ����
	    	//������Ϊ�����������Ǹ�ֵ�����ᱣ����һ��UINT����
	    	//���ڴ������Բ������ʽ���ڵģ���ΪUINT�����λ����λҲ����Ϊֵ��һ����
	    	//��������-3���ͻ���2^32-3���ǲ����Ӧ���޷���ֵ
			tmp = (INT64_T)time_info->present_time->u.npt_time 
			 	   + (INT64_T)speed*(INT64_T)((time_ms() - time_info->present_time->last_update_time)/1000)
			 	   - (INT64_T)time_info->start_time->u.npt_time;
	    }
	    else //��ͣʱ��������pause response�и��µ�ǰʱ��
	    {
	    	tmp = (INT64_T)time_info->present_time->u.npt_time - (INT64_T)time_info->start_time->u.npt_time;
	    }

		//npt��Ҫ�ж��Ƿ񳬹���ֹ��
		tmp = tmp < 0 ? 0 : tmp;
        tmp = tmp < (INT64_T)time_info->start_time->u.npt_time ? (INT64_T)time_info->start_time->u.npt_time : tmp;
        tmp = tmp > (INT64_T)time_info->end_time->u.npt_time ? (INT64_T)time_info->end_time->u.npt_time : tmp;
	}	
	else if(time_info->present_time->type == TIME_TYPE_YMDHMS)
	{
		//TIME_TYPE_YMDHMS���ص��Ǿ���ʱ�� ����ʾ1970�����������(UTC)
		//ע����GMT+8��UTCʱ��
		if(status != VOD_PLAY_PAUSE)
	    {
			tmp = (INT64_T)me->present_T 
					+ (INT64_T)speed*(INT64_T)((time_ms() - time_info->present_time->last_update_time)/1000);
		}
	    else //��ͣʱ��������pause response�и��µ�ǰʱ��
	    {
	    	tmp = (INT64_T)me->present_T;
	    }

		//�ؿ���ֱ�������ж���ֹ�㣬���������ģ����ܻᳬ�����ұ߽�
		
		TIME_FMT tmp_time[1];
		memset(tmp_time, 0, sizeof(TIME_FMT));
		tmp_time->type = TIME_TYPE_YMDHMS;

		//os_gmtime���������GMT+0��׼ʱ���struct tm��ת����clock�ַ�����������20141208T012600.00Z
		memcpy(tmp_time->u.ymdhms_time, os_gmtime((OS_TIME_T*)&tmp), sizeof(OS_TIME));
		
		INT8_T tmp_time_string[20] = {0};
		ret = time_fmt_to_string(tmp_time, tmp_time_string);
		tmp_time_string[19] = 0;
    	VOD_DBUG(("hold UTC should be %s.00Z\n", tmp_time_string));
	}
	
	//������Ȱ�tmpת��ΪINT64_T
	//���tmp��int�ͣ�ֱ����tmp*1000���õ�����һ��int��
	//���ܻ���������֮����תΪlong long ���Ǹ�ֵ
	*cur_position = 1000 * (INT64_T)tmp;
	
    VOD_INFO(("leave %s, cur_position=%lld ms\n", __FUNCTION__, *cur_position));

    return ret;
}


//��ȡ��ǰ��������Ϣ
//��Ӧ����static��������ʱ��������Ҫ���Ÿ��ϲ����
//����������vod_onewave_client.h��
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
        return 0; //����0������Ϊ��δ���ţ���ȡ��ǰ״̬�Ϳ�����
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
	
	//ֱ�������ܻ�ȡ���Űٷֱ�
    if(0 == me->flag_not_live) 
    {
        VOD_INFO(("vod is in live state!!!\n"));
        return 0; //����0������Ϊ����ֱ����ȡ������Щ��Ϣ���ǳɹ���
    }

    rtsp_info_get_property(info,RTSP_INFO_PROP_TIME_INFO,time_info);

	double progress = 0.0;

	//npt��ȡ�ĵ�ǰλ�����Ѳ���ʱ��
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
	
	//clock��ȡ�ĵ�ǰλ����UTCʱ��
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

	//�����ӡ������ֵ�뵽��Ӧ�Ľṹ����
	//playerinfo->Speed=0���������ٶ�,��ΪROC_MP_SPEED_NORMAL = 0
    VOD_INFO(("leave %s\n, playerinfo->State is %u, playerinfo->Speed is %u\n" 
			"playerinfo->TimePlayedInMS/1000 is %u sec, playerinfo->Progress is %u%%\n",
		__FUNCTION__, playerinfo->State, playerinfo->Speed, 
					playerinfo->TimePlayedInMS/1000, playerinfo->Progress));

    return ret;
}



//onewave VOD�ͻ��˿��ƾ��
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
��ȡvod app����������
����:
    vod_app_ctrl VOD APP���ƾ����
����ֵ:
    ���ڵ���0�ɹ���С��0ʧ�ܡ�
*******************************************************************************/
INT32_T onewave_udp_ctrl_handle(vod_app_ctrl_t **vod_app_ctrl)
{
    *vod_app_ctrl = &g_onewave_app_ctrl;
    VOD_INFO(("vod app type is onewave! \n"));
    return 0;
}

