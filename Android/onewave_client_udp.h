/*******************************************************************************
COPYRIGHT (C) 2013    SUMAVISION TECHNOLOGIES CO.,LTD.

File name   : onewave_client_udp.h
    
Description : 思华VOD client头文件

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
    rtsp_mgr *copy_mgr;	  //统一资源管理器
    INT32_T vhandler;     //实例外部句柄
    INT32_T ihandler;     //实例内部句柄
    INT32_T is_using;     //实例是否正在使用
    
	//UINT32_T最大值2^32-1，差不多40亿，2014年末距1970年已经过了14亿多秒，还能再用80多年
	UINT32_T live_T;		//直播的时候，保存当前直播时间点，因为直播返回的时间总是直播时间
							//快进快退的时候和当前时间不一致
	UINT32_T present_T;		//这三个是为了保存time_info的时间，方便使用
	UINT32_T start_T;		//对应于time_info为clock类型时从1970年开始的秒数，包含了本地时区的信息
	UINT32_T end_T;			//只用于TIME_TYPE_YMDHMS，不能用于TIME_TYPE_NPT

	//内部标志位，为了弥补VOD_STATUS_e中有些情况可能没考虑到
    UINT8_T flag_not_live; //1-不是直播，0-直播，也就是默认直播
    UINT8_T flag_seek;	   //选时，用于区分给上层消息，本层操作无变化
    UINT8_T flag_scale;	   //快进快退，用于区分给上层消息，本层操作无变化
    UINT8_T flag_boundary; //表示推流服务器通知我们流已经到了边界，或者到了起始点，或者到了终止点
    UINT8_T flag_MpSetUrl; //需要重新设置url给mplayer的标志，除了暂停恢复之外，这个值应该都为1

}OnewaveMgr;

//关于使用flag_not_live，而不是使用flag_live，是因为默认直播，而非点播
//更深层次的原因是如果是下面这样的格式，flag_live默认为0会有问题，处理起来比较复杂
//而flag_not_live默认为0就没有问题，只有当片长大于0是才是非直播
//a=rtpmap:33 MP2T/90000     	//即多个a=字段，flag_live会比较混乱
//a=range:npt=0.000-5626.000	 //而flag_not_live会在只有a=range:出现，并符合规则时，才会变为1
//a=rtpmap:33 MP2T/90000     	//这样处理起来就简单多了

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



