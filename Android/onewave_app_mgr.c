/*******************************************************************************
COPYRIGHT (C) 2013    SUMAVISION TECHNOLOGIES CO.,LTD. 

File name   : onewave_app_mgr.c
	
Description : onewave应用管理模块API实现

Date          Modification        Name
----          ------------        ----
2014.12.01    Created             gy
*******************************************************************************/

#include "vod_app_mgr.h"
#include "vod_common_api.h"
#include "rtsp_client.h"
#include "vod_onewave_client.h"


/*------------------------     MACRO\ENUM\STRUCT  ---------------------------*/

/*---------------------------  External Variables ---------------------------*/

/*---------------------------  Private Variables  ---------------------------*/
extern VOD_APP_Area_e g_vod_app_area;

/*---------------------------  External Functions ---------------------------*/

/*---------------------------  Private Functions  ---------------------------*/

/*---------------------------  Function Definition --------------------------*/

//VOD APP管理初始化。后面的vod app area指定了区域
INT32_T OnewaveVod_Init( VODProject *project,rtsp_mgr *mgr)
{
	INT32_T ret;
	
    switch(g_vod_app_area)
    {
    case VOD_APP_AREA_XIANNING:
    {
        ret = OnewaveVod_Udp_Init(project,mgr);
        break;
    }
	case VOD_APP_AREA_HUASHU:
	{
        ret = OnewaveVod_Tcp_Cable_Init(project,mgr);
		break;
	}
	default:
	VOD_WARN(("unknown vod app area: 0x%x! \n",g_vod_app_area));
	ret = G_FAILURE;
	break;
    }

    return ret;
}

INT32_T  OnewaveVod_UnInit( VODProject *project )
{
	INT32_T ret;
	
    switch(g_vod_app_area)
    {
    case VOD_APP_AREA_XIANNING:
    {
        ret = OnewaveVod_Udp_UnInit(project);
        break;
    }
	case VOD_APP_AREA_HUASHU:
	{
        ret = OnewaveVod_Tcp_Cable_UnInit(project);
		break;
	}
	default:
	VOD_WARN(("unknown vod app area: 0x%x! \n",g_vod_app_area));
	ret = G_FAILURE;
	break;
    }

    return ret;
}

INT32_T onewave_app_ctrl_handle(vod_app_ctrl_t **vod_app_ctrl)
{
	INT32_T ret;
	
    switch(g_vod_app_area)
    {
    case VOD_APP_AREA_XIANNING:
    {
        ret = onewave_udp_ctrl_handle(vod_app_ctrl);
        break;
    }
	case VOD_APP_AREA_HUASHU:
	{
        ret = onewave_tcp_cable_ctrl_handle(vod_app_ctrl);
		break;
	}
	default:
	VOD_WARN(("unknown vod app area: 0x%x! \n",g_vod_app_area));
	ret = G_FAILURE;
	break;
    }

    return ret;
}

