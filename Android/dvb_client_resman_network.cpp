/*******************************************************************************
COPYRIGHT (C) 2014    SUMAVISION TECHNOLOGIES CO.,LTD. 

File name   : dvb_client_resman_network.cpp

Description: 
RESMAN network模块
暂时没有使用binder服务端/客户端IPC模式

Project:  android4.2 hi3716c

Date            Modification        Name
----            ------------        ----
2014.12.30      Created             gy
*******************************************************************************/

#include "dvb_client_resman_network_helper.h"


/********************** Self Static Network Obj *************************************/
static ResmanNetwork theNetwork;

/*****************************	Function	*******************************************/

INT32_T Roc_Net_Event_Register(roc_network_event_callback cbk, void *usrdata, INT32_T *handle)
{
	RESC_FUNC_ENTER;
	
	if (cbk == NULL)
	{
		RESC_PRINT("the paremeter cbk is a NULL pointer\n");
		return -1;
	}
	
	if (handle == NULL)
	{
		RESC_PRINT("the paremeter handle is a NULL pointer\n");
		return -1;
	}
	
	int err = theNetwork.AddNetEvent(cbk, usrdata, handle);
	RESC_PRINT("handle is %d\n", *handle);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Event_Unregister(INT32_T handle)
{
	RESC_FUNC_ENTER;
	
	if (handle <= 0)
	{
		RESC_PRINT("the parameter handle less than 0 or equal to 0\n");
		return -1;
	}
	
	int err = theNetwork.DeleteNetEvent(handle);
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Get_Device_ID_By_Name(CHAR_T* devname, UINT32_T *devID)
{
	RESC_FUNC_ENTER;
	
	if (devname == NULL)
	{
		RESC_PRINT("the paremeter devname is a NULL pointer\n");
		return -1;
	}
	
	if (devID == NULL)
	{
		RESC_PRINT("the paremeter devID is a NULL pointer\n");
		return -1;
	}
	
	int err = theNetwork.GetDevIDByName(devname, devID);
	RESC_DEBUG("devname is %s, devID is %u\n", devname, *devID);
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Get_Device_Info_By_ID(UINT32_T devID, Roc_Network_Device_t *device)
{
	RESC_FUNC_ENTER;

	if (device == NULL)
	{
		RESC_PRINT("the paremeter device is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	memcpy(device,&(devTmp->devBasicInfo),sizeof(Roc_Network_Device_t));
	RESC_DEBUG("devID is %u, device id is %u\n", devID, device->id);

	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T Roc_Net_Get_Device_Phy_State(INT32_T devID, INT32_T *devstate)
{
	RESC_FUNC_ENTER;
	
    if (NULL == devstate)
    {
		RESC_PRINT("the paremeter devstate is a NULL pointer\n");
        return -1;
    }
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = -1;
    if (devTmp->devBasicInfo.type == ROC_NET_CONNECT_TYPE_WIRED)
	{
		err = theNetwork.CheckDevStatus(devTmp, devstate);
		
    }
    else if (devTmp->devBasicInfo.type == ROC_NET_CONNECT_TYPE_CABLE_MODEM)
    {
        RESC_PRINT("cableModem not support phy state\n");
    }
    else if (devTmp->devBasicInfo.type == ROC_NET_CONNECT_TYPE_WIRELESS)
    {
        RESC_PRINT("wifi not support phy state\n");
    }
	RESC_DEBUG("devID is %u, devstate is %d\n", devID, *devstate);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Device(Roc_Network_Device_t *device, INT32_T maxcnt, INT32_T* realcnt)
{
	RESC_FUNC_ENTER;
	
    if (NULL == device)
    {
		RESC_PRINT("the paremeter device is a NULL pointer\n");
        return -1;
    }
	
    if (NULL == realcnt)
    {
		RESC_PRINT("the paremeter realcnt is a NULL pointer\n");
        return -1;
    }
	
    if (0 == maxcnt)
    {
		RESC_PRINT("the paremeter maxcnt is 0\n");
        return -1;
    }
	
	INT32_T err = theNetwork.GetAllNetDevice(device, maxcnt, realcnt);
	RESC_DEBUG("maxcnt is %d, realcnt is %d\n", maxcnt, *realcnt);
	for(int i=0; i<*realcnt; ++i)
	{
		RESC_DEBUG("device %d: id is %d, devName is %s, type is %d, ipv6_supported is %u\n", 
			i, device[i].id, device[i].devName, device[i].type, device[i].is_ipv6_supported);
	}

	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Enable_Device(UINT32_T devID, ROC_BOOL isOpen)
{
	RESC_FUNC_ENTER;
	RESC_DEBUG("devID is %u, isOpen is %u\n", devID, isOpen);

	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = -1;
	if(isOpen) //启用网卡
	{
		err = theNetwork.EnableDevice(devTmp);
	}
	else	//禁用网卡
	{
		err = theNetwork.DisableDevice(devTmp);
	}
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Is_Device_Enabled(UINT32_T devID, ROC_BOOL *isOpen)
{
	RESC_FUNC_ENTER;
	
    if (NULL == isOpen)
    {
		RESC_PRINT("the paremeter isOpen is a NULL pointer\n");
        return -1;
    }
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	int err = theNetwork.IsDeviceEnabled(devTmp, isOpen);
	RESC_DEBUG("devID is %u, isOpen is %u\n", devID, *isOpen);
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Get_MAC_Addr(UINT32_T devID, UINT8_T mac[6])
{
	RESC_FUNC_ENTER;
	
	if (mac == NULL)
	{
		RESC_PRINT("the paremeter mac is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}

	int err = theNetwork.GetDevHwaddr(devTmp, mac, 6);
	RESC_DEBUG("devID is %u, mac is %x:%x:%x:%x:%x:%x\n", 
			devID, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Data_Rate(UINT32_T devID, UINT32_T* rate)
{
	RESC_FUNC_ENTER;
	
	if (rate == NULL)
	{
		RESC_PRINT("the paremeter rate is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	int err = theNetwork.GetDevHwSpeed(devTmp, rate);
	RESC_DEBUG("devID is %u, rate is %u\n", devID, *rate);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_Commute_Way(UINT32_T devID, CHAR_T *commuWay)
{
	RESC_FUNC_ENTER;
	
	RESC_DEBUG("devID is %u, commuWay is %s\n", devID, commuWay);
	if (commuWay == NULL)
	{
		RESC_PRINT("the paremeter commuWay is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = theNetwork.SetDevCommuteWay(devTmp, commuWay);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Commute_Way(UINT32_T devID, CHAR_T *commuWay,INT32_T maxlen)
{
	RESC_FUNC_ENTER;
	
	if (commuWay == NULL)
	{
		RESC_PRINT("the paremeter commuWay is a NULL pointer\n");
		return -1;
	}
	
	if (maxlen <= 0)
	{
		RESC_PRINT("the paremeter maxlen is 0\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = theNetwork.GetDevCommuteWay(devTmp, commuWay, maxlen);
	RESC_DEBUG("devID is %u, commuWay is %s, maxlen is %d\n", devID, commuWay, maxlen);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Packeages(UINT32_T devID, Roc_Net_Package_Info_t *pstNetPackage)
{
	RESC_FUNC_ENTER;
	
	if (pstNetPackage == NULL)
	{
		RESC_PRINT("the paremeter pstNetPackage is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = theNetwork.GetPackeages(devTmp, pstNetPackage);
	RESC_DEBUG("devID is %u, sentpack is %u, recvpack is %u\n", 
			devID, pstNetPackage->sentPackages, pstNetPackage->receivedPackages);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_Net_Mode(UINT32_T devID, Roc_NET_Mode_e net_mode)
{
	RESC_FUNC_ENTER;
	
    if (ROC_NET_UNKOWN_MODE == net_mode)
    {
		RESC_PRINT("the paremeter net_mode is ROC_NET_UNKOWN_MODE\n");
        return -1;
    }
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	RESC_DEBUG("devID is %u, net_mode is 0x%x\n", devID, net_mode);
	int err = theNetwork.SetDeviceMode(devTmp, net_mode);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Net_Mode(UINT32_T devID, Roc_NET_Mode_e *net_mode, ROC_BOOL is_in_use )
{
	RESC_FUNC_ENTER;
	
    if (NULL == net_mode)
    {
		RESC_PRINT("the paremeter net_mode is a NULL pointer\n");
        return -1;
    }
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}

	if(is_in_use)
	{
		*net_mode = devTmp->net_mode;
	}
	else
	{
		*net_mode = devTmp->noRT_net_mode; 
	}
	
	RESC_DEBUG("devID is %u, net_mode is 0x%x\n", devID, *net_mode);
	
	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T Roc_Net_Get_DHCP_Info(UINT32_T devID, Roc_Net_DHCP_Info_t *pdhcpInfo)
{
	RESC_FUNC_ENTER;
	
    if (NULL == pdhcpInfo)
    {
		RESC_PRINT("the paremeter pdhcpInfo is a NULL pointer\n");
        return -1;
    }
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	if(theNetwork.GetDHCPInfo(devTmp, pdhcpInfo) != 0)
	{
		RESC_PRINT("GetDHCPInfo failed\n");
		return -1;
	}
	
	RESC_DEBUG("dhcpServer type is %u, ipv4_add is %s\n",
		pdhcpInfo->dhcpServer.type, 
		theNetwork.ip_to_string((in_addr_t)pdhcpInfo->dhcpServer.ip_add.ipv4_add));

	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T Roc_Net_Get_IP(UINT32_T devID, INT32_T index, Roc_Net_IP_t *ipCfg)
{
	RESC_FUNC_ENTER;
	
    if (NULL == ipCfg)
    {
		RESC_PRINT("the paremeter ipCfg is a NULL pointer\n");
        return -1;
    }
	
	RESC_DEBUG("devID is %u, index is %d\n", devID, index );
	if(index < 0 || index > 3)
	{
		RESC_PRINT("only support most 3 ip addr, the index is %d\n", index);
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = 0;
	ipCfg->is_ipv6_lastsetted = ROC_FALSE; //暂不支持ipv6
	err = theNetwork.GetIPv4Address(devTmp, index, (in_addr_t *)&ipCfg->ipv4.address, (in_addr_t *)&ipCfg->ipv4.netmask);
	if(0 != err)
	{
		RESC_PRINT("GetIPv4Address failed\n");
		return -1;
	}
	
	//不能在printf中连续使用ip_to_string，确切的说是inet_ntoa
	//inet_ntoa内部实现应该是静态变量，必须调用后立即使用其结果
	//否则最后一次调用的结果会覆盖之前的结果，printf是所有参数计算完再使用的
	RESC_DEBUG("ipv4 address is %s\n", theNetwork.ip_to_string((in_addr_t)ipCfg->ipv4.address));
	RESC_DEBUG("ipv4 netmask is %s\n", theNetwork.ip_to_string((in_addr_t)ipCfg->ipv4.netmask));

	//获取默认路由
	err = theNetwork.GetDefaultRoute(devTmp, (in_addr_t *)(&ipCfg->ipv4.gateway));
	if(0 != err)
	{
		RESC_PRINT("GetDefaultRoute failed\n");
		ipCfg->ipv4.gateway = 0;
	}
	else
	{
		RESC_DEBUG("ipv4 gateway is %s\n", theNetwork.ip_to_string((in_addr_t)ipCfg->ipv4.gateway));
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T Roc_Net_Delete_IP(UINT32_T devID, INT32_T index)
{
	RESC_FUNC_ENTER;
	
	RESC_DEBUG("devID is %u, index is %d\n", devID, index );
	if(index < 0 || index > 3)
	{
		RESC_PRINT("only support most 3 ip addr, the index is %d\n", index);
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = 0;
	in_addr_t address = 0;
	in_addr_t netMask = 0;
	err = theNetwork.GetIPv4Address(devTmp,index, &address, &netMask);
	if(0 != err)
	{
		RESC_PRINT("GetIPv4Address failed\n");
		return -1;
	}
	RESC_DEBUG("address is %s\n", theNetwork.ip_to_string((in_addr_t)address));
	RESC_DEBUG("netmask is %s\n", theNetwork.ip_to_string((in_addr_t)netMask));

	err = theNetwork.DelIPv4Address(devTmp,index, (in_addr_t)address, (in_addr_t)netMask); 
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Set_IP_ipv4(UINT32_T devID, INT32_T index, const Roc_Net_IPv4_t ipCfg)
{
	RESC_FUNC_ENTER;
	
	RESC_DEBUG("devID is %u, index is %d\n", devID, index );
	if(index < 0 || index > 3)
	{
		RESC_PRINT("only support most 3 ip addr, the index is %d\n", index);
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	RESC_DEBUG("ipv4 address is %s\n", theNetwork.ip_to_string((in_addr_t)ipCfg.address));
	RESC_DEBUG("ipv4 netmask is %s\n", theNetwork.ip_to_string((in_addr_t)ipCfg.netmask));
	RESC_DEBUG("ipv4 gateway is %s\n", theNetwork.ip_to_string((in_addr_t)ipCfg.gateway));

	int err = 0;
	if(0 != ipCfg.address && INADDR_NONE != (in_addr_t)(ipCfg.address) && 0 != ipCfg.netmask)
	{
		err = theNetwork.SetIPv4Address(devTmp, index, (in_addr_t)(ipCfg.address), (in_addr_t)(ipCfg.netmask));
	}

	if(0 != err)
	{
		RESC_PRINT("SetIPv4Address failed\n");
		return -1;
	}

	if(0 != index)
	{
		RESC_PRINT("only index 0 can set route\n");
		return 0;
	}
	
	if(0 != ipCfg.gateway && INADDR_NONE != (in_addr_t)(ipCfg.gateway))
	{
		theNetwork.SetDefaultRoute(devTmp, (in_addr_t)(ipCfg.gateway));
	}
	
	RESC_FUNC_LEAVE;
	return 0;
}


INT32_T Roc_Net_Get_Lan_Actual_Status(UINT32_T devID, ROC_BOOL *is_linked)
{
	RESC_FUNC_ENTER;
	
    if (NULL == is_linked)
    {
		RESC_PRINT("the paremeter is_linked is a NULL pointer\n");
        return -1;
    }

	int err = 0;
	INT32_T devstate = 0;
	err = Roc_Net_Get_Device_Phy_State(devID, &devstate);
	if(err != 0)
	{
		return err;
	}
	if(devstate == 0)
	{
		*is_linked = ROC_FALSE;
	}
	else
	{
		*is_linked = ROC_TRUE;
	}
	
	RESC_DEBUG("devID is %u, is_linked is 0x%x\n", devID, *is_linked);

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T Roc_Net_Set_DNS(UINT32_T devID, INT32_T index, const Roc_IP_t dnsAddr)
{
	RESC_FUNC_ENTER;
	
	if(ROC_IP_TYPE_IPv4 != dnsAddr.type)
	{
		RESC_PRINT("not support ipv6 dns\n");
		return -1;
	}

	if(0 == dnsAddr.ip_add.ipv4_add || INADDR_NONE == dnsAddr.ip_add.ipv4_add)
	{
		RESC_PRINT("invalid ipv4 dns address\n");
		return -1;
	}
		
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	RESC_DEBUG("devID is %u, index is %d, dnsAddr is %s\n", devID, index, 
		theNetwork.ip_to_string((in_addr_t)dnsAddr.ip_add.ipv4_add));
	int err = theNetwork.SetDNS(devTmp, index, dnsAddr);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_DNS(UINT32_T devID, INT32_T index, ROC_BOOL isIPV4, Roc_IP_t *dnsAddr )
{
	RESC_FUNC_ENTER;
	
    if (NULL == dnsAddr)
    {
		RESC_PRINT("the paremeter dnsAddr is a NULL pointer\n");
        return -1;
    }

	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = theNetwork.GetDNS(devTmp, index, isIPV4, dnsAddr);
	RESC_DEBUG("devID is %u, index is %d, isIPV4 is %s, dnsAddr is %s\n", 
			devID, index, ROC_TRUE==isIPV4?"true":"false", 
			theNetwork.ip_to_string((in_addr_t)dnsAddr->ip_add.ipv4_add));
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Set_DNS_Mode(UINT32_T devID, INT32_T dnsMode )
{
	RESC_FUNC_ENTER;
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	RESC_DEBUG("devID is %u, dnsMode is %d\n",devID, dnsMode);
	int err = theNetwork.SetDNSMode(devTmp, dnsMode);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_DNS_Mode(UINT32_T devID, INT32_T *dnsMode )
{
	RESC_FUNC_ENTER;
	
    if (NULL == dnsMode)
    {
		RESC_PRINT("the paremeter dnsMode is a NULL pointer\n");
        return -1;
    }

	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = theNetwork.GetDNSMode(devTmp, dnsMode);
	RESC_DEBUG("devID is %u, dnsMode is %d\n",devID, *dnsMode);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_PingEx(Roc_IP_t address, CHAR_T result[ROC_MAX_PING_RESULT], INT32_T timeout_ms, CHAR_T *parameter)
{
	RESC_FUNC_ENTER;
	
	if(address.type != ROC_IP_TYPE_IPv4)
	{
		RESC_PRINT("only support ipv4\n");
		return -1;
	}
	
    if (0 == address.ip_add.ipv4_add)
    {
		RESC_PRINT("target ipv4_add is 0\n");
        return -1;
    }

	int err = theNetwork.NetPingEx(address, result, timeout_ms, parameter);
	RESC_DEBUG("result is \n%s\n", result);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Ping_CancelEx()
{
	RESC_FUNC_ENTER;
	
	int err = theNetwork.NetPingCancelEx();

	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Set_Host(CHAR_T *host)
{
	RESC_FUNC_ENTER;
	
	if (host == NULL)
	{
		RESC_PRINT("the paremeter host is a NULL pointer\n");
		return -1;
	}
	
	RESC_DEBUG("host is %s\n", host);
	int err = theNetwork.SetHostName(host);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Host(CHAR_T *host, INT32_T maxlen)
{
	RESC_FUNC_ENTER;
	
	if (host == NULL)
	{
		RESC_PRINT("the paremeter host is a NULL pointer\n");
		return -1;
	}
	
	if (0 == maxlen)
	{
		RESC_PRINT("the paremeter maxlen is 0\n");
		return -1;
	}
	
	int err = theNetwork.GetHostName(host, maxlen);
	RESC_DEBUG("host is %s\n", host);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_WorkGroup(CHAR_T *workGroup)
{
	RESC_FUNC_ENTER;
	
	if (workGroup == NULL)
	{
		RESC_PRINT("the paremeter workGroup is a NULL pointer\n");
		return -1;
	}
	
	RESC_DEBUG("workGroup is %s\n", workGroup);
	int err = theNetwork.SetWorkGroup(workGroup);

	RESC_FUNC_LEAVE;
	return 0;
}

INT32_T Roc_Net_Get_WorkGroup(CHAR_T *workGroup, INT32_T maxlen)
{
	RESC_FUNC_ENTER;
	
	if (workGroup == NULL)
	{
		RESC_PRINT("the paremeter workGroup is a NULL pointer\n");
		return -1;
	}

	if (0 == maxlen)
	{
		RESC_PRINT("the paremeter maxlen is 0\n");
		return -1;
	}
	
	int err = theNetwork.GetWorkGroup(workGroup, maxlen);
	RESC_DEBUG("workGroup is %s\n", workGroup);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_Proxy(UINT32_T devID, Roc_Proxy_Mode_e proxyMode, Roc_Proxy_Config_t* configInfo)
{
	RESC_FUNC_ENTER;
	
	if (configInfo == NULL)
	{
		RESC_PRINT("the paremeter configInfo is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	RESC_DEBUG("proxyMode is %d, proxyEnable is %u, usrName is %s, password is %s, "
		"server is %s, port is %d, unusedProxyURLs is \n%s\n%s\n%s\n%s\n%s\n", 
		proxyMode, configInfo->proxyEnable, configInfo->usrName, configInfo->password, 
		configInfo->server, configInfo->port, configInfo->unusedProxyURLs[0], configInfo->unusedProxyURLs[1],
		configInfo->unusedProxyURLs[2], configInfo->unusedProxyURLs[3], configInfo->unusedProxyURLs[4]);

	int err = theNetwork.SetProxy(devTmp, proxyMode, configInfo);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Proxy(UINT32_T devID, Roc_Proxy_Mode_e proxyMode, Roc_Proxy_Config_t* configInfo)
{
	RESC_FUNC_ENTER;
	
	if (configInfo == NULL)
	{
		RESC_PRINT("the paremeter configInfo is a NULL pointer\n");
		return -1;
	}
	
	NetworkDeviceNode_t *devTmp = NULL;
	if(theNetwork.GetDevNodeByID(devID,&devTmp) != 0)
	{
		RESC_PRINT("The device with the id doesn't exist\n");
		return -1;
	}
	
	int err = theNetwork.GetProxy(devTmp, proxyMode, configInfo);
	RESC_DEBUG("proxyMode is %d, proxyEnable is %u, usrName is %s, password is %s, "
		"server is %s, port is %d, unusedProxyURLs is \n%s\n%s\n%s\n%s\n%s\n", 
		proxyMode, configInfo->proxyEnable, configInfo->usrName, configInfo->password, 
		configInfo->server, configInfo->port, configInfo->unusedProxyURLs[0], configInfo->unusedProxyURLs[1],
		configInfo->unusedProxyURLs[2], configInfo->unusedProxyURLs[3], configInfo->unusedProxyURLs[4]);
	
	RESC_FUNC_LEAVE;
	return err;
}


INT32_T Roc_Net_Set_NTP_Timeout(INT32_T Timeout)
{
	RESC_FUNC_ENTER;
	
	if (Timeout == 0)
	{
		RESC_PRINT("the paremeter Timeout is 0\n");
		return -1;
	}
	
	RESC_DEBUG("Timeout is %d\n", Timeout);
	int err = theNetwork.SetNtpTimeout(Timeout);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_NTP_Timeout(INT32_T *Timeout)
{
	RESC_FUNC_ENTER;
	
	if (Timeout == NULL)
	{
		RESC_PRINT("the paremeter Timeout is NULL\n");
		return -1;
	}
	
	int err = theNetwork.GetNtpTimeout(Timeout);
	RESC_DEBUG("Timeout is %d\n", *Timeout);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_NTP_Interval(INT32_T interval)
{
	RESC_FUNC_ENTER;
	
	if (interval == 0)
	{
		RESC_PRINT("the paremeter interval is 0\n");
		return -1;
	}
	
	RESC_DEBUG("interval is %d\n", interval);
	int err = theNetwork.SetNtpInterval(interval);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_NTP_Interval(INT32_T *interval)
{
	RESC_FUNC_ENTER;
	
	if (interval == NULL)
	{
		RESC_PRINT("the paremeter interval is NULL\n");
		return -1;
	}
	
	int err = theNetwork.GetNtpInterval(interval);
	RESC_DEBUG("interval is %d\n", *interval);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_NTP_Server(CHAR_T *ntpserver)
{
	RESC_FUNC_ENTER;
	
	if (ntpserver == NULL)
	{
		RESC_PRINT("the paremeter ntpserver is NULL\n");
		return -1;
	}
	
	if(0 == strlen(ntpserver))
	{
		RESC_PRINT("the paremeter ntpserver has no character\n");
		return -1;
	}

	RESC_DEBUG("ntpserver is %s\n", ntpserver);
	int err = theNetwork.SetNtpServer(ntpserver);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_NTP_Server(CHAR_T *ntpserver, INT32_T maxlen)
{
	RESC_FUNC_ENTER;
	
	if (ntpserver == NULL)
	{
		RESC_PRINT("the paremeter ntpserver is NULL\n");
		return -1;
	}
	if (maxlen == 0)
	{
		RESC_PRINT("the paremeter maxlen is 0\n");
		return -1;
	}
	
	int err = theNetwork.GetNtpServer(ntpserver, maxlen);
	RESC_DEBUG("ntpserver is %s\n", ntpserver);
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_NTP_Update()
{
	RESC_FUNC_ENTER;
	
	int err = theNetwork.NtpUpdate();
	
	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_Param_RealTime(ROC_BOOL isRealTime)
{
	RESC_FUNC_ENTER;
	RESC_DEBUG("isRealTime is %u\n", isRealTime);
	int err = theNetwork.SetParamRealTime(isRealTime);

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Get_Param_RealTime(ROC_BOOL *isRealTime)
{
	RESC_FUNC_ENTER;
	int err = theNetwork.GetParamRealTime(isRealTime);
	RESC_DEBUG("isRealTime is %u\n", *isRealTime);

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Set_Param_Commnit()
{
	RESC_FUNC_ENTER;
	int err = theNetwork.SetParamCommnit();

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Clean_Param_RealTime_Data()
{
	RESC_FUNC_ENTER;
	int err = theNetwork.CleanParamRealTimeData();

	RESC_FUNC_LEAVE;
	return err;
}

INT32_T Roc_Net_Cfg_Save()
{

	return 0;
}


INT32_T Roc_Net_WIFI_Start_Scan(UINT32_T devID, INT32_T maxNum, INT32_T timeout)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Get_Aps(Roc_Wireless_AP_Info_t *aps, UINT8_T *ap_num)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Connect(UINT32_T devID, UINT32_T configID)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Disconnect(UINT32_T devID)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Get_Connect_APInfo(UINT32_T devID, Roc_Wireless_AP_Info_t* info)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Get_ConnectInfo(UINT32_T devID, Roc_Wifi_ConnectInfo_t* info)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Get_ConnectState(UINT32_T devID, Roc_Net_WIFI_State_e* state)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Get_Avail_Config(Roc_WIFI_Connection_Config_t *configs, UINT8_T *config_num)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Add_Wifi_Config(INT8_T *key, INT32_T keyLen, UINT32_T configID)
{

	return 0;
}

INT32_T Roc_Net_WIFI_Remove_Wifi_Config(UINT32_T configID)
{

	return 0;
}

INT32_T Roc_Net_WIFI_SaveConfig()
{

	return 0;
}


INT32_T Roc_Net_Get_CM_State(Roc_Net_CM_Status_e *state)
{

	return 0;
}

INT32_T Roc_Net_Set_IP_ipv6(UINT32_T devID, INT32_T index, const Roc_Net_IPv6_t ipCfg)
{

	return 0;
}












