#include "easyipc_base.h"
#include "easyipc_daemon.h"
#include "easyipc_debug.h"
#if enable_syslog
#include <syslog.h>
#endif
#include "easyipc_console.h"


static int debug_record_flag=1;
static int log_size=0;
extern int debug_log_size;
static DList *log_list=NULL;
static pthread_mutex_t debug_print_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t debug_save_mutex=PTHREAD_MUTEX_INITIALIZER;
char printf_log_type_flag[16]={0};
char printf_broadcast_flag = 0;
char printf_rpc_flag = 0;
char printf_log_level_flag[16]={0};
char printf_log_pname[PROCESS_NAME_MAX_SIZE]={0};
static int log_fp = -1; 
char log_path[IPC_LOG_SAVE_PATH_MAX_LENS]={0};

void ipcd_debug_register()
{
	
}


void ipcd_debug_unregister()
{
	
}

void ipcd_debug_join()
{
	
}

void ipcd_debug_exit()
{

}

void ipcd_debug_hangup()
{
	
}

void ipcd_debug_msg_send()
{
	
}

void ipcd_debug_runtime_begin()
{

}
void ipcd_debug_runtime_finish()
{
	
}

void ipcd_debug_creat_logfile()
{
	char path[128]={0};
	char cmd[256]={0};
	char first_log[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	snprintf(path,128,"%s",eipc_conf_p->ipc_log_save_path);
	snprintf(cmd,256,"rm %s",path);
	int ret = system(cmd);
	struct timespec ptime={0,0};
	clock_gettime(CLOCK_MONOTONIC,&ptime);
	static int printflag=0;
	if(ret < 0)
	{
		printf("system[%s] fail\r\n",cmd);
	}
	if(!strlen(log_path))
	{
		snprintf(log_path,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",path);
	}
	
	log_fp = open(log_path,O_WRONLY|O_CREAT,0777);
	if(log_fp>0)
	{
		snprintf(first_log,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s""\r\n","S","N","easyipc_daemon",ptime.tv_sec,ptime.tv_nsec/1000000,"Daemon start...");
		ret = write(log_fp,first_log, strlen(first_log));
	}else
	{
		if(printflag==0)
		{
			printf("log file creat error:%d\r\n",errno);
			printflag=1;
		}
	}
}


void ipcd_debug_log_memory_push(char *str,int maxsize)
{
	pthread_mutex_lock(&debug_save_mutex);	
	if(NULL == log_list)
		log_list=dlist_create();

	int malloc_size = (strlen(str)+1)>maxsize?maxsize:(strlen(str)+1);
	char *tmpm = malloc(malloc_size);
	memset(tmpm,0,malloc_size);
	snprintf(tmpm,malloc_size,"%s",str);
	
	if(log_size>debug_log_size)
	{
		dlist_delete(log_list,0);
		log_size--;
	}

	dlist_attach(log_list,tmpm);
	log_size++;
	pthread_mutex_unlock(&debug_save_mutex);
}


int ipcd_debug_log_memory_pull(char *str,int maxsize)
{
	static int index=0;
	pthread_mutex_lock(&debug_save_mutex);
	if(index<log_size)
	{
		DListNode *node = dlist_get(log_list,index);
		if(NULL == node)
		{
			index=0;
			pthread_mutex_unlock(&debug_save_mutex);
			return 0;
		}

		if(NULL == node->data)
		{
			index=0;
			pthread_mutex_unlock(&debug_save_mutex);
			return 0;
		}

		snprintf(str,maxsize,"%s",(char *)node->data);
		index++;
		pthread_mutex_unlock(&debug_save_mutex);
		return 1;
	}
	else
	{
		index=0;
		pthread_mutex_unlock(&debug_save_mutex);
		return 0;
	}
}


void ipcd_debug_init()
{
	int ret;
	snprintf(printf_log_type_flag,16,"UPS");
	if(!strlen(printf_log_type_flag))
		snprintf(printf_log_level_flag,16,"ED");	// 默认打印error的log信息
	ipcd_debug_creat_logfile();
}


void ipcd_log_save_and_print(IPC_LOG_TYPE tlt,IPC_LOG_LEVEL level,int pid,char *fmt, ...)
{
	static unsigned int rnum=0; 
	struct timespec ptime={0,0};

	int printf_flag=0;
	
	char sprint_buf[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	char printf_buf[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	char save_buf[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	char *pname = malloc(PROCESS_NAME_MAX_SIZE);
	char *level_char,*tlt_char;

	
	pthread_mutex_lock(&debug_print_mutex);
	memset(pname,0,PROCESS_NAME_MAX_SIZE);
    va_list args;
    int n;
    va_start(args, fmt);
    n = vsprintf(sprint_buf, fmt, args);
    va_end(args);
	clock_gettime(CLOCK_MONOTONIC,&ptime);
	
	// 打印格式
	// 序号 7  时间 6.4  进程名32    消息类型1    消息级别1 :内容
	if(pid>0)
	{
		ipcd_get_pname_via_pid(pid,pname);
		if(strlen(pname)<=0)
		{
			strcpy(pname,"unknow");
		}
	}else
		strcpy(pname,"ipcd");

	switch(tlt)
	{
		case ENUM_IPC_LOG_TYPE_PROGESS:
			tlt_char="P";
			break;
		case ENUM_IPC_LOG_TYPE_SYSTEM:
			tlt_char="S";
			break;
		case ENUM_IPC_LOG_TYPE_USER:
			tlt_char="U";
			break;
		default:
			tlt_char="?";
			break;
	}
	
	switch(level)
	{
		case ENUM_IPC_LOG_LEVEL_ERR:
			level_char="E";
			snprintf(save_buf,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s",tlt_char,level_char,pname,
				ptime.tv_sec,ptime.tv_nsec/1000000,	sprint_buf);
			snprintf(printf_buf,IPC_DEBUG_PRINT_MAX_SIZE,RED "%s" NONE "\r\n",save_buf);
#if enable_syslog
			openlog(pname,LOG_PID,LOG_LOCAL5);
			syslog(LOG_ERR, "%s",sprint_buf);
			closelog();
#endif
			break;
		case ENUM_IPC_LOG_LEVEL_WARN:
			level_char="W";
			snprintf(save_buf,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s",tlt_char,level_char,pname,
				ptime.tv_sec,ptime.tv_nsec/1000000,sprint_buf);
			snprintf(printf_buf,IPC_DEBUG_PRINT_MAX_SIZE,YELLOW "%s" NONE "\r\n",save_buf);
#if enable_syslog
			openlog(pname,LOG_PID,LOG_LOCAL5);
			syslog(LOG_WARNING, "%s",sprint_buf);
			closelog();
#endif				
			break;
		case ENUM_IPC_LOG_LEVEL_NORMAL:
			level_char="N";
			snprintf(save_buf,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s",tlt_char,level_char,pname,
				ptime.tv_sec,ptime.tv_nsec/1000000,sprint_buf);
			snprintf(printf_buf,IPC_DEBUG_PRINT_MAX_SIZE,L_GREEN "%s" NONE "\r\n",save_buf);
#if enable_syslog
			openlog(pname,LOG_PID,LOG_LOCAL5);
			syslog(LOG_DEBUG, "%s",sprint_buf);
			closelog();
#endif				
			break;
		case ENUM_IPC_LOG_LEVEL_INFO:
			level_char="I";
			snprintf(save_buf,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s",tlt_char,level_char,pname,
				ptime.tv_sec,ptime.tv_nsec/1000000,sprint_buf);
			snprintf(printf_buf,IPC_DEBUG_PRINT_MAX_SIZE, BOLD "%s" NONE "\r\n",save_buf);
#if enable_syslog
			openlog(pname,LOG_PID,LOG_LOCAL5);
			syslog(LOG_INFO, "%s",sprint_buf);
			closelog();
#endif			
			break;
		case ENUM_IPC_LOG_LEVEL_DBG:
			level_char="D";
			snprintf(save_buf,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s",tlt_char,level_char,pname,
				ptime.tv_sec,ptime.tv_nsec/1000000,sprint_buf);
			snprintf(printf_buf,IPC_DEBUG_PRINT_MAX_SIZE, L_PURPLE "%s" NONE "\r\n",save_buf);
#if enable_syslog
			openlog(pname,LOG_PID,LOG_LOCAL5);
			syslog(LOG_INFO, "%s",sprint_buf);
			closelog();
#endif			
			break;



		default:
			level_char="?";
			snprintf(save_buf,IPC_DEBUG_PRINT_MAX_SIZE,"%s %s %-32s %6ld.%03ld : %s",tlt_char,level_char,pname,
				ptime.tv_sec,ptime.tv_nsec/1000000,sprint_buf);
			snprintf(printf_buf,IPC_DEBUG_PRINT_MAX_SIZE,RED "%s" NONE "\r\n",save_buf);
			break;
	}

	// "PSEWNID"
	if(strstr(printf_log_type_flag,tlt_char)&&strstr(printf_log_level_flag,level_char))
	{
		ipcd_console_print("%s",printf_buf);
	}

	int ret;

	if(debug_log_size == 0)
	{
		ret = write(log_fp,save_buf, strlen(save_buf));
		ret = write(log_fp,"\r\n", strlen("\r\n"));
		
		if(ret<=0)
		{
			ipcd_debug_creat_logfile();
			ret =write(log_fp,save_buf, strlen(save_buf));
			ret =write(log_fp,"\r\n", strlen("\r\n"));
		}
	}else if (debug_log_size>0)
	{
		ipcd_debug_log_memory_push(save_buf,IPC_DEBUG_PRINT_MAX_SIZE);
	}
	if(pname)
	{
		free(pname);
		pname=NULL;
	}
	pthread_mutex_unlock(&debug_print_mutex);
}


