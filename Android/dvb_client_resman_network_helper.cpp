/*******************************************************************************
COPYRIGHT (C) 2014    SUMAVISION TECHNOLOGIES CO.,LTD. 

File name   : dvb_client_resman_network_helper.cpp

Description: 
参考frameworks_base\core\jni\android_net_xxx和
\system\core\libnetutils


Project:  android4.2 hi3716c

Date            Modification        Name
----            ------------        ----
2014.12.30      Created             gy
*******************************************************************************/
#include "dvb_client_resman_network_helper.h"


/********************** Private Class Func *************************************/
ResmanNetwork::ResmanNetwork():
	g_evt_reg_list(NULL),
	g_evt_handle(1),
	g_device_list(NULL),
	g_device_count(0),
	g_sqlite_setting_DB_fake(NULL),
	g_sqlite_setting_num_fake(0),
	g_sqlite_ErrMsg_fake(NULL),
	g_ping_readTask_handle(0),
	g_ping_fd(NULL),
	dhcpID(0),
	g_isRealTime(true),
	g_noRT_devNode(NULL),
	g_ntp_Isfirst(0)
{
	RESC_DEBUG("getuid %d, geteuid %d\n", getuid(), geteuid());
		
	memset(&g_ping_param, 0, sizeof(_ping_param_t));
	memset(&g_ntp_info, 0, sizeof(NtpInfo_t));
	
	g_evt_lock = rocme_porting_mutex_create(ROC_MUTEX_TIMED_NP);
	g_device_lock = rocme_porting_mutex_create(ROC_MUTEX_TIMED_NP);

	int err = 0;
	err = InitDeviceList();
	if(err != 0)
	{
		RESC_PRINT("InitDeviceList failed\n");
	}
	
	UINT32_T task_handle = 
		rocme_porting_task_create((INT8_T *)"detect eth0 task",&(ResmanNetwork::detect_eth0_connect),
				this,ROC_TASK_PRIO_LEVEL_1,10*1024);
	if (task_handle == 0)
	{
		RESC_PRINT("create ping task failed\n");
	}
	
	err = ntp_update_onReboot();
	if(err != 0)
	{
		RESC_PRINT("ntp_update_onReboot failed\n");
	}
}

ResmanNetwork::~ResmanNetwork()
{
	FreeDeviceList();
	
	rocme_porting_mutex_destroy(g_evt_lock);
	rocme_porting_mutex_destroy(g_device_lock);
}

void ResmanNetwork::net_mutex_lock(UINT32_T mutex)
{
    if( 0 == mutex )
    {
        return;
    }
    rocme_porting_mutex_lock(mutex);
}

void ResmanNetwork::net_mutex_unlock(UINT32_T mutex)
{
    if( 0 == mutex )
    {
        return;
    }
    rocme_porting_mutex_unlock(mutex);
}

int ResmanNetwork::InitSqliteSettingDB_FAKE(void)
{
	int err = 0;
	
	if(NULL == g_sqlite_setting_DB_fake)
	{
		err = sqlite3_open(SYSDB_SETTINGS_FAKE, &g_sqlite_setting_DB_fake);
	}
	
	if (err == SQLITE_OK)  
    {  
		g_sqlite_setting_num_fake++; //打开成功，次数+1
    }

	return err;
}

void ResmanNetwork::CloseSqliteSettingDB_FAKE(void)
{
	g_sqlite_setting_num_fake--; //当所有打开sqlite的调用者都退出后才关闭数据库
	
	if(NULL != g_sqlite_setting_DB_fake && 0 == g_sqlite_setting_num_fake)
	{
		sqlite3_close(g_sqlite_setting_DB_fake);
		g_sqlite_setting_DB_fake = NULL;
	}
}

void ResmanNetwork::PrintSqliteErrMsg_FAKE(void)
{
	if(g_sqlite_ErrMsg_fake)
	{
		RESC_PRINT("operate fail: %s\n", g_sqlite_ErrMsg_fake);
		sqlite3_free(g_sqlite_ErrMsg_fake);
		g_sqlite_ErrMsg_fake = NULL;
	}
	else
	{
		RESC_PRINT("g_sqlite_ErrMsg_fake is NULL, check if it is 5th para of sqlite3_exec"
			" or 6th para of sqlite3_get_table\n");
	}
}

static void exec_sqlite_cmd(void *param)
{
	char *cmd_str = (char *)param;
	int sys_ret = system(cmd_str);
	
	if(WIFEXITED(sys_ret))
	{
		RESC_DEBUG("normal termination, exit status = %d\n", WEXITSTATUS(sys_ret));
	}
	else if(WIFSIGNALED(sys_ret))
	{
		RESC_DEBUG("abnormal termination, signal number = %d %s\n", 
			WTERMSIG(sys_ret), WCOREDUMP(sys_ret) ? "core dump" : "");
	}
	
	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}
}

int ResmanNetwork::ReadValueFromSqliteDB
	(const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, char* sValue)
{
	int err = 0;
	char select_string[128] = {0};
	sprintf(select_string, "select %s from %s where %s='%s';", sValueName, sTableName, sItemName, sItem);
	RESC_DEBUG("select_string is %s\n", select_string);

	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "sqlite3 -cmd \"%s\" %s > "SQLITE_LOG, select_string, SYSDB_SETTINGS);
	RESC_DEBUG("cmd_str is %s\n", cmd_str);

	UINT32_T task_handle = 
		rocme_porting_task_create((INT8_T *)"sqlite task",&exec_sqlite_cmd,
			cmd_str,ROC_TASK_PRIO_LEVEL_1,1*1024);
	if (task_handle == 0)
	{
		RESC_PRINT("create sqlite task failed\n");
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	kill_task("sqlite3", SIGKILL);
	task_handle = 0;
	
	char path[SYSFS_PATH_MAX] = {0};
	FILE *ifidx	= NULL;
	char tmp_str[128] = {0};

	snprintf(path, SYSFS_PATH_MAX, SQLITE_LOG);
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if(fgets(tmp_str,128, ifidx) != NULL) 
		{
			sscanf(tmp_str, "%s", sValue);
			RESC_PRINT("read Value: %s\n", sValue);

			if(!strncasecmp("SQLite", sValue, 6))
			{
				RESC_PRINT("the %s not exist\n", sItem);
				err = -1;
			}
			else
			{
				err = 0;
			}
		}
		else
		{
			err = -1; 
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
		}
		fclose(ifidx);
	} 
	else 
	{
		err = -1;  
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
	}

	return err;
}

int ResmanNetwork::ReadValueFromSqliteDB_FAKE
	(sqlite3* sqlite_DB, const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, char* sValue)
{
	int err = 0;
	char select_string[128] = {0};
	sprintf(select_string, "select %s from %s where %s='%s';", sValueName, sTableName, sItemName, sItem);
	RESC_DEBUG("select_string is %s\n", select_string);

	int row=0;//用于记录下面结果集中的行数
	char **selectResult = NULL;//二维数组用于存放结果
	err = sqlite3_get_table(sqlite_DB, select_string, &selectResult, &row, 0, &g_sqlite_ErrMsg_fake);
	if (err != SQLITE_OK)  
    {  
        err = -1;  
		PrintSqliteErrMsg_FAKE();
    }

	if(selectResult)
	{
		if(row < 1) //row是除标题栏的行数
		{
			RESC_PRINT("selectResult is empty\n");
			err = -1;  
		}
		else
		{
			sprintf(sValue, "%s", selectResult[1]); //第一行是标题栏，取第2行数据
			RESC_DEBUG("selectResult is %s\n", sValue);
			sqlite3_free_table(selectResult);//释放result的内存空间
			selectResult = NULL;
		}
	}
	else
	{
        err = -1;  
		PrintSqliteErrMsg_FAKE();
	}

	return err;
}

int ResmanNetwork::UpdateValueToSqliteDB
	(const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, const char* sNewValue)
{  
	int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB(sTableName, sItemName, sItem, sValueName, value);
	if(err != 0)
	{	
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", sItem);
		rocme_porting_task_msleep(100);
		
		RESC_DEBUG("now insert the new value\n");
		err = InsertValueToSqliteDB(sTableName, sItem, sNewValue);
		if(err != 0)
		{	
			RESC_PRINT("InsertValueToSqliteDB %s insert failed!\n", sItem);
			return -1;
		}
		return 0;
	}
	
	char update_string[128] = {0};
	sprintf(update_string, "update %s set %s='%s' where %s='%s';", sTableName, sValueName, sNewValue, sItemName, sItem);
	RESC_DEBUG("update_string is %s\n", update_string);

	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "sqlite3 -cmd \"%s\" %s", update_string, SYSDB_SETTINGS);
	RESC_DEBUG("cmd_str is %s\n", cmd_str);

	UINT32_T task_handle = 
		rocme_porting_task_create((INT8_T *)"sqlite task",&exec_sqlite_cmd,
			cmd_str,ROC_TASK_PRIO_LEVEL_1,1*1024);
	if (task_handle == 0)
	{
		RESC_PRINT("create sqlite task failed\n");
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	kill_task("sqlite3", SIGKILL);
	task_handle = 0;
	
    return 0;  
}  

int ResmanNetwork::UpdateValueToSqliteDB_FAKE
	(sqlite3* sqlite_DB, const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, const char* sNewValue)
{  
	int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB_FAKE(sqlite_DB, sTableName, sItemName, sItem, sValueName, value);
	if(err != 0)
	{	
		RESC_PRINT("ReadValueFromSqliteDB_FAKE %s read failed!\n", sItem);
		
		RESC_DEBUG("now insert the new value\n");
		err = InsertValueToSqliteDB_FAKE(sqlite_DB, sTableName, sItem, sNewValue);
		if(err != 0)
		{	
			RESC_PRINT("InsertValueToSqliteDB_FAKE %s insert failed!\n", sItem);
			return -1;
		}
		return 0;
	}
	
	char update_string[128] = {0};
	sprintf(update_string, "update %s set %s='%s' where %s='%s';", sTableName, sValueName, sNewValue, sItemName, sItem);
	RESC_DEBUG("update_string is %s\n", update_string);

	err = sqlite3_exec(sqlite_DB, update_string, 0, 0, &g_sqlite_ErrMsg_fake);
	if (err != SQLITE_OK)  
    {  
        err = -1;  
		PrintSqliteErrMsg_FAKE();
    }
    return err;  
}  

int ResmanNetwork::InsertValueToSqliteDB
	(const char* sTableName, const char* sNewItem, const char* sNewValue)
{  
	int err = 0;
	char insert_string[128] = {0};
	sprintf(insert_string, "insert into %s values(?,'%s','%s');", sTableName, sNewItem, sNewValue);
	RESC_DEBUG("insert_string is %s\n", insert_string);

	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "sqlite3 -cmd \"%s\" %s", insert_string, SYSDB_SETTINGS);
	RESC_DEBUG("cmd_str is %s\n", cmd_str);

	UINT32_T task_handle = 
		rocme_porting_task_create((INT8_T *)"sqlite task",&exec_sqlite_cmd,
			cmd_str,ROC_TASK_PRIO_LEVEL_1,1*1024);
	if (task_handle == 0)
	{
		RESC_PRINT("create sqlite task failed\n");
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	kill_task("sqlite3", SIGKILL);
	task_handle = 0;
	
    return 0;  
}  

int ResmanNetwork::InsertValueToSqliteDB_FAKE
	(sqlite3* sqlite_DB, const char* sTableName, const char* sNewItem, const char* sNewValue)
{  
	int err = 0;
	char insert_string[128] = {0};
	sprintf(insert_string, "insert into %s values(?,'%s','%s');", sTableName, sNewItem, sNewValue);
	RESC_DEBUG("insert_string is %s\n", insert_string);

	err = sqlite3_exec(sqlite_DB, insert_string, 0, 0, &g_sqlite_ErrMsg_fake);
	if (err != SQLITE_OK)  
    {  
        err = -1;  
		PrintSqliteErrMsg_FAKE();
    }
	
    return err;  
}  

int ResmanNetwork::ipv4_netmask_to_prefixLength(const in_addr_t mask) const
{
    int prefixLength = 0;
    uint32_t m = (uint32_t)ntohl(mask);
    while (m & 0x80000000) 
	{
        prefixLength++;
        m = m << 1;
    }
    return prefixLength;
}

const char *ResmanNetwork::ip_to_string(const in_addr_t addr) const
{
    struct in_addr in_addr;

    in_addr.s_addr = addr;
    return inet_ntoa(in_addr);
}

int ResmanNetwork::string_to_ip(const char *string, struct sockaddr_storage *ss) const
{
    struct addrinfo hints, *ai;
    int ret;

    if (ss == NULL) 
	{
        return -EFAULT;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo(string, NULL, &hints, &ai);
    if (ret == 0) 
	{
        memcpy(ss, ai->ai_addr, ai->ai_addrlen);
        freeaddrinfo(ai);
    }

    return ret;
}

INT32_T ResmanNetwork::AddNetEvent(const roc_network_event_callback cbk, void *usrdata, INT32_T *handle)
{
	RESC_FUNC_ENTER;
	
	NetEvtRegNode_t *evttmp = (NetEvtRegNode_t*)malloc(sizeof(NetEvtRegNode_t));
	if (evttmp == NULL)
	{
		RESC_PRINT("no enough memory\n");
		return -1;
	}

	evttmp->cbk = cbk;
	evttmp->usrdata = usrdata;
	evttmp->next = NULL;
	evttmp->handle = g_evt_handle;
	g_evt_handle++;
	
	*handle = evttmp->handle;
	
	net_mutex_lock(g_evt_lock);

	if (g_evt_reg_list == NULL)
	{
		g_evt_reg_list = evttmp;
	}
	else
	{
		NetEvtRegNode_t *lctmp;

		lctmp = g_evt_reg_list;
		while(lctmp->next)
		{
			lctmp = lctmp->next;
		}
		lctmp->next = evttmp;
	}
	
	net_mutex_unlock(g_evt_lock);
	
	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::DeleteNetEvent(const INT32_T handle)
{
	RESC_FUNC_ENTER;
	
	net_mutex_lock(g_evt_lock);
	
	NetEvtRegNode_t *evttmp = NULL,*evttmp1 = NULL;
	if (g_evt_reg_list->handle == (UINT32_T)handle)
	{
		evttmp = g_evt_reg_list;
		g_evt_reg_list = g_evt_reg_list->next;
		free(evttmp);
		
		net_mutex_unlock(g_evt_lock);

		RESC_FUNC_LEAVE;
		return 0;
	}
	else
	{
		evttmp = g_evt_reg_list;
		evttmp1 = g_evt_reg_list->next;
		while(evttmp1)
		{
			if (evttmp1->handle == (UINT32_T)handle)
			{
				evttmp->next = evttmp1->next;
				free(evttmp1);
				
				net_mutex_unlock(g_evt_lock);

				RESC_FUNC_LEAVE;
				return 0;
			}
			else
			{
				evttmp = evttmp1;
				evttmp1 = evttmp1->next;
			}
		}
	}
	
	net_mutex_unlock(g_evt_lock);

	return -1;
}

int ResmanNetwork::FindDevice(const char *ifname)
{
	RESC_FUNC_ENTER;
	
	DIR  *netdir 		= NULL;
	struct dirent *de 	= NULL;
	
	if ((netdir = opendir(SYSFS_CLASS_NET)) != NULL) 
	{
		 while ((de = readdir(netdir))) 
		 {
			if ((!strcmp(de->d_name, ".")) || (!strcmp(de->d_name, "..")))
			{
				continue;
			}

			if(!strcmp(de->d_name, ifname))
			{
				RESC_FUNC_LEAVE;
				return 0;
			}
			
		}
		closedir(netdir);
	}
	else
	{
		RESC_PRINT("%s is not exist\n", SYSFS_CLASS_NET);
	}

	return -1;
}

void ResmanNetwork::RemoveDevNodeFromListByID(const UINT32_T devID)
{
	RESC_FUNC_ENTER;
	
	net_mutex_lock(g_device_lock);
	
	NetworkDeviceNode_t *dev_iterator = g_device_list;
	NetworkDeviceNode_t *devPrev = g_device_list;
	while (dev_iterator)
	{
		if (devID == dev_iterator->devBasicInfo.id)
		{
			//找到Device了,同时devPrev也指向了它的前驱
			//如果只有头结点一个节点，则dev_iterator、devPrev都指向g_device_list
			devPrev->next = dev_iterator->next;
			free(dev_iterator);
			g_device_count--;
			
			net_mutex_unlock(g_device_lock);

			RESC_FUNC_LEAVE;
			return; 
		}
		devPrev = dev_iterator; //保存该节点的前驱节点
		dev_iterator = dev_iterator->next;
	}
    RESC_PRINT("devID=%u not find\n", devID);

	net_mutex_unlock(g_device_lock);
}


int ResmanNetwork::AddDevNodeToList(const Roc_Network_Device_t device)
{
	RESC_FUNC_ENTER;
	
	net_mutex_lock(g_device_lock);
	
	NetworkDeviceNode_t *dev_iterator = g_device_list;
	NetworkDeviceNode_t *devPrev = g_device_list;
	while (dev_iterator)
	{
		if (device.id == dev_iterator->devBasicInfo.id)
		{
			net_mutex_unlock(g_device_lock);
			RESC_PRINT("device has been added, id is %d, devname is %s\n", 
				dev_iterator->devBasicInfo.id, dev_iterator->devBasicInfo.devName);

			RESC_FUNC_LEAVE;
			return 0; //Device已经加入List了
		}
		devPrev = dev_iterator; //保存该节点的前驱节点
		dev_iterator = dev_iterator->next;
	}
	
	//上面循环退出后dev_iterator已经指向最后一个节点的next了
	//如果g_device_list为空，则此时指向的是g_device_list
	/* make some room! */
	NetworkDeviceNode_t *devTmp = 
			(NetworkDeviceNode_t *)calloc(1, sizeof(NetworkDeviceNode_t));
	if (devTmp == NULL) 
	{
		RESC_PRINT("NetworkDeviceNode_t malloc failed: errno is %d, %s\n", errno, strerror(errno));
		net_mutex_unlock(g_device_lock);
		return -1;
	}

	//初始化设备节点信息
	memcpy(&devTmp->devBasicInfo, &device, sizeof(Roc_Network_Device_t));

	int isOpen;
	IsDeviceEnabled(devTmp, &isOpen); //这里更新devNode->device_disable和Android数据库
		
	int err = GetDeviceMode(devTmp, &(devTmp->net_mode));
	if(0 != err)
	{
		RESC_PRINT("GetDeviceMode failed\n");
		devTmp->net_mode = ROC_NET_UNKOWN_MODE;
	}
	RESC_DEBUG("device Name is %s, device_disable is %u, net_mode is %u\n", 
			devTmp->devBasicInfo.devName, devTmp->device_disable, devTmp->net_mode);

	devTmp->noRT_net_mode = ROC_NET_UNKOWN_MODE;
	devTmp->dhcp_begin = 0;
	devTmp->noRT_dnsMode = -1;

	//将该设备加到设备链表末尾
	devTmp->next = NULL;
	if(g_device_list)
	{
		devPrev->next = devTmp;
	}
	else
	{
		g_device_list = devTmp; //设备链表为空
	}
	g_device_count++;
	
	net_mutex_unlock(g_device_lock);
	
	//这个地方和主应用有冲突，他也会设置一遍丢失的ip
	//添加设备节点后再设置丢失的ip，否则节点不存在，会失败
	/*err = set_missing_IP(devTmp);
	if(err != 0)
	{
		RESC_PRINT("set_missing_IP failed\n");
	}*/
	clear_more_IP_onReboot(devTmp); //重启后清除多余未使用iP

	if(!strncmp(devTmp->devBasicInfo.devName, "eth0", 4)) //默认eth0为dhcp的网卡
	{
		dhcpID = devTmp->devBasicInfo.id;
	}

	RESC_FUNC_LEAVE;
	return 0;
}

//该函数参考自Android原生函数netlink_init_interfaces_list
//frameworks/base/core/jni/android_net_ethernet.cpp
int ResmanNetwork::InitDeviceList(void)
{
	RESC_FUNC_ENTER;
	
	char path[SYSFS_PATH_MAX] = {0};
	DIR  *netdir 		= NULL;
	struct dirent *de 	= NULL;
	
	FILE *ifidx			= NULL;
	int index			= 0;
	const int MAX_FGETS_LEN = 4; //android本身管理了一个index,最大长度为4个字节
	char idx[MAX_FGETS_LEN+1] = {0}; 

	if ((netdir = opendir(SYSFS_CLASS_NET)) != NULL) 
	{
		 while ((de = readdir(netdir))) 
		 {
			if ((!strcmp(de->d_name, ".")) || (!strcmp(de->d_name, ".."))
				||(!strcmp(de->d_name, "lo")) || (!strcmp(de->d_name, "wmaster0")) 
				||(!strcmp(de->d_name, "pan0")) || (!strcmp(de->d_name, "ppp0")))
			{
				continue;
			}

			snprintf(path, SYSFS_PATH_MAX,"%s/%s/ifindex", SYSFS_CLASS_NET, de->d_name);
			if ((ifidx = fopen(path, "r")) != NULL) 
			{
				memset(idx, 0, MAX_FGETS_LEN+1);
				if (fgets(idx,MAX_FGETS_LEN, ifidx) != NULL) 
				{
					index = strtoimax(idx, NULL, 10);
				} 
				else 
				{
					RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
				}
				fclose(ifidx);
			} 
			else 
			{
				RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
			}
			
			Roc_Network_Device_t device;
			/* copy the interface name (eth0, eth1, wlan0 ...) */
			strncpy(device.devName, (char *) de->d_name, ROC_MAX_NET_DEVICE_NAME);
			device.id = index;

			device.type = ROC_NET_CONNECT_TYPE_WIRED;
			snprintf(path, SYSFS_PATH_MAX,"%s/%s/phy80211", SYSFS_CLASS_NET, de->d_name);
			if (!access(path, F_OK)) //访问成功
			{
				device.type = ROC_NET_CONNECT_TYPE_WIRELESS;
			}
			else
			{
				snprintf(path, SYSFS_PATH_MAX,"%s/%s/wireless", SYSFS_CLASS_NET, de->d_name);
				if (!access(path, F_OK))
				{
					device.type = ROC_NET_CONNECT_TYPE_WIRELESS;
				}
			}
			// TODO: 不支持ipv6
			device.is_ipv6_supported = ROC_FALSE;
			
			RESC_PRINT("device found: id is %d, devName is %s, type is %d, ipv6_supported is %u\n", 
				device.id, device.devName, device.type, device.is_ipv6_supported);
			
			AddDevNodeToList(device);
		}
		closedir(netdir);
	}
	else
	{
		RESC_PRINT("%s is not exist\n", SYSFS_CLASS_NET);
	}

	RESC_FUNC_LEAVE;
	return 0;
}

void ResmanNetwork::FreeDeviceList(void)
{
	RESC_FUNC_ENTER;
	
	NetworkDeviceNode_t *dev_iterator = g_device_list;
	while (dev_iterator) 
	{
		g_device_list = dev_iterator->next;
		free(dev_iterator);
		dev_iterator = g_device_list;
		g_device_count--;
	}
	
	if (g_device_count) 
	{
		RESC_PRINT("Wrong interface count found\n");
		g_device_count = 0;
	}
	
	RESC_FUNC_LEAVE;
}

INT32_T ResmanNetwork::GetDevIDByName(const CHAR_T* devname, UINT32_T *devID)
{
	RESC_FUNC_ENTER;
	
	NetworkDeviceNode_t *dev_iterator = g_device_list;
	while(dev_iterator)
	{
		if (!strcmp(devname,dev_iterator->devBasicInfo.devName))
		{
			*devID = dev_iterator->devBasicInfo.id;

			RESC_FUNC_LEAVE;
			return 0;
		}
		dev_iterator = dev_iterator->next;
	}
	RESC_PRINT("no device with name %s\n",devname);

	return -1;
}

INT32_T ResmanNetwork::GetDevNodeByID(const UINT32_T devID,NetworkDeviceNode_t **devNode)
{
	RESC_FUNC_ENTER;
	
	NetworkDeviceNode_t *dev_iterator = g_device_list;
	while(dev_iterator)
	{
		if (dev_iterator->devBasicInfo.id == devID)
		{
			*devNode = dev_iterator;
			RESC_PRINT("devID=%d,devname=%s find ok\n",devID,(*devNode)->devBasicInfo.devName);

			RESC_FUNC_LEAVE;
			return 0;
		}
		dev_iterator = dev_iterator->next;
	}
	
    RESC_PRINT("devID=%d,not find\n",devID);
	
	return -1;
}

INT32_T ResmanNetwork::GetAllNetDevice(Roc_Network_Device_t *device, const INT32_T maxcnt, INT32_T* realcnt)
{
	RESC_FUNC_ENTER;
	
	if(NULL == g_device_list)
	{
		RESC_PRINT("g_device_list is NULL, check if init\n");
	}
	
	INT32_T i = 0;
	NetworkDeviceNode_t *dev_iterator = g_device_list;
	while(dev_iterator)
	{
		RESC_DEBUG("device %d: id is %d, devName is %s, type is %d, ipv6_supported is %u\n", 
			i, dev_iterator->devBasicInfo.id, dev_iterator->devBasicInfo.devName, 
			dev_iterator->devBasicInfo.type, dev_iterator->devBasicInfo.is_ipv6_supported);
		
		if (i<maxcnt)
		{
			strcpy(device[i].devName,dev_iterator->devBasicInfo.devName);
			device[i].id = dev_iterator->devBasicInfo.id;
			device[i].type = dev_iterator->devBasicInfo.type;
			i++;
		}
		else
		{
			break;
		}
		dev_iterator = dev_iterator->next;
	}
	*realcnt = i;

	RESC_FUNC_LEAVE;
	return 0;
}

int ResmanNetwork::set_sqliteDB_ethernet_state(const NetworkDeviceNode_t *devNode)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
    int err = 0;

	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, devNode->devBasicInfo.devName))
	{
		RESC_PRINT("the %s is not use now!\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	
	memset(value, 0, VALUE_LEN);
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_on", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_on read failed!\n");
		return -1;
	}

	int last_save_state; 
	last_save_state = strtoimax(value,NULL,10);
	ROC_BOOL last_save_disable; 
	last_save_disable = (last_save_state == 0) ? ROC_TRUE : ROC_FALSE;
	
	if(last_save_disable  == devNode->device_disable)
	{
		RESC_PRINT("device_disable is not change!\n");
		return -1;
	}
	
	rocme_porting_task_msleep(100);

	sprintf(value, "%s", devNode->device_disable ? "0" : "1");
	err = UpdateValueToSqliteDB("secure", "name", "ethernet_on", "value", value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB ethernet_on update failed!\n");
		return -1;
	}

	RESC_FUNC_LEAVE;
	return err;
}

int ResmanNetwork::get_device_flags(NetworkDeviceNode_t *devNode, short *flags)
{
	RESC_FUNC_ENTER;
	
	if(NULL == flags)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	FILE *ifidx = NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/flags", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	const int FLAGS_STRING_LEN = 32; 
	char flag_str[FLAGS_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(flag_str,FLAGS_STRING_LEN, ifidx) != NULL) 
		{
			RESC_PRINT("get flags: %s\n", flag_str);
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	*flags = strtoimax(flag_str+2, NULL, 16);
	RESC_PRINT("get flags: 0x%x\n", *flags);
	
	RESC_FUNC_LEAVE;
    return 0;
}

//This function is deprecated, it needs root privilege
int ResmanNetwork::set_device_flags(NetworkDeviceNode_t *devNode, short flags)
{
	RESC_FUNC_ENTER;
	
	RESC_PRINT("set flags: 0x%x\n", flags);
	
	const int FLAGS_STRING_LEN = 32; 
	char flag_str[FLAGS_STRING_LEN+1] = {0};
	sprintf(flag_str, "0x%x\n", flags);	
		
	FILE *ifidx = NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/flags", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	if ((ifidx = fopen(path, "r+")) != NULL) 
	{
		if (fputs(flag_str, ifidx) >= 0) 
		{
			RESC_PRINT("put flags: %s\n", flag_str);
		} 
		else 
		{
			RESC_PRINT("Can not write %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::DisableDevice(NetworkDeviceNode_t *devNode)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "netcfg %s down", devNode->devBasicInfo.devName);
	int sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}

	devNode->device_disable = ROC_TRUE;
	set_sqliteDB_ethernet_state(devNode);
	
	RESC_FUNC_LEAVE;
    return 0;
}

void ResmanNetwork::print_exit_status(const int status) const 
{
	if(WIFEXITED(status))
	{
		RESC_DEBUG("normal termination, exit status = %d\n", WEXITSTATUS(status));
	}
	else if(WIFSIGNALED(status))
	{
		RESC_DEBUG("abnormal termination, signal number = %d %s\n", 
			WTERMSIG(status), WCOREDUMP(status) ? "core dump" : "");
	}
}

void ResmanNetwork::kill_task(const char* taskname, int signo) const
{
	RESC_FUNC_ENTER;
	
	char path[SYSFS_PATH_MAX] = {0};
	FILE *ifidx	= NULL;
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	char tmp_str[128] = {0};

	sprintf(cmd_str, "ps | grep %s", taskname); 

	ifidx = popen(cmd_str, "r"); //建立管道
    if (!ifidx) 
	{
		RESC_PRINT("popen %s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
        return;
    }

	while(fgets(tmp_str,128, ifidx) != NULL) 
	{
		RESC_DEBUG("tmp_str is %s\n", tmp_str);
		if(strcasestr(tmp_str, taskname))
		{
			int i = 0;
			while(isspace(tmp_str[i])) //跳过开头的空字符
			{
				i++;
			}
			
			while(!isspace(tmp_str[i]))//跳过用户名
			{
				i++;
			}
			
			pid_t pid = strtoimax(tmp_str+i, NULL, 10);
			RESC_DEBUG("pid is %d\n", pid);
			
			if(pid <= 0) //如果pid ==0，则kill会杀死该进程组的所有进程，一个进程组可以查看proc/$pid/task
			{
				continue;
			}

			memset(cmd_str, 0, CMD_STRLEN);
			sprintf(cmd_str, "kill -%d %d", signo, pid); //查看signal.h
			sys_ret = system(cmd_str);
			print_exit_status(sys_ret);
			
			if(0 != WEXITSTATUS(sys_ret))
			{
				RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
			}
		}
		else
		{
			RESC_PRINT("no %s is running\n", taskname);
		}
	} 
	pclose(ifidx);
	
	RESC_FUNC_LEAVE;
}

//和kill_task采用不同实现，是因为如果和ping的临时文件采用同样方法，会堵塞
//ping还需获取最后统计信息，所以kill时发送SIGINT信号
void ResmanNetwork::kill_ping(void) const 
{
	RESC_FUNC_ENTER;
	
	char path[SYSFS_PATH_MAX] = {0};
	FILE *ifidx	= NULL;
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	char tmp_str[128] = {0};

	sprintf(cmd_str, "ps | grep ping > "PS_LOG);
	sys_ret = system(cmd_str);
	print_exit_status(sys_ret);
	
	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}

	snprintf(path, SYSFS_PATH_MAX,PS_LOG);
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		while(fgets(tmp_str,128, ifidx) != NULL) 
		{
			RESC_DEBUG("tmp_str is %s\n", tmp_str);
			if(strcasestr(tmp_str, "ping"))
			{
				int i = 0;
				while(isspace(tmp_str[i])) //跳过开头的空字符
				{
					i++;
				}
				
				while(!isspace(tmp_str[i]))//跳过用户名
				{
					i++;
				}

				int pid = strtoimax(tmp_str+i, NULL, 10);
				RESC_DEBUG("pid is %d\n", pid);
				
				if(pid <= 0)
				{
					continue;
				}

				memset(cmd_str, 0, CMD_STRLEN);
				sprintf(cmd_str, "kill -2 %d", pid);
				sys_ret = system(cmd_str);
				print_exit_status(sys_ret);
				
				if(0 != WEXITSTATUS(sys_ret))
				{
					RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
				}
				
			}
			else
			{
				RESC_PRINT("no ping is running\n");
			}
		} 
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
	}
	
	RESC_FUNC_LEAVE;
}

void ResmanNetwork::setproperty(const char *name, const char* value)
{
	RESC_FUNC_ENTER;
	char prop_name[PROPERTY_KEY_MAX] = {0};
	char prop_value[PROPERTY_VALUE_MAX] = {0};
	strncpy(prop_name, name, PROPERTY_KEY_MAX-1);
	strncpy(prop_value, value, PROPERTY_VALUE_MAX-1);

	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	sprintf(cmd_str, "setprop %s %s", prop_name, prop_value);
	RESC_DEBUG("the cmd str is %s\n", cmd_str);
	
	sys_ret = system(cmd_str); //这边必须采用system或fork或pthread_create，因为do_dhcp可能会阻塞
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}

	RESC_FUNC_LEAVE;
}

/*
 * Wait for a system property to be assigned a specified value.
 * If desired_value is NULL, then just wait for the property to
 * be created with any value. maxwait is the maximum amount of
 * time in seconds to wait before giving up.
 */
static const int NAP_TIME = 200;   /* wait for 200ms at a time */
static int wait_for_property(const char *name, const char *desired_value, int maxwait)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int maxnaps = (maxwait * 1000) / NAP_TIME;

    if (maxnaps < 1) {
        maxnaps = 1;
    }

    while (maxnaps-- > 0) {
        usleep(NAP_TIME * 1000);
        if (property_get(name, value, NULL)) {
            if (desired_value == NULL || 
                    strcmp(value, desired_value) == 0) {
                return 0;
            }
        }
    }
    return -1; /* failure */
}

void ResmanNetwork::start_dhcp(void *param)
{
	RESC_FUNC_ENTER;
	
	ResmanNetwork *tmpNetwork = (ResmanNetwork *)param;
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(tmpNetwork->GetDevNodeByID(tmpNetwork->dhcpID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return;
	}

	char result_prop_name[PROPERTY_KEY_MAX] = {'\0'};
    char daemon_prop_name[PROPERTY_KEY_MAX] = {'\0'};
    char prop_value[PROPERTY_VALUE_MAX] = {'\0'};
    char daemon_cmd[PROPERTY_VALUE_MAX] = {'\0'};
    const char *ctrl_prop = "ctl.start";
	int err = 0;
	Roc_Network_Evt_t evtTmp;

    snprintf(result_prop_name, sizeof(result_prop_name), "dhcp.%s.result",
            devTmp->devBasicInfo.devName);

    snprintf(daemon_prop_name, sizeof(daemon_prop_name), "init.svc.dhcpcd_%s",
		devTmp->devBasicInfo.devName);

    /* Erase any previous setting of the dhcp result property */
    tmpNetwork->setproperty(result_prop_name, "\" \"");
	
    /* Start the daemon and wait until it's ready */
    snprintf(daemon_cmd, sizeof(daemon_cmd), "dhcpcd_%s:%s",
        	devTmp->devBasicInfo.devName, devTmp->devBasicInfo.devName);

    tmpNetwork->setproperty(ctrl_prop, daemon_cmd);
	
    if (wait_for_property(daemon_prop_name, "running", 5) < 0) 
	{
        RESC_PRINT("Timed out waiting for dhcpcd to start\n");
        goto DHCP_FAILED;
    }

    /* Wait for the daemon to return a result */
    if (wait_for_property(result_prop_name, "ok", 10) < 0) 
	{
        RESC_PRINT("Timed out waiting for dhcpcd start finish\n");
        goto DHCP_FAILED;
    }

	devTmp->dhcp_begin = time(NULL);
	
	memset(&evtTmp, 0, sizeof(Roc_Network_Evt_t));
	evtTmp.devID = devTmp->devBasicInfo.id;
	evtTmp.type = ROC_RES_NETWORK_DHCP_READY;
	tmpNetwork->network_event_callback_send(&evtTmp);
	
	rocme_porting_task_msleep(1000); //两个事件间隔太快，页面看不出来
	
	memset(&evtTmp, 0, sizeof(Roc_Network_Evt_t));
	evtTmp.devID = devTmp->devBasicInfo.id;
	evtTmp.type = ROC_RES_NETWORK_DHCP_RENEW_IP;
	tmpNetwork->network_event_callback_send(&evtTmp);
	
	rocme_porting_task_msleep(1000);
	
	memset(&evtTmp, 0, sizeof(Roc_Network_Evt_t));
	evtTmp.devID = devTmp->devBasicInfo.id;
	evtTmp.type = ROC_RES_NETWORK_LINK_UP;
	tmpNetwork->network_event_callback_send(&evtTmp);

	memset(prop_value, 0, PROPERTY_VALUE_MAX);
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.dns1", 
			devTmp->devBasicInfo.devName);
	err = property_get(result_prop_name, prop_value, NULL);
	if(err <= 0)
	{
		RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
	}
	RESC_DEBUG("property_get result is %s\n", prop_value);

	tmpNetwork->setproperty("net.dns1", prop_value);
	
	memset(prop_value, 0, PROPERTY_VALUE_MAX);
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.dns2", 
			devTmp->devBasicInfo.devName);
	err = property_get(result_prop_name, prop_value, NULL);
	if(err <= 0)
	{
		RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
	}
	RESC_DEBUG("property_get result is %s\n", prop_value);

	tmpNetwork->setproperty("net.dns2", prop_value);

	return;

DHCP_FAILED:
	memset(&evtTmp, 0, sizeof(Roc_Network_Evt_t));
	evtTmp.devID = devTmp->devBasicInfo.id;
	evtTmp.type = ROC_RES_NETWORK_DHCP_GET_IP_TIMEOUT;
	tmpNetwork->network_event_callback_send(&evtTmp);
	
	RESC_FUNC_LEAVE;
}

/**
 * Stop the DHCP client daemon.
 */
int ResmanNetwork::dhcp_stop(void)
{
	RESC_FUNC_ENTER;
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(GetDevNodeByID(dhcpID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
    char daemon_prop_name[PROPERTY_KEY_MAX] = {0};
    char daemon_cmd[PROPERTY_VALUE_MAX] = {0};
    const char *ctrl_prop = "ctl.stop";
    const char *desired_status = "stopped";

    snprintf(daemon_prop_name, sizeof(daemon_prop_name), "init.svc.dhcpcd_%s",
		devTmp->devBasicInfo.devName);

    snprintf(daemon_cmd, sizeof(daemon_cmd), "dhcpcd_%s", devTmp->devBasicInfo.devName);

    /* Stop the daemon and wait until it's reported to be stopped */
    setproperty(ctrl_prop, daemon_cmd);
	kill_task("dhcpcd", SIGKILL);
	
    if (wait_for_property(daemon_prop_name, desired_status, 5) < 0) 
	{
        RESC_PRINT("Timed out waiting for dhcpcd to stop\n");
        return -1;
    }
	
    char result_prop_name[PROPERTY_KEY_MAX] = {0};
	char result_addr[PROPERTY_VALUE_MAX] = {0};
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.ipaddress", 
			devTmp->devBasicInfo.devName);
	int err = property_get(result_prop_name, result_addr, NULL);
	if(err <= 0)
	{
		RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
	}
	
	char result_netmask[PROPERTY_VALUE_MAX] = {0};
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.mask", 
			devTmp->devBasicInfo.devName);
	err = property_get(result_prop_name, result_netmask, NULL);
	if(err <= 0)
	{
		RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
	}
	
	struct sockaddr_storage ss;
	memset(&ss, 0, sizeof(struct sockaddr_storage));
	struct sockaddr_in *sin;
	string_to_ip(result_netmask, &ss);
	sin = (struct sockaddr_in *) &ss;
	in_addr_t netMask = sin->sin_addr.s_addr;

    int prefixLength = ipv4_netmask_to_prefixLength(netMask);
	
	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "ip addr del %s/%d dev %s", 
		result_addr, prefixLength, devTmp->devBasicInfo.devName);
	int sys_ret = system(cmd_str);
	print_exit_status(sys_ret);
	
	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}
	
	if(254 == WEXITSTATUS(sys_ret)) //删除失败的时候，ip命令返回值为254
	{
		RESC_PRINT("ip addr del failed\n");
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}

void ResmanNetwork::clear_more_IP_onReboot(NetworkDeviceNode_t *devNode)
{
	RESC_FUNC_ENTER;
	
	int err = 0;
	char result_addr[PROPERTY_VALUE_MAX] = {0};
	char result_prop_name[PROPERTY_KEY_MAX] = {0};
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "network.%s.firstEnter", 
			devNode->devBasicInfo.devName);
	
	err = property_get(result_prop_name, result_addr, NULL);
	if(err <= 0)
	{
		RESC_PRINT("%s called first times\n", devNode->devBasicInfo.devName);
	}
	else
	{
		return;
	}
	setproperty(result_prop_name, "1");
	
	//保留主ip，删除其余多余ip
	for(int i=3; i>0; i--)
	{
		err = set_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName,i, 0, 0);
		if(err != 0)
		{
			RESC_PRINT("set_sqliteDB_ethernet_ipaddr failed\n");
			break;
		}
		
		rocme_porting_task_msleep(100);
	}
}


int ResmanNetwork::set_missing_IP(NetworkDeviceNode_t *devNode)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	int err = 0;
	in_addr_t last_address[4] = {0};
	int last_prefixLength[4] = {0};
	int i=0;

	for(i=0; i<4; ++i)
	{
		err = get_sqliteDB_ethernet_ipaddr
			(devNode->devBasicInfo.devName,i, &last_address[i], &last_prefixLength[i]);
		if(err != 0)
		{
			RESC_PRINT("get_sqliteDB_ethernet_ipaddr failed\n");
		}
		
		if(0 == last_address[i] || 0 == last_prefixLength[i])
		{
			RESC_PRINT("%dth ip addr or prefix is not set yet\n", i);
			break;
		}
	}
	
	in_addr_t last_gateway = 0;
	err = get_sqliteDB_ethernet_gateway(devNode->devBasicInfo.devName, &last_gateway);
	if(err != 0)
	{
		RESC_PRINT("get_sqliteDB_ethernet_gateway failed\n");
	}
	
	const int valid_static_addr_count = i;
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	
	if(ROC_NET_STATIC_MODE == devNode->net_mode)
	{
		RESC_PRINT("static net mode!\n");
		
		if(0 != dhcp_stop())
		{
			RESC_PRINT("dhcp_stop failed\n");
			return -1;
		}
		
		//从前向后添加
		for(i=0; i<valid_static_addr_count; ++i)
		{
			if(0 != last_address[i] && 0 != last_prefixLength[i])
			{
				memset(cmd_str, 0, CMD_STRLEN);
				char addr_str[32] = {0};
				strncpy(addr_str, ip_to_string(last_address[i]), 32);
				
				sprintf(cmd_str, "ip addr add %s/%d dev %s", 
					addr_str, last_prefixLength[i], devNode->devBasicInfo.devName);
				sys_ret = system(cmd_str);
				print_exit_status(sys_ret);
				
				if(0 != WEXITSTATUS(sys_ret))
				{
					RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
				}
			}
				
			rocme_porting_task_msleep(100);
		}
		
		char gway_str[32] = {0};
		strncpy(gway_str, ip_to_string(last_gateway), 32);
		memset(cmd_str, 0, CMD_STRLEN);
		sprintf(cmd_str, "ip route add default via %s dev %s", 
			gway_str, devNode->devBasicInfo.devName);
		
		sys_ret = system(cmd_str);
		print_exit_status(sys_ret);
		
		if(0 != WEXITSTATUS(sys_ret))
		{
			RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
		}
		
		setproperty("net.dns1", "114.114.114.114");
		setproperty("net.dns2", "8.8.8.8");
	}
	else if(ROC_NET_DHCP_MODE == devNode->net_mode)
	{
		RESC_PRINT("DHCP net mode!\n");

		//从后向前删除
		for(i=valid_static_addr_count-1; i>=0; i--)
		{
			if(0 != last_address[i] && 0 != last_prefixLength[i])
			{
				sprintf(cmd_str, "ip addr del %s/%d dev %s", 
					ip_to_string(last_address[i]), last_prefixLength[i], devNode->devBasicInfo.devName);
				sys_ret = system(cmd_str);
				print_exit_status(sys_ret);
				
				if(0 != WEXITSTATUS(sys_ret))
				{
					RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
				}
				
				if(254 == WEXITSTATUS(sys_ret)) //删除失败的时候，ip命令返回值为254
				{
					RESC_PRINT("ip addr del failed\n");
				}
			}
		}

		char gway_str[32] = {0};
		strncpy(gway_str, ip_to_string(last_gateway), 32);
		memset(cmd_str, 0, CMD_STRLEN);
		sprintf(cmd_str, "ip route del default via %s dev %s", 
			gway_str, devNode->devBasicInfo.devName);
		
		sys_ret = system(cmd_str);
		print_exit_status(sys_ret);
		
		if(0 != WEXITSTATUS(sys_ret))
		{
			RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
		}
		
		UINT32_T task_handle = 
			rocme_porting_task_create((INT8_T *)"dhcp task",&(ResmanNetwork::start_dhcp),
				this,ROC_TASK_PRIO_LEVEL_1,10*1024);
		if (task_handle == 0)
		{
			RESC_PRINT("%s create dhcp task failed\n", devNode->devBasicInfo.devName);
			return -1;
		}
	}
	else
	{
		RESC_PRINT("device %s is on pppoe or unkown mode, not support\n", devNode->devBasicInfo.devName);
		return -1;
	}

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::EnableDevice(NetworkDeviceNode_t *devNode)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "netcfg %s up", devNode->devBasicInfo.devName);
	int sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}

	devNode->device_disable = ROC_FALSE;
	set_sqliteDB_ethernet_state(devNode);
	
	//休眠一段时间，网卡启用时netlink可能会进行某些操作，等它完成
	rocme_porting_task_msleep(1500);
	
	int err = set_missing_IP(devNode);
	if(err != 0)
	{
		RESC_PRINT("set_missing_IP failed\n");
		return -1;
	}
	
	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::IsDeviceEnabled(NetworkDeviceNode_t *devNode, ROC_BOOL *isOpen)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}

	int err = 0;
	short flags = 0;
	err = get_device_flags(devNode, &flags);
	if(err != 0)
	{
		RESC_PRINT("get_device_flags failed\n");
		return -1;
	}
	RESC_DEBUG("flags is 0x%x\n", flags);
	
	*isOpen = (flags & IFF_UP) ? ROC_TRUE : ROC_FALSE;
	
	RESC_FUNC_LEAVE;
	return 0;
}

void ResmanNetwork::detect_eth0_connect(void *param)
{
	RESC_FUNC_ENTER;
	
	ResmanNetwork *tmpNetwork = (ResmanNetwork *)param;
	if(NULL == tmpNetwork)
	{
		RESC_PRINT("param is NULL\n");
		return;
	}

	int err = 0;
	UINT32_T devID = 0;
	CHAR_T devname[8] = "eth0";
	err = tmpNetwork->GetDevIDByName(devname, &devID);
	if(err != 0)
	{
		RESC_PRINT("GetDevIDByName failed\n");
		return;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(tmpNetwork->GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return;
	}
	
	Roc_Network_Evt_t evtTmp;
	memset(&evtTmp, 0, sizeof(Roc_Network_Evt_t));
	evtTmp.devID = devTmp->devBasicInfo.id;

	INT32_T last_devstate = 0;
	INT32_T devstate = 0;
	
	while(1)
	{
		err = tmpNetwork->CheckDevStatus(devTmp, &devstate);
		if(0 != err || last_devstate == devstate)
		{
			rocme_porting_task_msleep(3000);
			continue;
		}
		
		RESC_DEBUG("last_devstate is %d, devstate is %d\n", last_devstate, devstate);

		if(0 == last_devstate && 1 == devstate)
		{
			evtTmp.type = ROC_RES_NETWORK_LINK_UP; //其实网线插拔和网卡状态无关，不过调用者希望如此
			tmpNetwork->network_event_callback_send(&evtTmp);

			rocme_porting_task_msleep(1000);
			
			evtTmp.type = ROC_RES_NETWORK_LINE_CONNECTED;
			tmpNetwork->network_event_callback_send(&evtTmp);
		}
		else if(1 == last_devstate && 0 == devstate)
		{
			evtTmp.type = ROC_RES_NETWORK_LINK_DOWN; //其实网线插拔和网卡状态无关，不过调用者希望如此
			tmpNetwork->network_event_callback_send(&evtTmp);
			
			rocme_porting_task_msleep(1000);
			
			evtTmp.type = ROC_RES_NETWORK_LINE_DROPED;
			tmpNetwork->network_event_callback_send(&evtTmp);
		}
		
		last_devstate = devstate;
		rocme_porting_task_msleep(3000);
	}
	
	RESC_FUNC_LEAVE;
}

INT32_T ResmanNetwork::CheckDevStatus(const NetworkDeviceNode_t *devNode, INT32_T *devstate)
{
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	FILE *ifidx	= NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/carrier", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	const int STATE_STRING_LEN = 4;
	char state[STATE_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(state,STATE_STRING_LEN, ifidx) != NULL) 
		{
			*devstate = strtoimax(state, NULL, 10);
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}
	
	return 0;
}

INT32_T ResmanNetwork::GetDevHwaddr(const NetworkDeviceNode_t *devNode, UINT8_T *ptr, int maxlen)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	FILE *ifidx = NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/addr_len", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	int addr_len = 0;
	const int MAC_STRING_LEN = 32; 
	char mac_len[MAC_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(mac_len,MAC_STRING_LEN, ifidx) != NULL) 
		{
			addr_len = strtoimax(mac_len, NULL, 10); 
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	if(6 != addr_len || maxlen < 6)
	{
		RESC_PRINT("mac addr len is not 6 or caller give the length is too short!\n");
		return -1;
	}

	snprintf(path, SYSFS_PATH_MAX,"%s/%s/address", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);
	char mac_addr[MAC_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(mac_addr, MAC_STRING_LEN, ifidx) != NULL) 
		{
			RESC_DEBUG("mac_addr is %s\n", mac_addr);
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	char *p = strtok(mac_addr, ":");
	for(int i = 0; p!=NULL && i<maxlen; i++)
	{
		ptr[i] = strtoimax(p, NULL, 16);
		p = strtok(NULL, ":");
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T ResmanNetwork::GetDevHwSpeed(const NetworkDeviceNode_t *devNode, UINT32_T* rate)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	FILE *ifidx	= NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/speed", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	const int SPEED_STRING_LEN = 8;
	char speed[SPEED_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(speed,SPEED_STRING_LEN, ifidx) != NULL) 
		{
			*rate = 1000*strtoimax(speed, NULL, 10); //单位Kbps
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::GetDevCommuteWay(const NetworkDeviceNode_t *devNode, CHAR_T *commuWay, const INT32_T maxlen)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	RESC_PRINT("first read it from db\n");
	
	int err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
	}
	else
	{
		const int VALUE_LEN = 32;
		char value[VALUE_LEN] = {0};
		err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "secure", "name", 
					"ethernet_ifname_commuteway", "value", value);
		if(0 != err)
		{
			RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname_commuteway read failed!\n");
			err = -1;
			goto Sqlite_CLose;
		}
		else
		{
			err = 0;
			goto Sqlite_CLose;
		}
		
	Sqlite_CLose:	
		CloseSqliteSettingDB_FAKE();
		if(0 == err)
		{
			RESC_PRINT("get commuteway success from db!\n");
			strncpy(commuWay, value, maxlen);
			return 0;
		}
	}
	RESC_PRINT("get commuteway failed from db!\n");
		
	FILE *ifidx	= NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/duplex", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	const int COMMUTE_STRING_LEN = 16; 
	char duplex[COMMUTE_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(duplex,COMMUTE_STRING_LEN, ifidx) == NULL) 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	snprintf(path, SYSFS_PATH_MAX,"%s/%s/speed", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);
	char speed[COMMUTE_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(speed,COMMUTE_STRING_LEN, ifidx) == NULL) 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}

	//去掉speed最后的换行
	int i = 0;
	while(speed[i])
	{
		if('\n' == speed[i] || '\r' == speed[i])
		{
			speed[i] = '\0';
			break;
		}
		++i;
	}
	RESC_DEBUG("speed is %s\n", speed);
	
	char realvalue[32] = {0};
	if(strcasestr(duplex, "adapt"))
	{
		sprintf(realvalue, "adapting");
	}
	else if(strcasestr(duplex, "full"))
	{
		sprintf(realvalue, "%sM_fullDuplex", speed);
	}
	else if(strcasestr(duplex, "half"))
	{
		sprintf(realvalue, "%sM_halfDuplex", speed);
	}
	strncpy(commuWay, realvalue, maxlen);

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::SetDevCommuteWay(NetworkDeviceNode_t *devNode, const CHAR_T *commuWay)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	RESC_PRINT("speed and duplex is not support to set, save it to db\n");

	int err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}
	
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "secure", "name", 
				"ethernet_ifname_commuteway", "value", commuWay);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB ethernet_ifname_commuteway update failed!\n");
		err = -1;
		goto Sqlite_CLose;
	}
	
Sqlite_CLose:	
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::GetPackeages(const NetworkDeviceNode_t *devNode, Roc_Net_Package_Info_t *pstNetPackage)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	FILE *ifidx = NULL;
	char path[SYSFS_PATH_MAX] = {0};
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/statistics/tx_bytes", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);

	const int PACK_STRING_LEN = 16; 
	char tx_bytes[PACK_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(tx_bytes,PACK_STRING_LEN, ifidx) != NULL) 
		{
			pstNetPackage->sentPackages = strtoimax(tx_bytes, NULL, 10); //单位byte
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}
	
	snprintf(path, SYSFS_PATH_MAX,"%s/%s/statistics/rx_bytes", SYSFS_CLASS_NET, devNode->devBasicInfo.devName);
	char rx_bytes[PACK_STRING_LEN+1] = {0};
	if ((ifidx = fopen(path, "r")) != NULL) 
	{
		if (fgets(rx_bytes,PACK_STRING_LEN, ifidx) != NULL) 
		{
			pstNetPackage->receivedPackages = strtoimax(rx_bytes, NULL, 10); //单位byte
		} 
		else 
		{
			RESC_PRINT("Can not read %s: errno is %d, %s\n", path, errno, strerror(errno));
			return -1;
		}
		fclose(ifidx);
	} 
	else 
	{
		RESC_PRINT("Can not open %s: errno is %d, %s\n", path, errno, strerror(errno));
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}


int ResmanNetwork::get_sqliteDB_ethernet_mode(const char *ifname, Roc_NET_Mode_e *net_mode)
{
	RESC_FUNC_ENTER;
	
    int err = 0;

	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, ifname))
	{
		RESC_PRINT("the %s is not use now!\n", ifname);
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	
	memset(value, 0, VALUE_LEN);
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_mode", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_mode read failed!\n");
		return -1;
	}

	if(!strcmp(value, "dhcp"))
	{
		*net_mode = ROC_NET_DHCP_MODE;
	}
	else if(!strcmp(value, "manual"))
	{
		*net_mode = ROC_NET_STATIC_MODE;
	}
	else
	{
		*net_mode = ROC_NET_UNKOWN_MODE;
	}

	RESC_FUNC_LEAVE;
	return err;
}

int ResmanNetwork::set_sqliteDB_ethernet_mode(const char *ifname, Roc_NET_Mode_e net_mode)
{
	RESC_FUNC_ENTER;
	
    int err = 0;

	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, ifname))
	{
		RESC_PRINT("the %s is not use now!\n", ifname);
		return -1;
	}
	
	if(net_mode == ROC_NET_DHCP_MODE)
	{
		sprintf(value, "%s", "dhcp");
	}
	else if(net_mode == ROC_NET_STATIC_MODE)
	{
		sprintf(value, "%s", "manual");
	}
	else
	{
		RESC_PRINT("device %s is on pppoe or unkown mode, not support\n", ifname);
		return -1;
	}
	
	rocme_porting_task_msleep(100);

	err = UpdateValueToSqliteDB("secure", "name", "ethernet_mode", "value", value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB ethernet_mode update failed!\n");
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::GetDeviceMode(const NetworkDeviceNode_t *devNode, Roc_NET_Mode_e *net_mode)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	int err = get_sqliteDB_ethernet_mode(devNode->devBasicInfo.devName, net_mode);
	if(err != 0)
	{
		RESC_PRINT("get_sqliteDB_ethernet_mode %s failed!\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::SetDeviceMode(NetworkDeviceNode_t *devNode, const Roc_NET_Mode_e net_mode)
{
	RESC_FUNC_ENTER;

	if(!g_isRealTime) //延时生效
	{
		g_noRT_devNode = devNode;
		g_noRT_devNode->noRT_net_mode = net_mode;
	
		RESC_FUNC_LEAVE;
		return 0;
	}
		
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
  	Roc_NET_Mode_e last_net_mode;
	int err = get_sqliteDB_ethernet_mode(devNode->devBasicInfo.devName, &last_net_mode);
	if(err != 0)
	{
	  RESC_PRINT("get_sqliteDB_ethernet_mode %s failed!\n", devNode->devBasicInfo.devName);
	  return -1;
	}
  
	if(last_net_mode == net_mode)
	{
		RESC_PRINT("net_mode is not change!\n");

		RESC_FUNC_LEAVE;
		return 0;
	}
	else
	{
		RESC_PRINT("net_mode change!\n");
	}
	
	//先存到netWork对象中，否则set_missing_IP会执行错误的流程
	devNode->net_mode = net_mode;
	err = set_missing_IP(devNode);
	if(err != 0)
	{
		RESC_PRINT("set_missing_IP failed\n");
		devNode->net_mode = last_net_mode;
		return -1;
	}
	
	err = set_sqliteDB_ethernet_mode(devNode->devBasicInfo.devName, net_mode);
	if(err != 0)
	{
		RESC_PRINT("set_sqliteDB_ethernet_mode %s failed!\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}

int ResmanNetwork::get_sqliteDB_ethernet_ipaddr
	(const char *ifname, INT32_T index, in_addr_t *address, int *prefixLength)
{
	RESC_FUNC_ENTER;
	
	if(NULL == ifname)
	{
		RESC_PRINT("ifname is NULL\n");
		return -1;
	}
	
    int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
    struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	char ip_item_name[32] = {0}; 
	char prefix_item_name[32] = {0}; 
	
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, ifname))
	{
		RESC_PRINT("the %s is not use now!\n", ifname);
		return -1;
	}
	
	rocme_porting_task_msleep(100);

	(0 == index) ? sprintf(ip_item_name, "ethernet_ip")
				:sprintf(ip_item_name, "ethernet_ip%d", index);
	
	memset(value, VALUE_LEN, 0);
	err = ReadValueFromSqliteDB("secure", "name",ip_item_name, "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", ip_item_name);
		return -1;
	}
	
	err = string_to_ip(value, &ss);
	if(err != 0)
	{
		RESC_PRINT("string_to_ip failed: errno is %d, %s\n", errno, strerror(errno));
		return -1;
	}
	sin = (struct sockaddr_in *) &ss;
	*address = sin->sin_addr.s_addr;

	rocme_porting_task_msleep(100);

	(0 == index) ? sprintf(prefix_item_name, "ethernet_prefixlength")
				:sprintf(prefix_item_name, "ethernet_prefixlength%d", index);

	memset(value, VALUE_LEN, 0);
	err = ReadValueFromSqliteDB("secure", "name",prefix_item_name, "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", prefix_item_name);
		*prefixLength = 24; //for most case
		err = 0;
	}
	*prefixLength = strtoimax(value,NULL,10);
	
	RESC_FUNC_LEAVE;
	return err;
}

int ResmanNetwork::set_sqliteDB_ethernet_ipaddr
	(const char *ifname, INT32_T index, const in_addr_t address, const int prefixLength)
{
	RESC_FUNC_ENTER;
	
	if(NULL == ifname)
	{
		RESC_PRINT("ifname is NULL\n");
		return -1;
	}
	
    int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	char ip_item_name[32] = {0}; 
	char prefix_item_name[32] = {0}; 
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, ifname))
	{
		RESC_PRINT("the %s is not use now!\n", ifname);
		return -1;
	}
	
	rocme_porting_task_msleep(100);

	(0 == index) ? sprintf(ip_item_name, "ethernet_ip")
				:sprintf(ip_item_name, "ethernet_ip%d", index);
	
	err = UpdateValueToSqliteDB("secure", "name", ip_item_name, "value", ip_to_string(address));
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", prefix_item_name);
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	
	(0 == index) ? sprintf(prefix_item_name, "ethernet_prefixlength")
				:sprintf(prefix_item_name, "ethernet_prefixlength%d", index);
	
	sprintf(value, "%d", prefixLength);
	err = UpdateValueToSqliteDB("secure", "name",prefix_item_name, "value", value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", prefix_item_name);
		return -1;
	}

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::DelIPv4Address(NetworkDeviceNode_t *devNode, INT32_T index, const in_addr_t address, const in_addr_t netMask) 
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	if(ROC_NET_STATIC_MODE != devNode->net_mode)
	{
		RESC_PRINT("device %s is not in static mode, cannot del ip\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	RESC_DEBUG("address is %s, prefixLength is %d\n", ip_to_string(address), ipv4_netmask_to_prefixLength(netMask));

	int err = 0;
	int no_need_replace_front = 0;
	in_addr_t tmp_address = 0;
	int tmp_prefixLength = 0;
	int i = 0;
	
	if(3 == index)
	{
		no_need_replace_front = 1;
	}
	else
	{
		err = get_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName,index+1, &tmp_address, &tmp_prefixLength);
		if(err != 0)
		{
			RESC_PRINT("get_sqliteDB_ethernet_ipaddr failed\n");
		}
		else if(0 == tmp_address && 0 == tmp_prefixLength)
		{
			no_need_replace_front = 1;
		}
	}
	
	if(no_need_replace_front == 1)
	{
		RESC_PRINT("no need replace from front\n");
		rocme_porting_task_msleep(100);
		
		err = set_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName, index, 0, 0);
		if(err != 0)
		{
			RESC_PRINT("set_sqliteDB_ethernet_ipaddr failed\n");
			return -1;
		}
		else
		{
			RESC_PRINT("set_sqliteDB_ethernet_ipaddr success\n");
			goto DELL_ADDR;
		}
	}
	
	for(i = index;i<3;i++)
	{
		err = get_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName,i+1, &tmp_address, &tmp_prefixLength);
		if(err != 0)
		{
			RESC_PRINT("get_sqliteDB_ethernet_ipaddr failed\n");
			return -1;
		}
		
		rocme_porting_task_msleep(100);

		if(0 == tmp_address || 0 == tmp_prefixLength)
		{
			RESC_PRINT("tmp_address or tmp_prefixLength is not set yet\n");
			break;
		}
		
		err = set_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName,i, tmp_address, tmp_prefixLength);
		if(err != 0)
		{
			RESC_PRINT("set_sqliteDB_ethernet_ipaddr failed\n");
			return -1;
		}
		
		rocme_porting_task_msleep(100);
	}

	//清除最后一个ip
	err = set_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName, i, 0, 0);
	if(err != 0)
	{
		RESC_PRINT("set_sqliteDB_ethernet_ipaddr failed\n");
		return -1;
	}

DELL_ADDR:	 //这里放到最后是因为这个ip addr命令几乎不会失败，上面均成功这里才会执行
	char cmd_str[CMD_STRLEN] = {0};
	sprintf(cmd_str, "ip addr del %s/%d dev %s", 
		ip_to_string(address), ipv4_netmask_to_prefixLength(netMask), devNode->devBasicInfo.devName);
	int sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}
	
	if(254 == WEXITSTATUS(sys_ret)) //删除失败的时候，ip命令返回值为254
	{
		RESC_PRINT("ip addr del failed\n");
		return -1;
	}

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::GetIPv4Address(const NetworkDeviceNode_t *devNode, INT32_T index, in_addr_t *address, in_addr_t *netMask)
{
	RESC_FUNC_ENTER;
		
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	int err = 0;
	if(devNode->net_mode == ROC_NET_STATIC_MODE)
	{
		RESC_PRINT("device %s is on static mode\n", devNode->devBasicInfo.devName);

		int prefixLength = 0;
		err = get_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName,index, address, &prefixLength);
		if(err != 0)
		{
			RESC_PRINT("get_sqliteDB_ethernet_ipaddr failed\n");
		}

		if(0 == address || 0 == prefixLength)
		{
			RESC_PRINT("address or prefixLength is not set yet\n");
			return -1;
		}
		*netMask = prefixLengthToIpv4Netmask(prefixLength);
	}
	else if (devNode->net_mode == ROC_NET_DHCP_MODE)
	{
		RESC_PRINT("device %s is on dhcp mode\n", devNode->devBasicInfo.devName);

		if(0 != index)
		{
			RESC_PRINT("dhcp mode only support index 0\n");
			return -1;
		}

		char result_addr[PROPERTY_VALUE_MAX] = {0};
		char result_prop_name[PROPERTY_KEY_MAX] = {0};
		snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.ipaddress", 
				devNode->devBasicInfo.devName);
		int err = property_get(result_prop_name, result_addr, NULL);
		if(err <= 0)
		{
			RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
		}
		struct sockaddr_storage ss;
		memset(&ss, 0, sizeof(struct sockaddr_storage));
		struct sockaddr_in *sin;
		string_to_ip(result_addr, &ss);
		sin = (struct sockaddr_in *) &ss;
		*address = sin->sin_addr.s_addr;

		memset(result_addr, 0, PROPERTY_VALUE_MAX);
		memset(result_prop_name, 0, PROPERTY_KEY_MAX);
		snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.mask", 
				devNode->devBasicInfo.devName);
		err = property_get(result_prop_name, result_addr, NULL);
		if(err <= 0)
		{
			RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
		}
		memset(&ss, 0, sizeof(struct sockaddr_storage));
		string_to_ip(result_addr, &ss);
		sin = (struct sockaddr_in *) &ss;
		*netMask = sin->sin_addr.s_addr;
	}
	else
	{
		RESC_PRINT("device %s is on pppoe or unkown mode, not support\n", 
				devNode->devBasicInfo.devName);
	}
	
	RESC_DEBUG("address is %s\n", ip_to_string(*address));
	RESC_DEBUG("netMask is %s\n", ip_to_string(*netMask));

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::SetIPv4Address(NetworkDeviceNode_t *devNode, INT32_T index, const in_addr_t address, const in_addr_t netMask) 
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	if(ROC_NET_STATIC_MODE != devNode->net_mode)
	{
		RESC_PRINT("device %s is not in static mode, cannot set ip\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	int prefixLength = ipv4_netmask_to_prefixLength(netMask);
	RESC_DEBUG("address is %s\n", ip_to_string(address));
	RESC_DEBUG("netMask is %s, prefixLength is %d\n", ip_to_string(netMask), prefixLength);

	int err = 0;
	in_addr_t last_address = 0; 
	in_addr_t last_netmask = 0; 
	err = GetIPv4Address(devNode,index, &last_address,&last_netmask);
	if(err != 0)
	{
		RESC_PRINT("GetIPv4Address failed\n");
	}
	RESC_DEBUG("last_address is %s\n", ip_to_string(last_address));
	RESC_DEBUG("last_netmask is %s\n", ip_to_string(last_netmask));
	
	//最长匹配原则,如果新的前缀小于等于原来的，则不会起作用
	int last_prefixLength = ipv4_netmask_to_prefixLength(last_netmask);
	if(address == last_address && last_prefixLength > prefixLength)
	{
		RESC_PRINT("address no change and prefixLength to set is less or equal than last one\n");

		RESC_FUNC_LEAVE;
		return 0;	
	}
	else
	{
		RESC_PRINT("address change or prefixLength change to be bigger!\n");
	}
		
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	
	char addr_str[32] = {0};
	strncpy(addr_str, ip_to_string(last_address), 32);

	sprintf(cmd_str, "ip addr del %s/%d dev %s", 
		addr_str, last_prefixLength, devNode->devBasicInfo.devName);
	sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}
	
	err = set_sqliteDB_ethernet_ipaddr(devNode->devBasicInfo.devName,index, address, prefixLength);
	if(err != 0)
	{
		RESC_PRINT("set_sqliteDB_ethernet_ipaddr failed\n");
		return -1;
	}
	
	memset(cmd_str, 0, CMD_STRLEN);
	memset(addr_str, 0, 32);
	strncpy(addr_str, ip_to_string(address), 32);

	sprintf(cmd_str, "ip addr add %s/%d dev %s", 
		addr_str, prefixLength, devNode->devBasicInfo.devName);
	sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}
	
	RESC_FUNC_LEAVE;
    return 0;
}

int ResmanNetwork::get_sqliteDB_ethernet_gateway(const char *ifname, in_addr_t *gateway)
{
	RESC_FUNC_ENTER;
		
	if(NULL == ifname)
	{
		RESC_PRINT("ifname is NULL\n");
		return -1;
	}
	
    int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
    struct sockaddr_storage ss;
	memset(&ss, 0, sizeof(sockaddr_storage));
	struct sockaddr_in *sin;
	
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, ifname))
	{
		RESC_PRINT("the %s is not use now!\n", ifname);
		return -1;
	}
	
	rocme_porting_task_msleep(100);

	memset(value, VALUE_LEN, 0);
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_iproute", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_iproute read failed!\n");
		return -1;
	}
	
	err = string_to_ip(value, &ss);
	if(err != 0)
	{
		RESC_PRINT("string_to_ip failed: errno is %d, %s\n", errno, strerror(errno));
		return -1;
	}
	sin = (struct sockaddr_in *) &ss;
	*gateway = (in_addr_t)sin->sin_addr.s_addr;

	RESC_FUNC_LEAVE;
	return err;
}

int ResmanNetwork::set_sqliteDB_ethernet_gateway(const char *ifname, const in_addr_t gateway)
{
	RESC_FUNC_ENTER;
	
	if(NULL == ifname)
	{
		RESC_PRINT("ifname is NULL\n");
		return -1;
	}
	
    int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB("secure", "name", "ethernet_ifname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ethernet_ifname read failed!\n");
		return -1;
	}
	
	if(0 != strcmp(value, ifname))
	{
		RESC_PRINT("the %s is not use now!\n", ifname);
		return -1;
	}
	
	rocme_porting_task_msleep(100);
	
	err = UpdateValueToSqliteDB("secure", "name", 
				"ethernet_iproute", "value", ip_to_string(gateway));
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB ethernet_iproute update failed!\n");
		return -1;
	}

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::GetDefaultRoute(const NetworkDeviceNode_t *devNode, in_addr_t *gateway)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	int err = 0;
	if(devNode->net_mode == ROC_NET_STATIC_MODE)
	{
		RESC_PRINT("device %s is on static mode\n", devNode->devBasicInfo.devName);

		err = get_sqliteDB_ethernet_gateway(devNode->devBasicInfo.devName, gateway);
		if(err != 0)
		{
			RESC_PRINT("get_sqliteDB_ethernet_gateway failed\n");
		}
	}
	else if (devNode->net_mode == ROC_NET_DHCP_MODE)
	{
		RESC_PRINT("device %s is on dhcp mode\n", devNode->devBasicInfo.devName);

		char route_addr[PROPERTY_VALUE_MAX] = {0};
		char route_prop_name[PROPERTY_KEY_MAX] = {0};
		snprintf(route_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.gateway", 
				devNode->devBasicInfo.devName);
		int err = property_get(route_prop_name, route_addr, NULL);
		if(err <= 0)
		{
			RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
		}
		else
		{
			struct sockaddr_storage ss;
			struct sockaddr_in *sin;
			string_to_ip(route_addr, &ss);
			sin = (struct sockaddr_in *) &ss;
			*gateway = sin->sin_addr.s_addr;
		}
	}
	else
	{
		RESC_PRINT("device %s is on pppoe or unkown mode, not support\n", 
				devNode->devBasicInfo.devName);
	}
	
	RESC_DEBUG("gateway is %s\n", ip_to_string(*gateway));
	
	RESC_FUNC_LEAVE;
    return 0 == *gateway ? -1 : 0;
}

INT32_T ResmanNetwork::SetDefaultRoute(NetworkDeviceNode_t *devNode, const in_addr_t gateway)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	if(ROC_NET_STATIC_MODE != devNode->net_mode)
	{
		RESC_PRINT("device %s is not in static mode, cannot set route\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	RESC_DEBUG("gateway is %s\n", ip_to_string(gateway));
	
	int err = 0;
	in_addr_t last_gateway = 0;
	err = GetDefaultRoute(devNode, &last_gateway);
	if(err != 0)
	{
		RESC_PRINT("GetDefaultRoute failed\n");
	}
	RESC_DEBUG("last_gateway is %s\n", ip_to_string(last_gateway));
	
	if(gateway == last_gateway)
	{
		RESC_PRINT("gateway no change!!\n");

		RESC_FUNC_LEAVE;
		return 0;	
	}
	else
	{
		RESC_PRINT("gateway change!\n");
	}
		
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;

	char gway_str[32] = {0};
	strncpy(gway_str, ip_to_string(last_gateway), 32);
	
	sprintf(cmd_str, "ip route del default via %s dev %s", 
		gway_str, devNode->devBasicInfo.devName);
	
	sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}
	
	rocme_porting_task_msleep(100);
	
	memset(cmd_str, 0, CMD_STRLEN);
	memset(gway_str, 0, 32);
	
	strncpy(gway_str, ip_to_string(gateway), 32);
	sprintf(cmd_str, "ip route add default via %s dev %s", 
		gway_str, devNode->devBasicInfo.devName);
	
	sys_ret = system(cmd_str);
	print_exit_status(sys_ret);

	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
	}

	err = set_sqliteDB_ethernet_gateway(devNode->devBasicInfo.devName, gateway);
	if(err != 0)
	{
		RESC_PRINT("set_sqliteDB_ethernet_gateway failed\n");
	}
	
	RESC_FUNC_LEAVE;
    return 0;
}

//yyyy-mm-dd hh:mm:ss
void ResmanNetwork::time_to_string(struct tm *fmt, char *string)   
{
	sprintf(string,"%4d-%02d-%02d %02d:%02d:%02d",              
            fmt->tm_year + 1900,    /*tm_year,  years since 1900 */       
            fmt->tm_mon + 1,        /* tm_mon, months since January - [0,11] */        
            fmt->tm_mday,       
            fmt->tm_hour,           
            fmt->tm_min,            
            fmt->tm_sec);          

	RESC_PRINT("string is %s\n", string);
}

INT32_T ResmanNetwork::GetDHCPInfo(const NetworkDeviceNode_t *devNode, Roc_Net_DHCP_Info_t *pdhcpInfo)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(devNode->device_disable)
	{
		RESC_PRINT("device %s is disabled\n", devNode->devBasicInfo.devName);
		return -1;
	}

	if(ROC_NET_DHCP_MODE != devNode->net_mode)
	{
		RESC_PRINT("device %s is not in dhcp mode, cannot get dhcp info\n", devNode->devBasicInfo.devName);
		return -1;
	}

	RESC_PRINT("device %s is on dhcp mode\n", devNode->devBasicInfo.devName);
	
	char result[PROPERTY_VALUE_MAX] = {0};
	char result_prop_name[PROPERTY_KEY_MAX] = {0};
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.gateway", 
			devNode->devBasicInfo.devName);
	int err = property_get(result_prop_name, result, NULL);
	if(err <= 0)
	{
		RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
	}
	RESC_DEBUG("property_get result is %s\n", result);
	
	pdhcpInfo->dhcpServer.type = ROC_IP_TYPE_IPv4;
    struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	string_to_ip(result, &ss);
	sin = (struct sockaddr_in *) &ss;
	pdhcpInfo->dhcpServer.ip_add.ipv4_add = sin->sin_addr.s_addr;
	pdhcpInfo->dhcpPort = 67;
	
	memset(result, 0, PROPERTY_VALUE_MAX);
	memset(result_prop_name, 0, PROPERTY_KEY_MAX);
	snprintf(result_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.leasetime", 
			devNode->devBasicInfo.devName);
	err = property_get(result_prop_name, result, NULL);
	if(err <= 0)
	{
		RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
	}
	RESC_DEBUG("property_get result is %s\n", result);
	time_t lease = strtoimax(result, NULL, 10);

	struct tm *tm_begin = localtime(&devNode->dhcp_begin);
	time_to_string(tm_begin, pdhcpInfo->stLeaseTime.leaseObtained);

	time_t dhcp_expires = devNode->dhcp_begin + lease;
	struct tm *tm_end = localtime(&dhcp_expires);
	time_to_string(tm_end, pdhcpInfo->stLeaseTime.leaseExpires);
	
	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T ResmanNetwork::NetPingCancelEx()
{
	RESC_FUNC_ENTER;
	
	if (0 != g_ping_readTask_handle)
	{
		kill_ping();
		rocme_porting_task_msleep(500); //等待ping read完成
		g_ping_readTask_handle = 0;

		RESC_FUNC_LEAVE;
		return 1;
	}
	
	RESC_FUNC_LEAVE;
    return 0;
}

static INT32_T ping_oneTime_callback( UINT32_T handle, Roc_Network_Evt_t *event, void *param)
{
	CHAR_T *result = (CHAR_T *)param;
	strcat(result, (CHAR_T *)event->addData);
	RESC_DEBUG("result is \n%s\n", result);

	return 0;
}

INT32_T ResmanNetwork::NetPingEx(Roc_IP_t address, CHAR_T result[ROC_MAX_PING_RESULT], INT32_T timeout_ms, CHAR_T *parameter)
{
	RESC_FUNC_ENTER;

	g_ping_param.devID = 0;
	memcpy(&g_ping_param.targetaddr,&address,sizeof(Roc_IP_t));
	
	if (parameter == NULL)
	{
		g_ping_param.param[0] = '\0';
	}
	else
	{
		strcpy(g_ping_param.param,parameter);
	}
	
	g_ping_param.timeout_ms = timeout_ms;
	if (g_ping_param.timeout_ms <= 0)
	{
		g_ping_param.timeout_ms = 3000;
	}
	
    g_ping_param.loop = 0;
    g_ping_param.endless = 0;
    if(strlen(g_ping_param.param)==0)
    {
        g_ping_param.loop = 1;
    }
    else if(!strcmp(g_ping_param.param,"-t"))
    {
        g_ping_param.endless = 1;
    }
	
	INT32_T handle = 0;
	if(NULL != result)
	{
		memset(result, 0, ROC_MAX_PING_RESULT);
        g_ping_param.endless = 0;
		g_ping_param.loop = 1;
		AddNetEvent(ping_oneTime_callback, result, &handle);
	}
	
	if(0 == g_ping_param.endless && g_ping_param.loop <= 0)
	{
		RESC_PRINT("**ERROR**: param is wrong!\n");
		return -1;
	}
	
	NetPingCancelEx();
	
	UINT32_T task_handle = 
		rocme_porting_task_create((INT8_T *)"ping task",&(ResmanNetwork::Ping),
				this,ROC_TASK_PRIO_LEVEL_1,10*1024);
	if (task_handle == 0)
	{
		RESC_PRINT("create ping task failed\n");
		return -1;
	}
	
	rocme_porting_task_msleep(1000);
	
	g_ping_readTask_handle = 
		rocme_porting_task_create((INT8_T *)"ping read task",&(ResmanNetwork::ping_read_result),
				this,ROC_TASK_PRIO_LEVEL_1,10*1024);
	if (g_ping_readTask_handle == 0)
	{
		RESC_PRINT("create ping read task failed\n");
		return -1;
	}
	
	if (handle > 0)
	{	
		int max_retry = 3;
		while(g_ping_readTask_handle && max_retry > 0)
		{
			RESC_PRINT("ping readTask has not finish\n");
			max_retry--;
			rocme_porting_task_msleep(1000);
		}
		
		DeleteNetEvent(handle);
		handle = 0;
	}

	RESC_FUNC_LEAVE;
	return 0;
}

void ResmanNetwork::ping_read_result(void *param)
{
	RESC_FUNC_ENTER;
	
	ResmanNetwork *tmpNetwork = (ResmanNetwork *)param;
	if(NULL == tmpNetwork)
	{
		RESC_PRINT("param is NULL\n");
		tmpNetwork->g_ping_readTask_handle = 0;
		return;
	}
	
    Roc_Network_Evt_t evtTmp;
	evtTmp.devID = tmpNetwork->g_ping_param.devID;
	evtTmp.type = ROC_RES_NETWORK_PING_RESPONSE;
	
	if(tmpNetwork->g_ping_fd == NULL)
	{
		RESC_PRINT("g_ping_fd is null, ping has not start\n");
		return;
	}
	
    CHAR_T pingResult[ROC_MAX_PING_RESULT] = {0};
	while(fgets(pingResult, ROC_MAX_PING_RESULT-1, tmpNetwork->g_ping_fd) != NULL)
	{
		if(0 != strlen(pingResult))
		{
			evtTmp.addData = (UINT8_T *)pingResult;
			evtTmp.datalen = (INT32_T)strlen(pingResult);
			tmpNetwork->network_event_callback_send(&evtTmp);
		}
		memset(pingResult, 0, ROC_MAX_PING_RESULT);
	}
	
	pclose(tmpNetwork->g_ping_fd);
	tmpNetwork->g_ping_fd = NULL;

	tmpNetwork->g_ping_readTask_handle = 0;
	RESC_FUNC_LEAVE;
}

void ResmanNetwork::Ping(void *param)
{
	RESC_FUNC_ENTER;
	
	ResmanNetwork *tmpNetwork = (ResmanNetwork *)param;
	if(NULL == tmpNetwork)
	{
		RESC_PRINT("param is NULL\n");
		return;
	}
	
	char dst_addr[32] = {0};
	strncpy(dst_addr, 
		tmpNetwork->ip_to_string(tmpNetwork->g_ping_param.targetaddr.ip_add.ipv4_add), 32);

	if(tmpNetwork->g_ping_fd != NULL)
	{
		RESC_PRINT("g_ping_fd is not null, last ping has not finish\n");
		return;
	}
	
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	if(tmpNetwork->g_ping_param.endless == 1)
	{
		sprintf(cmd_str, "ping -w %d %s", 
			tmpNetwork->g_ping_param.timeout_ms, dst_addr);
	}
	else
	{
		sprintf(cmd_str, "ping -w %d -c %d %s", 
			tmpNetwork->g_ping_param.timeout_ms,
			tmpNetwork->g_ping_param.loop, dst_addr);
	}
	
	tmpNetwork->g_ping_fd = popen(cmd_str, "r"); //建立管道
    if (!tmpNetwork->g_ping_fd) 
	{
		RESC_PRINT("popen %s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));
    }
	
	RESC_FUNC_LEAVE;
}

void ResmanNetwork::network_event_callback_send(Roc_Network_Evt_t *evt)
{
	NetEvtRegNode_t *evttmp;

	net_mutex_lock(g_evt_lock);
	
	evttmp = g_evt_reg_list;
	while(evttmp)
	{
		evttmp->cbk(evttmp->handle,evt,evttmp->usrdata);
		evttmp = evttmp->next;
	}
	
	net_mutex_unlock(g_evt_lock);
}

INT32_T ResmanNetwork::SetHostName(const CHAR_T *host)
{
	RESC_FUNC_ENTER;
	
	int err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}
	
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "secure", "name", 
				"hostname", "value", host);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB hostname update failed!\n");
		err = -1;
		goto Sqlite_CLose;
	}
	
Sqlite_CLose:
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::GetHostName(CHAR_T *host, const INT32_T maxlen)
{
	RESC_FUNC_ENTER;

	int err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}
	
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "secure", "name", 
				"hostname", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB hostname read failed!\n");
		err = -1;
		goto Sqlite_CLose;
	}
	strncpy(host, value, maxlen);
	
Sqlite_CLose:
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::SetWorkGroup(const CHAR_T *workGroup)
{
	RESC_FUNC_ENTER;
	
	int err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}
	
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "secure", "name", 
				"workgroup", "value", workGroup);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB workgroup update failed!\n");
		err = -1;
		goto Sqlite_CLose;
	}
	
Sqlite_CLose:
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::GetWorkGroup(CHAR_T *workGroup, const INT32_T maxlen)
{
	RESC_FUNC_ENTER;
	
	int err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}
	
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "secure", "name", 
				"workgroup", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB workgroup read failed!\n");
		err = -1;
		goto Sqlite_CLose;
	}
	strncpy(workGroup, value, maxlen);
	
Sqlite_CLose:
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T ResmanNetwork::GetDNS
	(const NetworkDeviceNode_t *devNode, const INT32_T index, const ROC_BOOL isIPV4, Roc_IP_t *dnsAddr )
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(index > 2 || index < 0)
	{
		RESC_PRINT("support most 3 dns\n");
		return -1;
	}
	
	if(ROC_TRUE != isIPV4)
	{
		RESC_PRINT("not support ipv6 dns\n");
		return -1;
	}
	
	INT32_T dnsMode;
	int err = GetDNSMode(devNode, &dnsMode);
	if(err != 0)
	{
		RESC_PRINT("GetDNSMode failed\n");
		return -1;
	}
	
	dnsAddr->type = ROC_IP_TYPE_IPv4;

	char dns_addr[PROPERTY_VALUE_MAX] = {0};
	if(1 == dnsMode)
	{
		RESC_PRINT("dns is in auto get state! read from property\n");

		char dns_prop_name[PROPERTY_KEY_MAX] = {0};
		snprintf(dns_prop_name, PROPERTY_KEY_MAX, "dhcp.%s.dns%d", 
				devNode->devBasicInfo.devName, index+1);
	    int err = property_get(dns_prop_name, dns_addr, NULL);
		if(err <= 0)
		{
			RESC_PRINT("property_get failed: errno is %d, %s\n", errno, strerror(errno));
		}
		RESC_DEBUG("property_get result is %s\n", dns_addr);
	}
	else if(0 == dnsMode)
	{
		RESC_PRINT("dns is in static state! read from settings.db\n");
		
		const int VALUE_LEN = 32;
		char dns_item_name[VALUE_LEN] = {0};
		
		snprintf(dns_item_name, VALUE_LEN, "ethernet_dns%d", index+1);
		err = ReadValueFromSqliteDB("secure", "name", dns_item_name, "value", dns_addr);
		if(0 != err)
		{
			RESC_PRINT("ReadValueFromSqliteDB %s read failed\n");
			return -1;
		}
	}
	
    struct sockaddr_storage ss;
	err = string_to_ip(dns_addr, &ss);
	if(err != 0)
	{
		RESC_PRINT("string_to_ip failed: errno is %d, %s\n", errno, strerror(errno));
		return -1;
	}
	struct sockaddr_in *sin = (struct sockaddr_in *) &ss;
	dnsAddr->ip_add.ipv4_add = sin->sin_addr.s_addr;

	RESC_FUNC_LEAVE;
	return 0;	
}

INT32_T ResmanNetwork::SetDNS(NetworkDeviceNode_t *devNode, INT32_T index, const Roc_IP_t dnsAddr)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(index > 2 || index < 0)
	{
		RESC_PRINT("support most 3 dns\n");
		return -1;
	}
	
	int err = 0;
	INT32_T dnsMode;
	err	= GetDNSMode(devNode, &dnsMode);
	if(err != 0)
	{
		RESC_PRINT("GetDNSMode failed\n");
		return -1;
	}

	if(0 != dnsMode)
	{
		RESC_PRINT("dns is not in static state! cannot set manually\n");
		return -1;
	}
	
	char prop_name[PROPERTY_KEY_MAX] = {0};
	sprintf(prop_name, "net.dns%d", index+1);
	setproperty(prop_name, ip_to_string(dnsAddr.ip_add.ipv4_add));

	const int VALUE_LEN = 32;
	char dns_item_name[VALUE_LEN] = {0};
    snprintf(dns_item_name, VALUE_LEN, "ethernet_dns%d", index+1);
	err = UpdateValueToSqliteDB
		("secure", "name", dns_item_name, "value", ip_to_string(dnsAddr.ip_add.ipv4_add));
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", dns_item_name);
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return err;	
}

INT32_T ResmanNetwork::GetDNSMode(const NetworkDeviceNode_t *devNode, INT32_T *dnsMode )
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}

	if(devNode->net_mode == ROC_NET_STATIC_MODE)
	{
		RESC_PRINT("device %s is on static mode\n", devNode->devBasicInfo.devName);
		*dnsMode = 0;
		return 0;
	}
	else if(devNode->net_mode != ROC_NET_DHCP_MODE)
	{
		RESC_PRINT("device %s is not on dhcp or static mode\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	int err = 0;
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	err = ReadValueFromSqliteDB("secure", "name", "dnsmode", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB dnsmode read failed!\n");
		return -1;
	}
	
	if(!strcmp(value, "1"))
	{
		*dnsMode = 1;
	}
	else if(!strcmp(value, "0"))
	{
		*dnsMode = 0;
	}
	else
	{
		RESC_PRINT("dnsmode is %s, wrong\n", value);
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T ResmanNetwork::SetDNSMode(NetworkDeviceNode_t *devNode, INT32_T dnsMode )
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	if(!g_isRealTime) //延时生效
	{
		// TODO: 如果是不同设备，这里是有问题的
		// TODO: 可以使用设备链表遍历检测延时参数
		g_noRT_devNode = devNode; //延时生效应该是操作的同一设备，延时参数之间不影响
		g_noRT_devNode->noRT_dnsMode = dnsMode;
	
		RESC_FUNC_LEAVE;
		return 0;
	}
		
	if(devNode->net_mode != ROC_NET_DHCP_MODE)
	{
		RESC_PRINT("device %s is not on dhcp mode\n", devNode->devBasicInfo.devName);
		return -1;
	}
	
	const int VALUE_LEN = 32;
	char value[VALUE_LEN] = {0};
	if(1 == dnsMode)
	{
		strcpy(value, "1");
	}
	else if(0 == dnsMode)
	{
		strcpy(value, "0");
	}
	else
	{
		RESC_PRINT("dnsMode is %d, wrong\n", dnsMode);
		return -1;
	}
	
	int err = 0;
	err = UpdateValueToSqliteDB("secure", "name","dnsmode", "value", value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB dnsmode update failed!\n");
		return -1;
	}

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::SetProxy(NetworkDeviceNode_t *devNode, const Roc_Proxy_Mode_e proxyMode, const Roc_Proxy_Config_t* configInfo)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}
	
	char proxy_name[32] = {0};
	char proxy_value[256] = {0};
	char proxy_header[16] = {0};
	switch(proxyMode)
	{
		case ROC_PROXY_HTTP: /*!< 使用http proxy*/
			strncpy(proxy_header, "http_proxy", 16);
			break;
		case ROC_PROXY_HTTPS: /*!< 使用https proxy*/
			strncpy(proxy_header, "https_proxy", 16);
			break;
		case ROC_PROXY_FTP: /*!< 使用ftp proxy*/
			strncpy(proxy_header, "ftp_proxy", 16);
			break;
		case ROC_PROXY_NUM:
		defalut:
			RESC_PRINT("proxyMode is wrong\n");
			return -1;
	}
	
	int err = 0;
	int sum_err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}

	sprintf(proxy_name, "%s_enable", proxy_header);
	memset(proxy_value, 0, 256);
	sprintf(proxy_value, "%u", configInfo->proxyEnable);
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", proxy_name);
	}
	sum_err += err;
	
	sprintf(proxy_name, "%s_usrName", proxy_header);
	memset(proxy_value, 0, 256);
	strncpy(proxy_value, configInfo->usrName, 256-1);
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", proxy_name);
	}
	sum_err += err;
	
	sprintf(proxy_name, "%s_password", proxy_header);
	memset(proxy_value, 0, 256);
	strncpy(proxy_value, configInfo->password, 256-1);
	proxy_value[255] = '\0';
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", proxy_name);
	}
	sum_err += err;
	
	sprintf(proxy_name, "%s_server", proxy_header);
	memset(proxy_value, 0, 256);
	strncpy(proxy_value, configInfo->server, 256-1);
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", proxy_name);
	}
	sum_err += err;
	
	sprintf(proxy_name, "%s_port", proxy_header);
	memset(proxy_value, 0, 256);
	sprintf(proxy_value, "%d", configInfo->port);
	err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", proxy_name);
	}
	sum_err += err;
	
	for(int i=0; i<5; ++i)
	{
		sprintf(proxy_name, "%s_unusedProxyURLs%d", proxy_header, i);
		memset(proxy_value, 0, 256);
		strncpy(proxy_value, configInfo->unusedProxyURLs[i], 256-1);
		err = UpdateValueToSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
					proxy_name, "value", proxy_value);
		if(0 != err)
		{
			RESC_PRINT("UpdateValueToSqliteDB %s update failed!\n", proxy_name);
		}
		sum_err += err;
	}
	
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return (0 == sum_err) ? 0 : -1;
}

INT32_T ResmanNetwork::GetProxy(NetworkDeviceNode_t *devNode, const Roc_Proxy_Mode_e proxyMode, Roc_Proxy_Config_t* configInfo)
{
	RESC_FUNC_ENTER;
	
	if(NULL == devNode)
	{
		RESC_PRINT("devNode is NULL\n");
		return -1;
	}

	memset(configInfo, 0, sizeof(Roc_Proxy_Config_t));
	strncpy(configInfo->deviceName, devNode->devBasicInfo.devName, 16);
	configInfo->proxyMode = proxyMode;
	
	char proxy_name[32] = {0};
	char proxy_value[256] = {0};
	char proxy_header[16] = {0};
	switch(proxyMode)
	{
		case ROC_PROXY_HTTP: /*!< 使用http proxy*/
			strncpy(proxy_header, "http_proxy", 16);
			break;
		case ROC_PROXY_HTTPS: /*!< 使用https proxy*/
			strncpy(proxy_header, "https_proxy", 16);
			break;
		case ROC_PROXY_FTP: /*!< 使用ftp proxy*/
			strncpy(proxy_header, "ftp_proxy", 16);
			break;
		case ROC_PROXY_NUM:
		defalut:
			RESC_PRINT("proxyMode is wrong\n");
			return -1;
	}
	
	int err = 0;
	int sum_err = 0;
	err = InitSqliteSettingDB_FAKE();
	if (err != SQLITE_OK)  
	{  
		RESC_PRINT("InitSqliteSettingDB_FAKE failed\n");
		return -1;
	}

	sprintf(proxy_name, "%s_enable", proxy_header);
	memset(proxy_value, 0, 256);
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", proxy_name);
	}
	configInfo->proxyEnable = (UINT8_T)strtoimax(proxy_value, NULL, 10);
	sum_err += err;
	
	sprintf(proxy_name, "%s_usrName", proxy_header);
	memset(proxy_value, 0, 256);
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", proxy_name);
	}
	strncpy(configInfo->usrName, proxy_value, 32-1);
	sum_err += err;
	
	sprintf(proxy_name, "%s_password", proxy_header);
	memset(proxy_value, 0, 256);
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", proxy_name);
	}
	strncpy(configInfo->password, proxy_value, 32-1);
	sum_err += err;
	
	sprintf(proxy_name, "%s_server", proxy_header);
	memset(proxy_value, 0, 256);
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", proxy_name);
	}
	strncpy(configInfo->server, proxy_value, 256-1);
	sum_err += err;
	
	sprintf(proxy_name, "%s_port", proxy_header);
	memset(proxy_value, 0, 256);
	err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
				proxy_name, "value", proxy_value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", proxy_name);
	}
	configInfo->port = (INT32_T)strtoimax(proxy_value, NULL, 10);
	sum_err += err;
	
	for(int i=0; i<5; ++i)
	{
		sprintf(proxy_name, "%s_unusedProxyURLs%d", proxy_header, i);
		memset(proxy_value, 0, 256);
		err = ReadValueFromSqliteDB_FAKE(g_sqlite_setting_DB_fake, "global", "name", 
					proxy_name, "value", proxy_value);
		if(0 != err)
		{
			RESC_PRINT("ReadValueFromSqliteDB %s read failed!\n", proxy_name);
		}
		strncpy(configInfo->unusedProxyURLs[i], proxy_value, 256-1);
		sum_err += err;
	}
	
	CloseSqliteSettingDB_FAKE();
	
	RESC_FUNC_LEAVE;
	return (0 == sum_err) ? 0 : -1;
}

INT32_T ResmanNetwork::SetParamRealTime(ROC_BOOL isRealTime)
{
	RESC_FUNC_ENTER;
    g_isRealTime = isRealTime;
	
	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::GetParamRealTime(ROC_BOOL *isRealTime)
{
	RESC_FUNC_ENTER;
    *isRealTime = g_isRealTime;
	
	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T ResmanNetwork::SetParamCommnit(void)
{
	RESC_FUNC_ENTER;
	int err = 0;
	if(NULL != g_noRT_devNode)
	{
		if(ROC_NET_UNKOWN_MODE != g_noRT_devNode->noRT_net_mode)
		{
			err += SetDeviceMode(g_noRT_devNode, g_noRT_devNode->noRT_net_mode);
		}
		
		if(-1 != g_noRT_devNode->noRT_dnsMode)
		{
			err += SetDNSMode(g_noRT_devNode, g_noRT_devNode->noRT_dnsMode);
		}
	}

	//还原成默认值
	CleanParamRealTimeData();
	
	RESC_FUNC_LEAVE;
	return (0 == err) ? 0 : -1;
}

INT32_T ResmanNetwork::CleanParamRealTimeData(void)
{
	RESC_FUNC_ENTER;
	
	g_isRealTime = true;
	if(NULL != g_noRT_devNode)
	{
		g_noRT_devNode->noRT_net_mode = ROC_NET_UNKOWN_MODE;
		g_noRT_devNode->noRT_dnsMode = -1;
		g_noRT_devNode = NULL;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}

int ResmanNetwork::ntp_update_onReboot(void)
{
	RESC_FUNC_ENTER;
	
	int err = 0;
	strncpy(g_ntp_info.server, "111.13.55.21", 255);
	RESC_DEBUG("ntp server is %s, timeout is %d, interval is %d\n", 
		g_ntp_info.server, g_ntp_info.timeout, g_ntp_info.interval);

	//保存到Json文件中
    CHAR_T ntp_cJson[64] = {0};
	sprintf(ntp_cJson, "{\"address\":\"%s\",\"port\":\"%d\"}", 
							g_ntp_info.server, 123); //ntp默认端口123
	SYS_Prop_Init();
	SYS_Prop_Set(SYS_PROP_KEY_NTP, ntp_cJson);
	SYS_Prop_Save(SYS_PROP_KEY_NTP);
	RESC_DEBUG("SYS_Prop_Save done\n");

	err = NtpUpdate();
	if(err != 0)
	{
		RESC_PRINT("NtpUpdate failed!\n");
		return -1;
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}


int ResmanNetwork::get_sqliteDB_ntpServer(CHAR_T *ntp_server, const INT32_T maxlen)
{
	RESC_FUNC_ENTER;
		
	if(NULL == ntp_server)
	{
		RESC_PRINT("ntp_server is NULL\n");
		return -1;
	}
	
    int err = 0;
	const int VALUE_LEN = 256;
	char value[VALUE_LEN] = {0};
	
	err = ReadValueFromSqliteDB("secure", "name", "ntp_server", "value", value);
	if(0 != err)
	{
		RESC_PRINT("ReadValueFromSqliteDB ntp_server read failed!\n");
		return -1;
	}
	strncpy(ntp_server, value, maxlen);

	RESC_FUNC_LEAVE;
	return err;
}

int ResmanNetwork::set_sqliteDB_ntpServer(const CHAR_T *ntp_server)
{
	RESC_FUNC_ENTER;

	if(NULL == ntp_server)
	{
		RESC_PRINT("ntp_server is NULL\n");
		return -1;
	}
		
    int err = 0;
	const int VALUE_LEN = 256;
	char value[VALUE_LEN] = {0};
	strncpy(value, ntp_server, VALUE_LEN-1);
	
	err = UpdateValueToSqliteDB("secure", "name", "ntp_server", "value", value);
	if(0 != err)
	{
		RESC_PRINT("UpdateValueToSqliteDB ntp_server update failed!\n");
		return -1;
	}

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T ResmanNetwork::SetNtpTimeout(INT32_T Timeout)
{
	RESC_FUNC_ENTER;
    g_ntp_info.timeout = Timeout;

	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::GetNtpTimeout(INT32_T *Timeout)
{
	RESC_FUNC_ENTER;
    *Timeout = g_ntp_info.timeout;
    
	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::SetNtpInterval(INT32_T interval)
{
	RESC_FUNC_ENTER;
    g_ntp_info.interval = interval;
	
	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::GetNtpInterval(INT32_T *interval)
{
	RESC_FUNC_ENTER;
    *interval = g_ntp_info.interval;
    
	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::SetNtpServer(CHAR_T *ntpserver)
{
	RESC_FUNC_ENTER;
	memset(g_ntp_info.server, 0, 256);
    strncpy(g_ntp_info.server, ntpserver, 256-1);
	
	int err = set_sqliteDB_ntpServer(g_ntp_info.server);
	if(err != 0)
	{
		RESC_PRINT("set_sqliteDB_ntpServer failed!\n");
		return -1;
	}

	RESC_FUNC_LEAVE;
    return 0;
}

INT32_T ResmanNetwork::GetNtpServer(CHAR_T *ntpserver, const INT32_T maxlen)
{
	RESC_FUNC_ENTER;
    strncpy(ntpserver, g_ntp_info.server, maxlen);

	RESC_FUNC_LEAVE;
    return 0;
}

void ResmanNetwork::inner_net_ntp_update_exec(void *param)
{
	RESC_FUNC_ENTER;
	
	char cmd_str[CMD_STRLEN] = {0};
	int sys_ret = 0;
	
	Roc_Network_Evt_t evtTmp;
	memset(&evtTmp, 0, sizeof(Roc_Network_Evt_t));
	
	ResmanNetwork *tmpNetwork = (ResmanNetwork *)param;
	if(NULL == tmpNetwork)
	{
		RESC_PRINT("param is NULL\n");
		goto RESET_HANDLE;
	}
	
	tmpNetwork->g_ntp_info.timeout = 
		(tmpNetwork->g_ntp_info.timeout < 15) ? 15 : tmpNetwork->g_ntp_info.timeout;

	tmpNetwork->g_ntp_info.interval =    //根据RFC-4330 最小时间间隔15s
		(tmpNetwork->g_ntp_info.interval < 15) ? 15 : tmpNetwork->g_ntp_info.interval;

	tmpNetwork->g_ntp_info.interval =   //最大时间间隔不能大于超时时间
		(tmpNetwork->g_ntp_info.interval > tmpNetwork->g_ntp_info.timeout) 
			? tmpNetwork->g_ntp_info.timeout : tmpNetwork->g_ntp_info.interval;

	sprintf(cmd_str, "ntpclient -h %s -s -c %d -i %d -d", 
			tmpNetwork->g_ntp_info.server, 
			tmpNetwork->g_ntp_info.timeout/tmpNetwork->g_ntp_info.interval,
			tmpNetwork->g_ntp_info.interval);
	
	RESC_DEBUG("cmd_str is %s\n", cmd_str);
	
	sys_ret = system(cmd_str);
	tmpNetwork->print_exit_status(sys_ret);
	
	RESC_DEBUG("g_ntp_Isfirst is %u\n", tmpNetwork->g_ntp_Isfirst);
	if(0 != WEXITSTATUS(sys_ret))
	{
		RESC_PRINT("%s failed: errno is %d, %s\n", cmd_str, errno, strerror(errno));

		if(tmpNetwork->g_ntp_Isfirst > 1)
		{
			evtTmp.type = ROC_RES_NETWORK_NTP_TIMEOUT;
			tmpNetwork->network_event_callback_send(&evtTmp);
		}
	}
	else
	{
		RESC_PRINT("ntp update success\n");
		
		if(tmpNetwork->g_ntp_Isfirst > 1)
		{
			evtTmp.type = ROC_RES_NETWORK_NTP_SUCCESS;
			tmpNetwork->network_event_callback_send(&evtTmp);
		}
	}

RESET_HANDLE:
    tmpNetwork->g_ntp_task_handle = 0;
	
	RESC_FUNC_LEAVE;
}


INT32_T ResmanNetwork::NtpUpdate(void)
{
	RESC_FUNC_ENTER;
	
	if(NULL == g_ntp_info.server)
	{
		RESC_PRINT("ntp server is null!\n");
		return -1;
	}
	
    if (0 != g_ntp_task_handle)
    {
		RESC_PRINT("ntp task is running!!\n");
		kill_task("ntpclient", SIGTERM);
		g_ntp_task_handle = 0;
    }
	g_ntp_Isfirst++; //如果该值为1，则第一次进入，是程序自身行为，不是调用者行为，不发送消息
	
	g_ntp_task_handle = 
		rocme_porting_task_create((INT8_T *)"ntp task",&(ResmanNetwork::inner_net_ntp_update_exec),
				this,ROC_TASK_PRIO_LEVEL_1,10*1024);
	if (g_ntp_task_handle == 0)
	{
		RESC_PRINT("create ping read task failed\n");
		return -1;
	}
		
	RESC_FUNC_LEAVE;
    return 0;
}




