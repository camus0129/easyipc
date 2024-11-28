#ifndef __easyipc_daemon_h__
#define __easyipc_daemon_h__

#define IPC_CTL_SOCKET "/var/ipc.ctl.socket"
#define IPC_MSG_SOCKET "/var/ipc.msg.socket"
#define IPC_CLI_SOCKET_PREFIX "/var/"
#define IPC_CLI_SOCKET_MSG_SUFFIX ".msg.socket"
#define IPC_CLI_SOCKET_API_SUFFIX ".api.socket"
#define IPC_CONSOLE_BROADCAST_SOCKET "/var/ipc.bd.socket"

#define IPC_CTL_PROT 35669
#define IPC_MSG_PROT 35670
#define IPC_CLI_PORT_BASE 40100
#define IPC_DEBUG_PRINT_MAX_SIZE 8192
#define IPC_LIVE_TIMEOUT_NUM 14	// second
#define IPC_CONSOLE_BROADCAST_PORT 50392

#define USE_UDP 1
#define USE_TCP 0
#define USE_BACKTRACE 0


typedef enum
{
	ENUM_IPC_API_REGISTER,
	ENUM_IPC_API_UNREGISTER,
	ENUM_IPC_MSG_REGISTER,
	ENUM_IPC_MSG_UNREGISTER,
	ENUM_PROCESS_JOIN,
	ENUM_PROCESS_JOIN_ACK,
	ENUM_PROCESS_JOIN_ACK_FAIL,
	ENUM_PROCESS_LIVE,
	ENUM_PROCESS_EXIT,
	ENUM_PROCESS_HANGUP,
	ENUM_PROCESS_API_CALL,
	ENUM_IPC_CLI_API_RELAY,
	ENUM_IPC_CLI_API_ACK,
	ENUM_IPC_CLI_API_ACK_RELAY,
	ENUM_PROCESS_MSG_SEND,
	ENUM_PROCESS_MSG_RELAY,
	ENUM_PROCESS_API_RUNTIME_RECODE,
	ENUM_PROCESS_MSG_RUNTIME_RECODE,
	ENUM_PROCESS_LOG,
	ENUM_PROCESS_LOG_UPLOAD,
	ENUM_PROCESS_FILE_UPLOAD,
	ENUM_IPC_DAEMON,
}IPC_INFO_TYPE;


typedef struct
{
	IPC_INFO_TYPE iit;
	int send_pid;
	int size;
	char data;
}_ipc_packet;

typedef struct
{
	int  process_pid;
	char msg_name[IPC_MSG_MAX_SIZE];
	int  flag;	// 0:start  1:end
	struct timespec ptime;
	int caller_magic_number;
	int daemon_magic_number;
	IPC_API_RET ret;
	int ret_size;
	char ret_data;
}_ipc_runtime;


typedef struct
{
	int  register_process_pid;
	char register_msg_name[IPC_MSG_MAX_SIZE];
}_register_info;

typedef struct
{
	int  join_exit_process_pid;
	char join_exit_process_name[PROCESS_NAME_MAX_SIZE];
	int  api_port;
	int  msg_port;
	int  signal_no;
}_join_exit_info;


typedef struct __ipc_msg
{
	char msg_name[IPC_MSG_MAX_SIZE];
	char ipc_name[PROCESS_NAME_MAX_SIZE];
	int send_pid;
	int size;
	char data;
}_ipc_msg;


typedef struct __ipc_api
{
	char pname[PROCESS_NAME_MAX_SIZE];
	char api_name[IPC_MSG_MAX_SIZE];
	_ipc_api_ack_list iaal;
	int daemon_magic_number;
	int daemon_count_number;
	int timeout;
	int send_pid;
	int size;
	char data;
}_ipc_api;

typedef struct 
{
	int pid;
	int live_cnt;
	int msg_port;
	int api_port;
	char pname[PROCESS_NAME_MAX_SIZE];
	int send_cnt;
	int recv_cnt;
    int suspend;
	DList *pmsg_list;
	DList *api_list;
	struct sockaddr_un toAddr;
	DList *mask_sender;		//用于plugin的mask屏蔽消息
	DList *mask_receiver;
	char daemon_cmd[IPC_SYSCMD_SIZE];
}_mlist_member;

typedef struct 
{
	int pid;
	char daemon_cmd[IPC_SYSCMD_SIZE];
}_ipc_daemon_info;


typedef struct
{
	char msg_name[IPC_MSG_MAX_SIZE];
	int  process_cnt;
	struct timespec process_time;		// process msg use total time
}_m_member;



typedef struct __ipc_log
{
	IPC_LOG_LEVEL level;
	int size;
	char data;
}_ipc_log;


static inline struct timespec timespec_diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

static inline struct timespec timespec_sum(struct timespec src, struct timespec dest)
{
    struct timespec temp;
	temp.tv_sec = src.tv_sec + dest.tv_sec;
	temp.tv_nsec = src.tv_nsec + dest.tv_nsec;
	if(temp.tv_nsec>=1000000000)
	{
		temp.tv_nsec-=1000000000;
		temp.tv_sec++;
	}
    return temp;
}


static inline void timeraddMS(struct timeval *a, uint ms)
{
    a->tv_usec += ms * 1000;
    if(a->tv_usec >= 1000000)
    {
        a->tv_sec += a->tv_usec / 1000000;
        a->tv_usec %= 1000000;
    }
}


DList* ipcd_get_mlist();

extern DList * mlist;
extern char printf_broadcast_flag;
extern char printf_rpc_flag;
#define API_TIMEOUT_MAX 0xffffff

void ipcd_misc_help();
void ipcd_debug_init();
char* ipcd_get_pname_via_pid(int pid,char *pname);
void remove_socket_link_file(char *base_name,int port);
#endif
