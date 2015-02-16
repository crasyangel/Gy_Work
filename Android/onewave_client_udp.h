/*******************************************************************************
COPYRIGHT (C) 2013    SUMAVISION TECHNOLOGIES CO.,LTD.

File name   : onewave_client_udp.h
    
Description : ˼��VOD clientͷ�ļ�

Date          Modification        Name
----          ------------        ----
2014.12.01    Created             gy
*******************************************************************************/

#ifndef _ONEWAVE_CLIENT_UDP_H_
#define _ONEWAVE_CLIENT_UDP_H_

#include "vod_onewave_client.h"
#include "MediaPlayer.h"
#include "roc_resman_dec_api.h"
#include <time.h>

#define ONEWAVE_USER_AGENT     "User-Agent: SUMA RTSP 1.0\r\n"
#define END_STRING             "\r\n"


#ifdef  __cplusplus
extern "C" {
#endif

typedef struct tag_OnewaveMgr
{
    rtsp_mgr *copy_mgr;	  //ͳһ��Դ������
    INT32_T vhandler;     //ʵ���ⲿ���
    INT32_T ihandler;     //ʵ���ڲ����
    INT32_T is_using;     //ʵ���Ƿ�����ʹ��
    
	//UINT32_T���ֵ2^32-1�����40�ڣ�2014��ĩ��1970���Ѿ�����14�ڶ��룬��������80����
	UINT32_T live_T;		//ֱ����ʱ�򣬱��浱ǰֱ��ʱ��㣬��Ϊֱ�����ص�ʱ������ֱ��ʱ��
							//������˵�ʱ��͵�ǰʱ�䲻һ��
	UINT32_T present_T;		//��������Ϊ�˱���time_info��ʱ�䣬����ʹ��
	UINT32_T start_T;		//��Ӧ��time_infoΪclock����ʱ��1970�꿪ʼ�������������˱���ʱ������Ϣ
	UINT32_T end_T;			//ֻ����TIME_TYPE_YMDHMS����������TIME_TYPE_NPT

	//�ڲ���־λ��Ϊ���ֲ�VOD_STATUS_e����Щ�������û���ǵ�
    UINT8_T flag_not_live; //1-����ֱ����0-ֱ����Ҳ����Ĭ��ֱ��
    UINT8_T flag_seek;	   //ѡʱ���������ָ��ϲ���Ϣ����������ޱ仯
    UINT8_T flag_scale;	   //������ˣ��������ָ��ϲ���Ϣ����������ޱ仯
    UINT8_T flag_boundary; //��ʾ����������֪ͨ�������Ѿ����˱߽磬���ߵ�����ʼ�㣬���ߵ�����ֹ��
    UINT8_T flag_MpSetUrl; //��Ҫ��������url��mplayer�ı�־��������ͣ�ָ�֮�⣬���ֵӦ�ö�Ϊ1

}OnewaveMgr;

//����ʹ��flag_not_live��������ʹ��flag_live������ΪĬ��ֱ�������ǵ㲥
//�����ε�ԭ������������������ĸ�ʽ��flag_liveĬ��Ϊ0�������⣬���������Ƚϸ���
//��flag_not_liveĬ��Ϊ0��û�����⣬ֻ�е�Ƭ������0�ǲ��Ƿ�ֱ��
//a=rtpmap:33 MP2T/90000     	//�����a=�ֶΣ�flag_live��Ƚϻ���
//a=range:npt=0.000-5626.000	 //��flag_not_live����ֻ��a=range:���֣������Ϲ���ʱ���Ż��Ϊ1
//a=rtpmap:33 MP2T/90000     	//�������������ͼ򵥶���

typedef struct tag_ArrayOnewaveMgr
{
   VODProject* copy_project;
   OnewaveMgr  array_client[MAX_RTSP_CLIENT_NUM];
   rtsp_mgr*   copy_mgr;
}ArrayOnewaveMgr;


/************************************************************************/
/* function     prototype,                !!!!!!!!       here just for show       !!!!!!!!         */
/*****************************************************************************************************/
STATIC INT32_T Onewave_VOD_Open(INT8_T *url, INT32_T *vodHandle);
STATIC INT32_T Onewave_VOD_Close(INT32_T vodHandle, ROC_BOOL closeMode);
STATIC INT32_T Onewave_VOD_Play(INT32_T vodHandle, INT32_T scale, INT32_T npt);
STATIC INT32_T Onewave_VOD_Stop(INT32_T vodHandle, ROC_BOOL stopMode);
STATIC INT32_T Onewave_VOD_Pause(INT32_T vodHandle, ROC_BOOL pauseMode);
STATIC INT32_T Onewave_VOD_SetScale(INT32_T vodHandle, INT32_T scale);
STATIC INT32_T Onewave_VOD_GetScale(INT32_T vodHandle, INT32_T *scale);
STATIC INT32_T Onewave_VOD_GetDuration(INT32_T vodHandle, INT64_T *duration);
STATIC INT32_T Onewave_VOD_GetCurPosition(INT32_T vodHandle, INT64_T *cur_position);
STATIC INT32_T Onewave_VOD_GetPlayInfo(INT32_T vodHandle, vod_play_info *play_info);

STATIC INT32_T onewave_vod_describe_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_setup_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_play_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_close_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_pause_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_get_parameter_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_option_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_response_nfy(void *mgr,INT32_T msg,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_response_nfy_announce(void *mgr,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_response_nfy_describe(void *mgr,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_response_nfy_setup(void *mgr,INT32_T p1,INT32_T p2);
STATIC INT32_T onewave_vod_response_nfy_play(void *mgr,INT32_T p1,INT32_T p2);
/**********************************************************************************************************/
/* static                                                               */
/************************************************************************/

#ifdef  __cplusplus
}
#endif

#endif //_ONEWAVE_CLIENT_UDP_H_



