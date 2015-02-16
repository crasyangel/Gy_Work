/*******************************************************************************
COPYRIGHT (C) 2014    SUMAVISION TECHNOLOGIES CO.,LTD. 

File name   : dvb_client_resman_network_helper.h

Description: 
�ο�frameworks_base\core\jni\android_net_xxx��
\system\core\libnetutils


Project:  android4.2 hi3716c

Date            Modification        Name
----            ------------        ----
2014.12.30      Created             gy
*******************************************************************************/
#ifndef _DVB_CLIENT_RESMAN_NETWORK_HELPER_H
#define _DVB_CLIENT_RESMAN_NETWORK_HELPER_H


/*****************************	Include *******************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>

#include <errno.h>
#include <inttypes.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <sys/wait.h>

#include <linux/icmp.h>
#include <linux/socket.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/route.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>

#include <cutils/properties.h>
#include <netutils/ifc.h>

#include "sqlite3.h"
#include "roc_resman_net_api.h"
#include "rocme_adapter_osp.h"
#include "roc_sys_prop.h"

#ifndef WIFEXITED
#define WIFEXITED(status)	(((status) & 0xff) == 0)
#endif /* !defined WIFEXITED */
#ifndef WEXITSTATUS
#define WEXITSTATUS(status)	(((status) >> 8) & 0xff)
#endif /* !defined WEXITSTATUS */

/*****************************	Define	*******************************************/
#define RESC_PRINT(arg, ...) rocme_porting_dprintf("[resman-c][%s %d]: "arg,__func__,__LINE__,##__VA_ARGS__)

#define NETWORK_DEBUG
#ifdef NETWORK_DEBUG
    #define RESC_DEBUG RESC_PRINT
#else
    #define RESC_DEBUG(arg, ...) 
#endif

#define RESC_FUNC_ENTER RESC_PRINT("enter !!!\n")
#define RESC_FUNC_LEAVE RESC_PRINT("leave !!!\n")

#define MAX_INDEX_COUNT 4
#define SYSFS_PATH_MAX 256
#define SYSFS_CLASS_NET "/sys/class/net"
#define SYSDB_SETTINGS "/data/data/com.android.providers.settings/databases/settings.db"
#define SYSDB_SETTINGS_FAKE "/data/data/com.sumavision.browser/databases/settings.db"
#define INET_ADDRLEN 4
#define INET6_ADDRLEN 16
#define PING_RECV_BUF_LEN 128
#define CMD_STRLEN 512
#define PS_LOG  "/data/data/com.sumavision.browser/ps.log"
#define SQLITE_LOG  "/data/data/com.sumavision.browser/sqlite.log"
#define ADDRESS_LEN 32

/*****************************	Struct Prototype	*******************************************/
//��¼�û�ע��Ļص������Ľṹ��
typedef struct NetEvtRegNode
{
	roc_network_event_callback cbk;      //�ص�����ָ��
	void *usrdata;                       //�û�˽������ָ��
	UINT32_T handle;                     //ע����
	struct NetEvtRegNode *next;          //��һ��ע����Ϣ�ڵ�
}NetEvtRegNode_t;

typedef struct NetworkDeviceNode
{
	Roc_Network_Device_t 		devBasicInfo;   //����������id�����ơ�����
    ROC_BOOL                    device_disable; //ture�����������ڽ���״̬
	Roc_NET_Mode_e				net_mode;
    Roc_NET_Mode_e              noRT_net_mode; //��ʱ��Ч��net_mode
    time_t                      dhcp_begin;
    INT32_T                     noRT_dnsMode; //��ʱ��Ч��dns_mode
	struct NetworkDeviceNode 	*next;
}NetworkDeviceNode_t;

#define PACKETSIZE  52
struct icmppacket
{
    struct icmphdr hdr;
	uint32_t addr;
	struct timeval time;
    char data[PACKETSIZE]; //������icmpЭ��ͷ
};

typedef struct _ping_param
{
    UINT32_T devID;
    Roc_IP_t targetaddr;
    INT32_T  timeout_ms;
    CHAR_T param[10];
    INT32_T loop;
    INT32_T endless;
}_ping_param_t;

typedef struct NtpInfo
{
	CHAR_T server[256];
	INT32_T interval;         //��λ��
	INT32_T timeout;          //��λ��
}NtpInfo_t;

/*****************************	Private Class	*******************************************/
class ResmanNetwork
{
public:
    ResmanNetwork(void);
    virtual ~ResmanNetwork(void);

// TODO: ���ip֧��

public:
	INT32_T AddNetEvent(const roc_network_event_callback cbk, void *usrdata, INT32_T *handle);
	INT32_T DeleteNetEvent(const INT32_T handle);
	INT32_T GetDevIDByName(const CHAR_T* devname, UINT32_T *devID);
	INT32_T GetDevNodeByID(const UINT32_T devID,NetworkDeviceNode_t **devNode);
	INT32_T GetAllNetDevice(Roc_Network_Device_t *device, const INT32_T maxcnt, INT32_T* realcnt);
	INT32_T CheckDevStatus(const NetworkDeviceNode_t *devNode, INT32_T *devstate);
	INT32_T EnableDevice(NetworkDeviceNode_t *devNode);
	INT32_T DisableDevice(NetworkDeviceNode_t *devNode);
	INT32_T IsDeviceEnabled(NetworkDeviceNode_t *devNode, ROC_BOOL *isOpen);
	INT32_T GetDeviceMode(const NetworkDeviceNode_t *devNode, Roc_NET_Mode_e *net_mode);
	INT32_T SetDeviceMode(NetworkDeviceNode_t *devNode, const Roc_NET_Mode_e net_mode);
	INT32_T DelIPv4Address(NetworkDeviceNode_t *devNode, INT32_T index, const in_addr_t address, const in_addr_t netMask); 
	INT32_T GetIPv4Address(const NetworkDeviceNode_t *devNode, INT32_T index, in_addr_t *address, in_addr_t *netMask);
	INT32_T SetIPv4Address(NetworkDeviceNode_t *devNode, INT32_T index, const in_addr_t address, const in_addr_t netMask);
	INT32_T GetDefaultRoute(const NetworkDeviceNode_t *devNode, in_addr_t *gateway);
	INT32_T SetDefaultRoute(NetworkDeviceNode_t *devNode, const in_addr_t gateway);
	INT32_T GetDevHwaddr(const NetworkDeviceNode_t *devNode, UINT8_T *ptr, int maxlen);
	INT32_T GetDevHwSpeed(const NetworkDeviceNode_t *devNode, UINT32_T* rate);
    INT32_T GetPackeages(const NetworkDeviceNode_t *devNode, Roc_Net_Package_Info_t *pstNetPackage);
    INT32_T SetHostName(const CHAR_T *host);
    INT32_T GetHostName(CHAR_T *host, INT32_T maxlen);
    INT32_T SetWorkGroup(const CHAR_T *workGroup);
    INT32_T GetWorkGroup(CHAR_T *workGroup, const INT32_T maxlen);
    INT32_T GetDevCommuteWay(const NetworkDeviceNode_t *devNode, CHAR_T *commuWay, const INT32_T maxlen);
    INT32_T SetDevCommuteWay(NetworkDeviceNode_t *devNode, const CHAR_T *commuWay);
    INT32_T GetDNS(const NetworkDeviceNode_t *devNode, const INT32_T index, const ROC_BOOL isIPV4, Roc_IP_t *dnsAddr );
    INT32_T SetDNS(NetworkDeviceNode_t *devNode, INT32_T index, const Roc_IP_t dnsAddr);
    INT32_T GetDNSMode(const NetworkDeviceNode_t *devNode, INT32_T *dnsMode);
    INT32_T SetDNSMode(NetworkDeviceNode_t *devNode, INT32_T dnsMode);
    INT32_T GetDHCPInfo(const NetworkDeviceNode_t *devNode, Roc_Net_DHCP_Info_t *pdhcpInfo);
    INT32_T NetPingEx(Roc_IP_t address, CHAR_T result[ROC_MAX_PING_RESULT], INT32_T timeout_ms, CHAR_T *parameter);
    INT32_T NetPingCancelEx(void);
    INT32_T SetProxy(NetworkDeviceNode_t *devNode, const Roc_Proxy_Mode_e proxyMode, const Roc_Proxy_Config_t* configInfo);
    INT32_T GetProxy(NetworkDeviceNode_t *devNode, const Roc_Proxy_Mode_e proxyMode, Roc_Proxy_Config_t* configInfo);
    INT32_T SetParamRealTime(ROC_BOOL isRealTime);
    INT32_T GetParamRealTime(ROC_BOOL *isRealTime);
    INT32_T SetParamCommnit(void);
    INT32_T CleanParamRealTimeData(void);
    INT32_T SetNtpTimeout(INT32_T Timeout);
    INT32_T GetNtpTimeout(INT32_T *Timeout);
    INT32_T SetNtpInterval(INT32_T interval);
    INT32_T GetNtpInterval(INT32_T *interval);
    INT32_T SetNtpServer(CHAR_T *ntpserver);
    INT32_T GetNtpServer(CHAR_T *ntpserver, const INT32_T maxlen);
    INT32_T NtpUpdate(void);

public:
    const char *ip_to_string(in_addr_t addr) const;
    int string_to_ip(const char *string, struct sockaddr_storage *ss) const;
    int ipv4_netmask_to_prefixLength(in_addr_t mask) const;
    
private:
    void clear_more_IP_onReboot(NetworkDeviceNode_t *devNode);
    int set_missing_IP(NetworkDeviceNode_t *devNode);
    void time_to_string(struct tm *fmt, char *string);
    int dhcp_stop(void);
        
public:
    static void Ping(void *param);
    static void ping_read_result(void *param);
    static void start_dhcp(void *param);
    static void detect_eth0_connect(void *param);
    static void inner_net_ntp_update_exec(void *param);

private:
    void setproperty(const char *name, const char* value);
    int ntp_update_onReboot(void);
    void network_event_callback_send(Roc_Network_Evt_t *evt);
    void print_exit_status(const int status) const;
    void kill_task(const char* taskname, int signo) const;
    void kill_ping(void) const;

private:
    int get_device_flags(NetworkDeviceNode_t *devNode, short *flags);
    int set_device_flags(NetworkDeviceNode_t *devNode, short flags);

private:
	inline void net_mutex_lock(UINT32_T mutex);
	inline void net_mutex_unlock(UINT32_T mutex);

private:
	void RemoveDevNodeFromListByID(UINT32_T devID);
	int AddDevNodeToList(Roc_Network_Device_t device);
	int InitDeviceList(void);
	void FreeDeviceList(void);
    int FindDevice(const char *ifname);

private:
	int ReadValueFromSqliteDB
		(const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, char* sValue);
    int InsertValueToSqliteDB
        (const char* sTableName, const char* sNewItem, const char* sNewValue);
	int UpdateValueToSqliteDB
		(const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, const char* sNewValue);

private:
    int InitSqliteSettingDB_FAKE(void);
    void CloseSqliteSettingDB_FAKE(void);
    void PrintSqliteErrMsg_FAKE(void);
    int ReadValueFromSqliteDB_FAKE
        (sqlite3* sqlite_DB, const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, char* sValue);
    int UpdateValueToSqliteDB_FAKE
        (sqlite3* sqlite_DB, const char* sTableName, const char* sItemName, const char* sItem, const char* sValueName, const char* sNewValue);
    int InsertValueToSqliteDB_FAKE
        (sqlite3* sqlite_DB, const char* sTableName, const char* sNewItem, const char* sNewValue);

private: 
    //���½ӿ�ֱ�ӵ��������װ��sqliteDB�����ӿڣ��������½ӿ�֮�䲻���������
    //��Ϊ���ڿ�����ĳһ���ӿ��йص������ߴ򿪵�SqliteDB��handle�����²���ʧ��
    int set_sqliteDB_ethernet_state(const NetworkDeviceNode_t *devNode);
    int get_sqliteDB_ethernet_mode(const char *ifname, Roc_NET_Mode_e *net_mode);
    int set_sqliteDB_ethernet_mode(const char *ifname, Roc_NET_Mode_e net_mode);
    int get_sqliteDB_ethernet_ipaddr(const char *ifname, INT32_T index, in_addr_t *address, int *prefixLength);
    int set_sqliteDB_ethernet_ipaddr(const char *ifname, INT32_T index, const in_addr_t address, const int prefixLength);
    int get_sqliteDB_ethernet_gateway(const char *ifname, in_addr_t *gateway);
    int set_sqliteDB_ethernet_gateway(const char *ifname, const in_addr_t gateway);
    int get_sqliteDB_ntpServer(CHAR_T *ntp_server, const INT32_T maxlen);
    int set_sqliteDB_ntpServer(const CHAR_T *ntp_server);

private:
	//����ע��Ļص�������ͷָ��
	NetEvtRegNode_t *g_evt_reg_list;
	
	//�����ص���������Ļ�����
	UINT32_T g_evt_lock;
	
	//����ע��ص������ľ����־
	UINT32_T g_evt_handle;
	
private:
	//�豸��Ϣ����ͷָ��
	NetworkDeviceNode_t *g_device_list;

	//�����豸����Ļ�����
	UINT32_T g_device_lock;

	//ȫ���豸��Ŀ
	INT32_T	g_device_count;	

private: //һ��Ϊ����setting.db_fake����س�Ա������д�������û����Ч�����ߵײ�ֻ��ȥ������Щ����
    //�ڲ�����setting.db_fake��handle
    sqlite3* g_sqlite_setting_DB_fake;
    
    //�ڲ�����setting.db_fake�Ĵ���,���ڿ��ƽ������
    int g_sqlite_setting_num_fake;
    
    //�ڲ�����sqlite_fake�Ĵ�����Ϣ
    char* g_sqlite_ErrMsg_fake;
    
public:
    //ping����Ĳ�����Ϣ
    _ping_param_t g_ping_param;

    //ping read���߳̾��
    UINT32_T g_ping_readTask_handle; 
    
    //ping�������ʱ����ļ�ָ��
    FILE* g_ping_fd;
    
    //dhcp���豸��ʶ��
    UINT32_T dhcpID;

private:
    //ʵʱ��Ч�ı�ʶ��
    bool g_isRealTime;
    
    //��ʱ��Ч��device handle
    NetworkDeviceNode_t *g_noRT_devNode;

public:
    //�Լ������ntpinfo
    NtpInfo_t g_ntp_info;

    //ntp task��handle
    UINT32_T g_ntp_task_handle;
    
    //ntp task�ĵ�һ�ν����־
    UINT32_T g_ntp_Isfirst;
};

#endif //_DVB_CLIENT_RESMAN_NETWORK_HELPER_H

