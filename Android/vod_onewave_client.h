/*******************************************************************************
COPYRIGHT (C) 2013    SUMAVISION TECHNOLOGIES CO.,LTD.

File name   : vod_onewave_client.h
    
Description : 思华VOD client头文件

Date          Modification        Name
----          ------------        ----
2013.02.20    Created             tdc
*******************************************************************************/

#ifndef _ONEWAVE_API_H_
#define _ONEWAVE_API_H_
#include "rtsp_client.h"
#include "vod_common_debug.h"
#include "vod_common_module.h"
#include "vod_common_api.h"


INT32_T OnewaveVod_Udp_Init( VODProject *project,rtsp_mgr *mgr );
INT32_T  OnewaveVod_Udp_UnInit( VODProject *project );
INT32_T onewave_udp_ctrl_handle(vod_app_ctrl_t **vod_app_ctrl);

INT32_T OnewaveVod_Tcp_Cable_Init( VODProject *project,rtsp_mgr *mgr );
INT32_T  OnewaveVod_Tcp_Cable_UnInit( VODProject *project );
INT32_T onewave_tcp_cable_ctrl_handle(vod_app_ctrl_t **vod_app_ctrl);

INT32_T Onewave_VOD_GetPlayerInfo(INT32_T vodHandle, void* data);

#endif //_ONEWAVE_API_H_



