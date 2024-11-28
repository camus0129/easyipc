#ifndef __easyipc_base_h__
#define __easyipc_base_h__
#ifdef __cplusplus
extern "C" {
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <sys/sysinfo.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h> 
#include <netinet/in.h>
//#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/un.h>
#include "dlist.h"

#define eipc_version "1.0.0.9"
#define SOCKET_PATH "/tmp/unix_socket"

typedef enum
{
	ENUM_IPC_LOG_LEVEL_ERR=0,		// 记录一些错误,比如程序意外退出,消息严重阻塞等等
	ENUM_IPC_LOG_LEVEL_WARN,		// 主要记录一些无法处理的消息等,但是不影响整体的运行,比如某个消息处理时间过长,重复注册等问题
	ENUM_IPC_LOG_LEVEL_NORMAL,		// 主要用于记录消息走向,包含了发送,处理完成等等
	ENUM_IPC_LOG_LEVEL_INFO,		// 主要用于消息的注册,取消注册等信息,开始处理消息等
	ENUM_IPC_LOG_LEVEL_DBG			// 用于用户程序DBG使用
}IPC_LOG_LEVEL;


#define IPC_CONF_FILE_PATH 	"/etc/easyipc.conf"


#define PROCESS_NAME_MAX_SIZE 64		// IPC handle命名最大长度
#define IPC_MSG_MAX_SIZE 64				// IPC msg名称最大长度
#define RUNTIME_LOG_MAX_SIZE 128		// log最大字节数
#define IPC_MAX_PACKET 4096				// IPC包最大负载
#define IPC_LOG_SAVE_PATH_MAX_LENS 256	//存储log路径最大长度
#define IPC_DEBUG_SAVE_PATH "/tmp/easyipc.log"	//默认log存放路径
#define IPC_PLUGIN_MSG_MAX_SIZE 256
#define IPC_SYSCMD_SIZE 256
#define IPC_CMDLINE_MAX_SIZE 4096


#define enable_syslog 0




typedef struct _easyipc_config
{
	int ipc_msg_packet_max_size;
	char ipc_log_save_path[IPC_LOG_SAVE_PATH_MAX_LENS];
	int ipc_syslog_enable;
}easyipc_config;





typedef enum
{
	ENUM_APT_ACK_OK,					// 返回API OK , 用户可以使用
	ENUM_APT_ACK_FAIL,					// 返回API FAIL, 用户可以使用
	ENUM_API_IN_PARAM_ERROR,			// 系统使用,表示有参数返回,但是用户没有传入正确的指针获取这些参数
	ENUM_APT_ACK_CANTFIND_API,			// 找不到API最指定的服务中
	ENUM_APT_ACK_CANTFIND_PROGRAM,		// 找不到指定服务
	ENUM_APT_ACK_TIMEOUT				// 执行超时
}IPC_API_RET;

typedef struct
{
	int ack_id;
	sem_t ack_sem;
	int ack_size;
	IPC_API_RET ack_result;
	void *ack_data_point;
	char ack_data;
}_ipc_api_ack_list;


typedef void (*ipc_msg_proc_ext_p)(const char *mname,void *data,int size,void *usrdata,char *pname);
typedef void (*ipc_msg_proc_p)(const char *mname,void *data,int size,char *pname);

typedef void (*ipc_msg_proc_ext)(const char *mname,void *data,int size,void *usrdata);
typedef void (*ipc_msg_proc)(const char *mname,void *data,int size);
typedef IPC_API_RET (*ipc_api_proc)(const char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout);


typedef struct
{
	char alias_name[PROCESS_NAME_MAX_SIZE];
	int  pid;
	int send_sock;
	int msg_recv_sock;
	int msg_recv_port;
	int api_recv_sock;
	int api_recv_port;
	pthread_mutex_t send_mutex; 
	DList *msg_list;
	DList *api_list;
	DList *msg_recv_list;
	DList *api_recv_list;
	DList *api_ack_wait_list;
	int    msg_thread_start;
	int    api_thread_start;
	int api_ack_magic_num;
	sem_t connect_sem;
	sem_t msg_recv_sem;
	sem_t api_recv_sem;
	pthread_mutex_t msg_recv_mutex;
	pthread_mutex_t api_recv_mutex;
	struct sockaddr_un toAddr;
}ipc_handle;

// 创建一个服务句柄,如果失败则返回NULL, pname是服务句柄名称,进程之间不要冲突
ipc_handle * ipc_creat(const char * pname);
ipc_handle * ipc_get_default_handle();


// 注册一个关注的消息在easyipc系统,当系统中有服务广播此消息时会被注册消息的回调调用
void ipc_register_msg(ipc_handle *ipc ,const char *msg_name,ipc_msg_proc fun);
void ipc_register_msg_ext(ipc_handle *ipc,const char *msg_name,ipc_msg_proc_ext fun,void *usrdata);
void ipc_unregister_msg(ipc_handle *ipc,const char *msg_name);


// 声明一个远程API给外界调用,当外界调用msg_name时,回调函数fun则会被触发
void ipc_register_api(ipc_handle *ipc,const char *msg_name,ipc_api_proc fun);
void ipc_unregister_api(ipc_handle *ipc,const char *msg_name);

// 发送一个消息到系统,系统会自动将消息转发给注册了该消息的服务,可以携带参数 data并需要描述参数的长度
void ipc_send_msg(ipc_handle *ipc,const char *msg_name,void *data,int size);

// 远程调用指定服务名称的指定函数名称,该函数需要在它所在的服务中被声明过,可以携带参数发送给被调用者,同时也可以接受调用者的返回数据
// 需要注意,如果有返回值,则该返回值是需要调用者主动释放的,否则会发生内存泄漏问题
IPC_API_RET ipc_call_api(ipc_handle *ipc,const char *pname,const char *api_name,void *data,int size,void **ret_data,int *ret_size,int timeout);

// 进程的ipclib的初始化函数,一个进程中调用一次即可,需要在任何ipc的其它函数使用前被调用
void ipc_exit(ipc_handle *ipc);
// 打印一个log到 ipc的log系统,至于是否可以打印出来,则需要看ipcd的打印级别设置,具体可以参考 eipcprint 指令帮助,暂时不支持换行
void ipc_log(ipc_handle *ipc,IPC_LOG_LEVEL level,char *fmt, ...);

void ipc_print(ipc_handle * ipc, char * fmt, ...);
#define ipc_dbg_ext(x,...) _ipc_dbg(x,__FILE__,__FUNCTION__,__LINE__, __VA_ARGS__)
#define ipc_dbg(...) _ipc_dbg(ipc_get_default_handle(),__FILE__,__FUNCTION__,__LINE__, __VA_ARGS__)


// 设置守护该ipc,如果该ipc以外退出,则自动执行syscmd规定的指令,该指令会在ipcd中以system的方式执行,因此在一些环节中需要进程具备root权限,本指令需要在ipc creat后执行
void ipc_daemon_syscmd(ipc_handle *ipc ,char *syscmd);


// 快速声明一个远程API给x服务,API声明名称需要和函数名称一致
#define API_EXPORT_VIA_HANDLE(x,y) ipc_register_api(x,#y,y);

// 使用最后一个creat的ipc句柄服务声明一个API远程调用,常用于一个进程只有一个ipc句柄的情况,API声明名称需要和函数名称一致
#define API_EXPORT(y) ipc_register_api(ipc_get_default_handle(),#y,y);


// 详细使用方法参见 a1.c , a2.c 

void ipc_hangup_record(int signal_no);


extern easyipc_config *eipc_conf_p;

#ifdef __cplusplus
}
#endif

#endif

