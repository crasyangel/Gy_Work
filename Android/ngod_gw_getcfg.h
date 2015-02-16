#ifndef _NGOD_GW_GETCFG_H_
#define _NGOD_GW_GETCFG_H_

#ifdef __cplusplus
extern "C" {
#endif

//********************** Include Files ***************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h> 
#include <ctype.h>
#include <stdbool.h>

#include "rocme_porting_osp.h"
#include "rocme_porting_socket.h"
#include "vod_ext_debug.h"
#include "roc_sys_prop.h"
#include "cJSON.h"

//********************** Global Functions *************************************

//ªÒ»°VOD Portalµÿ÷∑
void ngod_gw_portal_http_init();

#ifdef __cplusplus
}
#endif

#endif  /*_NGOD_GW_GETCFG_H_*/

