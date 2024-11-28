#include "easyipc_base.h"
#include "easyipc_daemon.h"
#include "easyipc_conf_support.h"

easyipc_config *eipc_conf_p = NULL;
DList *ipc_list=NULL;

typedef struct
{
	char msg_name[IPC_MSG_MAX_SIZE];
	ipc_api_proc fun;
}_ipc_api_map_member;

typedef struct
{
	char msg_name[IPC_MSG_MAX_SIZE];
	ipc_msg_proc fun;
	ipc_msg_proc_ext fun_ext;
	void *usrdata;
}_ipc_msg_map_member;

typedef struct
{
	int size;
	char data;
}_msg_push_data;



static int  get_executable_path(char *name);
static void ipc_data_to_daemon(ipc_handle *ipc, IPC_INFO_TYPE iit,int size,char *data);
static void ipc_data_to_daemon_no_mutex(ipc_handle *ipc,IPC_INFO_TYPE iit,int size,char *data);
void ipc_recv_analysis(ipc_handle *icm,void *data,int size);

static ipc_handle* default_ipc=NULL;

void *ipc_msg_recv_probe(void *param);
void *ipc_api_recv_probe(void *param);
void *ipc_recv_msg_alloc(void *param);
void *ipc_recv_api_alloc(void *param);


ipc_handle* ipc_get_default_handle()
{
	return default_ipc;
}


// get process name and return current pid number

static int get_executable_path(char *name)
{
	char path[PATH_MAX];
	char processname[PROCESS_NAME_MAX_SIZE];
	char* path_end;
	if(readlink("/proc/self/exe", path,PROCESS_NAME_MAX_SIZE) <=0)
		return -1;
	path_end = strrchr(path,  '/');
	if(path_end == NULL)
		return -1;
	++path_end;
	strcpy(processname, path_end);
	*path_end = '\0';

	if(NULL != name)
	{
		memset(name,0,strlen(processname)+1);
		strcpy(name, processname);
	}
	return getpid();
}



static struct timespec ipc_api_runtime_start_record(ipc_handle *ipc,char *mname)
{
	_ipc_runtime ipc_runtime;
	struct timespec ptime;
	if(!ipc) return ptime;
	clock_gettime(CLOCK_MONOTONIC,&ptime);
	memset(&ipc_runtime,0,sizeof(_ipc_runtime));
	ipc_runtime.process_pid=ipc->pid;
	ipc_runtime.flag=0;
	ipc_runtime.ptime=ptime;
	//memcpy(&ipc_runtime.ptime,&ptime,sizeof(struct timespec));
	snprintf(ipc_runtime.msg_name,IPC_MSG_MAX_SIZE,"%s",mname);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_API_RUNTIME_RECODE,sizeof(_ipc_runtime),(char *)&ipc_runtime);
	//printf("start time:%ld.%ld\r\n",ptime.tv_sec,ptime.tv_nsec);
	return ptime;
}

static struct timespec ipc_msg_runtime_start_record(ipc_handle *ipc,char *mname)
{
	_ipc_runtime ipc_runtime;
	struct timespec ptime;
	if(!ipc) return ptime;
	clock_gettime(CLOCK_MONOTONIC,&ptime);
	memset(&ipc_runtime,0,sizeof(_ipc_runtime));
	ipc_runtime.process_pid=ipc->pid;
	ipc_runtime.flag=0;
	ipc_runtime.ptime=ptime;
	//memcpy(&ipc_runtime.ptime,&ptime,sizeof(struct timespec));
	snprintf(ipc_runtime.msg_name,IPC_MSG_MAX_SIZE,"%s",mname);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_MSG_RUNTIME_RECODE,sizeof(_ipc_runtime),(char *)&ipc_runtime);
	//printf("start time:%ld.%ld\r\n",ptime.tv_sec,ptime.tv_nsec);
	return ptime;
}



static void ipc_msg_runtime_end_record(ipc_handle *ipc,char *mname,struct timespec ptime_s)
{
	_ipc_runtime ipc_runtime;
	struct timespec e_time = {0, 0};
	struct timespec d_time;
	if(!ipc) return;
	clock_gettime(CLOCK_MONOTONIC,&e_time);
	memset(&ipc_runtime,0,sizeof(_ipc_runtime));
	ipc_runtime.process_pid=ipc->pid;
	ipc_runtime.flag=1;
	d_time = timespec_diff(ptime_s,e_time);
	memcpy(&ipc_runtime.ptime,&d_time,sizeof(struct timespec));
	snprintf(ipc_runtime.msg_name,IPC_MSG_MAX_SIZE,"%s",mname);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_MSG_RUNTIME_RECODE,sizeof(_ipc_runtime),(char *)&ipc_runtime);
}


static void ipc_api_runtime_end_record(ipc_handle *ipc,char *mname,struct timespec ptime_s,int ret_size,void *ret_data,IPC_API_RET ret,int daemon_magic_number,int caller_magic_number)
{
	_ipc_runtime *ipc_runtime=malloc(sizeof(_ipc_runtime)+ret_size);
	if(!ipc) return;
	if(NULL == ipc_runtime)
	{
		printf("ipc_api_runtime_end_record ipc_runtime malloc fail \r\n");
		exit(0);
	}
	memset(ipc_runtime,0,sizeof(_ipc_runtime)+ret_size);

	struct timespec e_time = {0, 0};
	struct timespec d_time;
	ipc_runtime->ret = ret;
	ipc_runtime->ret_size = ret_size;
	memcpy(&ipc_runtime->ret_data,ret_data,ret_size);
	clock_gettime(CLOCK_MONOTONIC,&e_time);
	
	ipc_runtime->process_pid=ipc->pid;
	ipc_runtime->flag=1;
	ipc_runtime->caller_magic_number = caller_magic_number;
	ipc_runtime->daemon_magic_number = daemon_magic_number;
	d_time = timespec_diff(ptime_s,e_time);
	memcpy(&ipc_runtime->ptime,&d_time,sizeof(struct timespec));
	snprintf(ipc_runtime->msg_name,IPC_MSG_MAX_SIZE,"%s",mname);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_API_RUNTIME_RECODE,sizeof(_ipc_runtime)+ret_size,(char *)ipc_runtime);
	free(ipc_runtime);
}


#if USE_UDP
static void ipc_connect_daemon(ipc_handle *ipc)
{
	// creat socket use udp via 127.0.0.1
	int sock;
	struct sockaddr_un toAddr;
	struct sockaddr_un fromAddr;

	if(!ipc) return;
	sock = socket(AF_UNIX,SOCK_DGRAM,0);
	if(sock<0)
	{
		printf("ipc_connect_daemon udp socket creat fail\r\n");
		return;
	}
	ipc->send_sock=sock;
	ipc->toAddr.sun_family=AF_UNIX;
	strcpy(ipc->toAddr.sun_path, IPC_MSG_SOCKET);
	
	_join_exit_info join_exit_info;
	memset(&join_exit_info,0,sizeof(join_exit_info));
	join_exit_info.api_port=ipc->api_recv_port;
	join_exit_info.msg_port=ipc->msg_recv_port;
	//join_exit_info.join_exit_process_pid = get_executable_path(join_exit_info.join_exit_process_name);
	join_exit_info.join_exit_process_pid = ipc->pid;
	snprintf(join_exit_info.join_exit_process_name,PROCESS_NAME_MAX_SIZE,"%s",ipc->alias_name);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_JOIN,sizeof(_join_exit_info),(char *)&join_exit_info);
}
#endif


void ipc_live(ipc_handle *ipc)
{
	if(!ipc) return;
	_join_exit_info join_exit_info;
	memset(&join_exit_info,0,sizeof(join_exit_info));
	join_exit_info.api_port=ipc->api_recv_port;
	join_exit_info.msg_port=ipc->msg_recv_port;
	join_exit_info.join_exit_process_pid = ipc->pid;
	snprintf(join_exit_info.join_exit_process_name,PROCESS_NAME_MAX_SIZE,"%s",ipc->alias_name);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_LIVE,sizeof(_join_exit_info),(char *)&join_exit_info);
}

void ipc_exit(ipc_handle *ipc)
{
	if(!ipc) return;
	_join_exit_info join_exit_info;
	memset(&join_exit_info,0,sizeof(join_exit_info));
	join_exit_info.api_port=ipc->api_recv_port;
	join_exit_info.msg_port=ipc->msg_recv_port;
	join_exit_info.join_exit_process_pid = ipc->pid;
	snprintf(join_exit_info.join_exit_process_name,PROCESS_NAME_MAX_SIZE,"%s",ipc->alias_name);
	ipc_data_to_daemon(ipc,ENUM_PROCESS_EXIT,sizeof(_join_exit_info),(char *)&join_exit_info);

	if(default_ipc==ipc)
		default_ipc=NULL;
	ipc=NULL;
}

void ipc_hangup_record(int signal_no)
{
	_join_exit_info join_exit_info;

	memset(&join_exit_info,0,sizeof(join_exit_info));
	//join_exit_info.join_exit_process_pid = get_executable_path(join_exit_info.join_exit_process_name);
	
	
	join_exit_info.signal_no = signal_no;
	
	int i,ipc_size = dlist_len(ipc_list);

	for(i=0;i<ipc_size;i++)
	{
		ipc_handle * p = dlist_get_data_safe(ipc_list,i);
		if(!p) continue;
		snprintf(join_exit_info.join_exit_process_name,PROCESS_NAME_MAX_SIZE,"%s",p->alias_name);
		join_exit_info.join_exit_process_pid = p->pid;
		ipc_data_to_daemon_no_mutex(p,ENUM_PROCESS_HANGUP,sizeof(_join_exit_info),(char *)&join_exit_info);
	}
}



static void ipc_data_to_daemon_no_mutex(ipc_handle *ipc,IPC_INFO_TYPE iit,int size,char *data)
{
	_ipc_packet *ipc_packet = (_ipc_packet *)malloc(sizeof(_ipc_packet)+size);
	static struct timespec save_ptime={0,0};
	struct timespec nowtime,difftime;
	if(!ipc) return;
	if(!ipc_packet)
	{
		printf("ipc_data_to_daemon malloc ipc_packet fail\r\n");
		exit(0);
	}
	//printf("ipc_data_to_daemon _ipc_packet malloc size = %ld\r\n",sizeof(_ipc_packet)+size);
	memset(ipc_packet,0,(sizeof(_ipc_packet)+size));
	ipc_packet->send_pid = ipc->pid;
	ipc_packet->iit=iit;
	ipc_packet->size=size;
	memcpy(&ipc_packet->data,data,size);
#if USE_UDP
	//pthread_mutex_lock(&ipc->send_mutex);
rewait:
	clock_gettime(CLOCK_MONOTONIC,&nowtime);
	difftime = timespec_diff(save_ptime,nowtime);	
	if(difftime.tv_sec<1&&difftime.tv_nsec<5*1000*1000)
	{
		usleep(5*1000);
		goto rewait;
	}
	save_ptime = nowtime;
	int ret=sendto(ipc->send_sock,ipc_packet,sizeof(_ipc_packet)+size,0,(struct sockaddr *)&ipc->toAddr,sizeof(struct sockaddr_un));
	//pthread_mutex_unlock(&ipc->send_mutex);
#endif
	free(ipc_packet);
}


static void ipc_data_to_daemon(ipc_handle *ipc,IPC_INFO_TYPE iit,int size,char *data)
{
	static struct timespec save_ptime={0,0};
	struct timespec nowtime,difftime;
	if(!ipc) return;
	if(sizeof(_ipc_packet)+size >= eipc_conf_p->ipc_msg_packet_max_size)
	{
		printf("ipc_data_to_daemon too large , max size is %d\r\n",eipc_conf_p->ipc_msg_packet_max_size);
		return;
	}
	_ipc_packet *ipc_packet = (_ipc_packet *)malloc(sizeof(_ipc_packet)+size);
	if(!ipc_packet)
	{
		printf("ipc_data_to_daemon malloc ipc_packet fail\r\n");
		exit(0);
	}
	//printf("ipc_data_to_daemon _ipc_packet malloc size = %ld\r\n",sizeof(_ipc_packet)+size);
	memset(ipc_packet,0,(sizeof(_ipc_packet)+size));
	ipc_packet->send_pid = ipc->pid;
	ipc_packet->iit=iit;
	ipc_packet->size=size;
	memcpy(&ipc_packet->data,data,size);
#if USE_UDP
	pthread_mutex_lock(&ipc->send_mutex);
#if 1	
rewait:

	clock_gettime(CLOCK_MONOTONIC,&nowtime);
	difftime = timespec_diff(save_ptime,nowtime);	
	if(difftime.tv_sec<1&&difftime.tv_nsec<50*1000)
	{
		usleep(10);
		goto rewait;
	}
	save_ptime = nowtime;
#endif	
	int ret=sendto(ipc->send_sock,ipc_packet,sizeof(_ipc_packet)+size,0,(struct sockaddr *)&ipc->toAddr,sizeof(struct sockaddr_un));
	pthread_mutex_unlock(&ipc->send_mutex);
#endif
	free(ipc_packet);
}

static void _ipc_register_msg(ipc_handle *ipc,_register_info *register_info)
{
	// send register info to daemon
	if(!ipc) return;
	ipc_data_to_daemon(ipc,ENUM_IPC_MSG_REGISTER,sizeof(_register_info),(char *)register_info);
}

static void _ipc_unregister_msg(ipc_handle *ipc,_register_info *register_info)
{
	// send register info to daemon
	if(!ipc) return;
	ipc_data_to_daemon(ipc,ENUM_IPC_MSG_UNREGISTER,sizeof(_register_info),(char *)register_info);
}

static void _ipc_register_api(ipc_handle *ipc,_register_info *register_info)
{
	// send register info to daemon
	if(!ipc) return;
	ipc_data_to_daemon(ipc,ENUM_IPC_API_REGISTER,sizeof(_register_info),(char *)register_info);
}

static void _ipc_unregister_api(ipc_handle *ipc,_register_info *register_info)
{
	// send register info to daemon
	if(!ipc) return;
	ipc_data_to_daemon(ipc,ENUM_IPC_API_UNREGISTER,sizeof(_register_info),(char *)register_info);
}

static inline void _msg_thread_strart(ipc_handle *ipc)
{
    if(ipc && !ipc->msg_thread_start)
    {
		pthread_t tid_token;
		pthread_attr_t threadAttr;
		pthread_attr_init(&threadAttr);
		pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
		if(0 !=pthread_create(&tid_token, &threadAttr, ipc_recv_msg_alloc,ipc))	
		{  
			pthread_attr_destroy(&threadAttr);
			return ;
		}
		if(0 !=pthread_create(&tid_token, &threadAttr, ipc_msg_recv_probe,ipc))	
		{  
			pthread_attr_destroy(&threadAttr);
			return ;
		}
		ipc->msg_thread_start = 1;
    }
}
static inline void _api_thread_strart(ipc_handle *ipc)
{
    if(ipc && !ipc->api_thread_start)
    {
		pthread_t tid_token;
		pthread_attr_t threadAttr;
		pthread_attr_init(&threadAttr);
		pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
		if(0 !=pthread_create(&tid_token, &threadAttr, ipc_recv_api_alloc,ipc))	
		{  
			pthread_attr_destroy(&threadAttr);
			return ;
		}
		if(0 !=pthread_create(&tid_token, &threadAttr, ipc_api_recv_probe,ipc))	
		{  
			pthread_attr_destroy(&threadAttr);
			return ;
		}
		ipc->api_thread_start = 1;
    }
}


void ipc_send_msg(ipc_handle *ipc,const char *msg_name,void *data,int size)
{
	if(!ipc)	return ;
	_ipc_msg *ipc_msg = (_ipc_msg *)malloc(sizeof(_ipc_msg)+size);
	if(!ipc_msg)
	{
		printf("ipc_send_msg malloc ipc_msg fail\r\n");
		exit(0);
	}
	memset(ipc_msg,0,(sizeof(_ipc_msg)+size));
	snprintf(ipc_msg->msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
	snprintf(ipc_msg->ipc_name,PROCESS_NAME_MAX_SIZE,"%s",ipc->alias_name);
	ipc_msg->size=size;
	ipc_msg->send_pid=ipc->pid;
	if(size>0)
	{
		memcpy(&ipc_msg->data,data,size);
	}
	
	ipc_data_to_daemon(ipc,ENUM_PROCESS_MSG_SEND,sizeof(_ipc_msg)+size,(char *)ipc_msg);
	free(ipc_msg);
}



IPC_API_RET ipc_call_api(ipc_handle *ipc,const char *pname,const char *api_name,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	IPC_API_RET ret;
	if(!ipc)
		return ENUM_APT_ACK_FAIL;
	_ipc_api *ipc_api = (_ipc_api *)malloc(sizeof(_ipc_api)+size);
	
	if(!ipc_api)
	{
		printf("ipc_call_api malloc ipc_api fail\r\n");
		exit(0);
	}

	memset(ipc_api,0,(sizeof(_ipc_api)+size));
	snprintf(ipc_api->api_name,IPC_MSG_MAX_SIZE,"%s",api_name);
	snprintf(ipc_api->pname,PROCESS_NAME_MAX_SIZE,"%s",pname);
	ipc_api->size=size;
	ipc_api->send_pid=ipc->pid;
	ipc_api->daemon_count_number=API_TIMEOUT_MAX;
	if(timeout)
		ipc_api->daemon_count_number=timeout;
	if(size>0)
	{
		memcpy(&ipc_api->data,data,size);
	}

	_ipc_api_ack_list *iaal = malloc(sizeof(_ipc_api_ack_list));
	memset(iaal,0,sizeof(sizeof(_ipc_api_ack_list)));
	sem_init(&iaal->ack_sem,0,0);
	iaal->ack_id=ipc->api_ack_magic_num++;
	dlist_attach(ipc->api_ack_wait_list,iaal); 
	memcpy(&ipc_api->iaal,iaal,sizeof(_ipc_api_ack_list));
	ipc_data_to_daemon(ipc,ENUM_PROCESS_API_CALL,sizeof(_ipc_api)+size,(char *)ipc_api);
	free(ipc_api);
	sem_wait(&iaal->ack_sem);
	ret = iaal->ack_result;

	if((iaal->ack_data_point!=NULL) &&(iaal->ack_size>0))
	{
		if(((!ret_size) || (!ret_data)))
		{
			if(iaal->ack_data_point)	
				free(iaal->ack_data_point);
			if(iaal)
				free(iaal);
			return ENUM_API_IN_PARAM_ERROR;
		}
	}
	if(ret!=ENUM_APT_ACK_OK&&ret!=ENUM_APT_ACK_FAIL)
	{
		if(iaal->ack_data_point)	
			free(iaal->ack_data_point);
		if(iaal)
			free(iaal);
		return ret;
	}
	if((ret_size && ret_data))
	{
		*ret_data=iaal->ack_data_point;
		*ret_size=iaal->ack_size;
	}
	if(iaal)
		free(iaal);
	
	return ret;
}


void ipc_register_msg_ext(ipc_handle *ipc,const char *msg_name,ipc_msg_proc_ext fun,void *usrdata)
{
	_register_info register_info;
	if(!ipc)
		return;
	if(!fun)
		return;
	if(NULL!=msg_name&&strlen(msg_name)>0)
	{
		int i,listSize = dlist_len(ipc->msg_list);
		for(i=0;i<listSize;i++)
		{
			_ipc_msg_map_member * immmp = dlist_get(ipc->msg_list,i)->data;
			if(!strcmp(immmp->msg_name,msg_name))
			{
				if(immmp->fun_ext == fun)
				{
					return;
				}
			}
		}

		memset(&register_info,0,sizeof(_register_info));
		register_info.register_process_pid =ipc->pid;
		snprintf(register_info.register_msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		_ipc_msg_map_member * ipc_map_member = malloc(sizeof(_ipc_msg_map_member));
		memset(ipc_map_member,0,sizeof(_ipc_msg_map_member));
		ipc_map_member->fun_ext = fun;
		ipc_map_member->usrdata = usrdata;
		ipc_map_member->fun = NULL;
		snprintf(ipc_map_member->msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		dlist_attach(ipc->msg_list,ipc_map_member);
		_ipc_register_msg(ipc,&register_info);
	}
}


void ipc_register_msg(ipc_handle *ipc,const char *msg_name,ipc_msg_proc fun)
{
	_register_info register_info;

	if(!ipc)
		return;
	if(!fun)
		return;
	if(NULL!=msg_name&&strlen(msg_name)>0)
	{
		int i,listSize = dlist_len(ipc->msg_list);
		for(i=0;i<listSize;i++)
		{
			_ipc_msg_map_member * immmp = dlist_get(ipc->msg_list,i)->data;
			if(!strcmp(immmp->msg_name,msg_name))
			{
				if(immmp->fun == fun)
				{
					return;
				}
			}
		}
		
		memset(&register_info,0,sizeof(_register_info));
		register_info.register_process_pid =ipc->pid;
		snprintf(register_info.register_msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		_ipc_msg_map_member * ipc_map_member = malloc(sizeof(_ipc_msg_map_member));
		memset(ipc_map_member,0,sizeof(_ipc_msg_map_member));
		ipc_map_member->fun = fun;
		ipc_map_member->fun_ext = NULL;
		ipc_map_member->usrdata = NULL;
		snprintf(ipc_map_member->msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		dlist_attach(ipc->msg_list,ipc_map_member);
		_ipc_register_msg(ipc,&register_info);
	}
}

void ipc_unregister_msg(ipc_handle *ipc,const char *msg_name)
{
	_register_info register_info;
	int sendFlag=0;
	if(!ipc)
			return;

	if(NULL!=msg_name&&strlen(msg_name)>0)
	{
		memset(&register_info,0,sizeof(_register_info));
		register_info.register_process_pid =ipc->pid;
		snprintf(register_info.register_msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		int i,lens = dlist_len(ipc->msg_list);
		for(i=0;i<lens;i++)
		{
			DListNode *node = dlist_get(ipc->msg_list, i);
			
			if(NULL == node)
				return;
			
			_ipc_msg_map_member *rimmm = node->data;
			if(NULL == rimmm)
				return;
			
			if(!strcmp(rimmm->msg_name,msg_name))
			{
				if(sendFlag==0)
				{
					sendFlag=1;
					_ipc_unregister_msg(ipc,&register_info);
				}
				dlist_delete(ipc->msg_list,i);
				//return;
			}
		}
	}
}





void ipc_register_api(ipc_handle *ipc,const char *msg_name,ipc_api_proc fun)
{
	_register_info register_info;
	if(!ipc)
		return;
	if(!fun)
		return;
	if(NULL!=msg_name&&strlen(msg_name)>0)
	{
		memset(&register_info,0,sizeof(_register_info));
		register_info.register_process_pid =ipc->pid;
		snprintf(register_info.register_msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		_ipc_api_map_member * ipc_map_member = malloc(sizeof(_ipc_api_map_member));
		memset(ipc_map_member,0,sizeof(_ipc_api_map_member));
		ipc_map_member->fun = fun;
		snprintf(ipc_map_member->msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);
		dlist_attach(ipc->api_list,ipc_map_member);
		_ipc_register_api(ipc,&register_info);
	}
}

void ipc_unregister_api(ipc_handle *ipc,const char *msg_name)
{
	_register_info register_info;
	if(!ipc)
			return;

	if(NULL!=msg_name&&strlen(msg_name)>0)
	{
		memset(&register_info,0,sizeof(_register_info));
		register_info.register_process_pid =ipc->pid;
		snprintf(register_info.register_msg_name,IPC_MSG_MAX_SIZE,"%s",msg_name);

		int i,lens = dlist_len(ipc->api_list);
		for(i=0;i<lens;i++)
		{
			DListNode *node = dlist_get(ipc->api_list, i);
			
			if(NULL == node)
				return;
			
			_ipc_api_map_member *riamm = node->data;
			if(NULL == riamm)
				return;
			
			if(!strcmp(riamm->msg_name,msg_name))
			{
				_ipc_unregister_api(ipc,&register_info);
				return;
			}
		}
	}
}



void _ipc_init()
{
	static int init_flag=0;

	if(init_flag) return;
	init_flag=1;
#ifdef ENABLE_IPC_SIGNAL
	ipc_backtrace_init();
#endif
	ipc_list = dlist_create();


	eipc_conf_p = malloc(sizeof(easyipc_config));
	memset(eipc_conf_p,0,sizeof(easyipc_config));
	snprintf(eipc_conf_p->ipc_log_save_path,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",IPC_DEBUG_SAVE_PATH);
	eipc_conf_p->ipc_msg_packet_max_size = IPC_MAX_PACKET;
	eipc_conf_p->ipc_syslog_enable=0;

	int value;
	int ret;
	ret = GetConfigIntValue(IPC_CONF_FILE_PATH,"Config","packet_max_size",&value);
	if(!ret)
	{
		eipc_conf_p->ipc_msg_packet_max_size = value;
		//printf("easyipc reset packet_max_size = %d\n",eipc_conf_p->ipc_msg_packet_max_size);
		
	}

	ret = GetConfigIntValue(IPC_CONF_FILE_PATH,"Config","ipc_syslog_enable",&value);
	if(!ret)
	{
		eipc_conf_p->ipc_syslog_enable = value;
		//printf("easyipc reset ipc_syslog_enable = %d\n",eipc_conf_p->ipc_syslog_enable);
		
	}

	char save_path[IPC_LOG_SAVE_PATH_MAX_LENS]={0};
	ret = GetConfigStringValue(IPC_CONF_FILE_PATH,"Config","ipc_log_save_path",save_path);
	if(!ret)
	{
	//	printf("easyipc reset ipc_log_save_path = %s\n",save_path);
		snprintf(eipc_conf_p->ipc_log_save_path,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",save_path);
	}	



}



void *ipc_msg_recv_probe(void *param)
{
	ipc_handle *icm=(ipc_handle *)param;
	struct timespec s_time;
	_ipc_msg_map_member *imm=NULL;
	int i,mlist_size;
	_msg_push_data *mpd = NULL;

	while(1)
	{
		sem_wait(&icm->msg_recv_sem);
		pthread_mutex_lock(&icm->msg_recv_mutex);
		if(dlist_len(icm->msg_recv_list)<=0)
		{
			pthread_mutex_unlock(&icm->msg_recv_mutex);
			continue;
		}
		DListNode *dn = dlist_get(icm->msg_recv_list,0);
		if(!dn)
		{
			dlist_delete(icm->msg_recv_list, 0);
			pthread_mutex_unlock(&icm->msg_recv_mutex);
			continue;
		}
		mpd = (_msg_push_data *)(dn->data);
		if(!mpd)
		{
			dlist_delete(icm->msg_recv_list, 0);
			pthread_mutex_unlock(&icm->msg_recv_mutex);
			continue;
		}
		int size = mpd->size;
		_ipc_msg * im = (_ipc_msg *)malloc(size);
		memset(im,0,size);
		memcpy(im,&mpd->data,size);
		dlist_delete(icm->msg_recv_list, 0);
		pthread_mutex_unlock(&icm->msg_recv_mutex);
		mlist_size = dlist_len(icm->msg_list);
		int normal_fun_exist=0;
		for(i=0;i<mlist_size;i++)
		{
			imm = dlist_get_data_safe(icm->msg_list,i);
			if(!imm) continue;
			//printf("index : %d   , imm->msg_name%s     im->msg_name=%s\n",i,imm->msg_name,im->msg_name);
			if(!strcmp(imm->msg_name,im->msg_name))
			{
				s_time = ipc_msg_runtime_start_record(icm,im->msg_name);
				if(imm->fun)
				{
					imm->fun(im->msg_name,&im->data,im->size);
				}else
				if(imm->fun_ext)
				{
					imm->fun_ext(im->msg_name,&im->data,im->size,imm->usrdata);
				}
				ipc_msg_runtime_end_record(icm,im->msg_name,s_time);
				normal_fun_exist=1;
				//break;
			}
		}	

		if(0==normal_fun_exist)
		{
			for(i=0;i<mlist_size;i++)
			{
				imm = dlist_get_data_safe(icm->msg_list,i);
				if(!imm) continue;
				if(!strcmp(imm->msg_name,"*"))
				{
					s_time = ipc_msg_runtime_start_record(icm,im->msg_name);
					imm->fun(im->msg_name,&im->data,im->size);
					ipc_msg_runtime_end_record(icm,im->msg_name,s_time);
					break;
				}
			}
		}
		
		free(im);
	}
	return NULL;
}

typedef struct
{
	_ipc_api * ia;
	ipc_handle *icm;
}_ipc_api_callreal_param;



void *ipc_api_callreal_thread(void *_iccp)
{
	struct timespec s_time;
	_ipc_api_map_member *imm=NULL;
	_ipc_api_callreal_param *iccp=(_ipc_api_callreal_param *)_iccp;
	int i,mlist_size;
	_ipc_api * ia = iccp->ia;
	ipc_handle *icm = iccp->icm;
	
	mlist_size = dlist_len(icm->api_list);
	for(i=0;i<mlist_size;i++)
	{
		imm = dlist_get_data_safe(icm->api_list,i);
		if(!imm) continue;
		if(!strcmp(imm->msg_name,ia->api_name))
		{
			//(char *mname,int size,void *data)
			s_time = ipc_api_runtime_start_record(icm,ia->api_name);
			int ret_size=0; 
			void *ret_data=NULL;
			IPC_API_RET ret = imm->fun(ia->api_name,&ia->data,ia->size,&ret_data,&ret_size,ia->daemon_count_number);
			ipc_api_runtime_end_record(icm,ia->api_name,s_time,ret_size,ret_data,ret,ia->daemon_magic_number,ia->iaal.ack_id);
			if((ret_size!=0)&&(NULL!=ret_data))
			{
				free(ret_data);
			}
			break;
		}
	}	
	free(ia);	
	free(iccp);
	return NULL;
}

void *ipc_api_callreal_thread_creat(_ipc_api_callreal_param *iccp)
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, ipc_api_callreal_thread,iccp))
	{  
		pthread_attr_destroy(&threadAttr);
		return NULL;
	}	
	return NULL;
}




void *ipc_api_recv_probe(void *param)
{
	ipc_handle *icm=(ipc_handle *)param;
	_ipc_api * p=NULL;
	_msg_push_data *mpd=NULL;
	while(1)
	{
		_ipc_api_callreal_param *iccp = malloc(sizeof(_ipc_api_callreal_param));
		sem_wait(&icm->api_recv_sem);
		pthread_mutex_lock(&icm->api_recv_mutex);
		mpd = (_msg_push_data *)dlist_get_data_safe(icm->api_recv_list,0);
		if(!mpd) continue;
		int size = mpd->size;
		_ipc_api * ia = (_ipc_api *)malloc(size);
		memcpy(ia,&mpd->data,size);
		dlist_delete(icm->api_recv_list,0);
		pthread_mutex_unlock(&icm->api_recv_mutex);
		iccp->ia=ia;
		iccp->icm=icm;
		ipc_api_callreal_thread_creat(iccp);
	}
	return NULL;
}



void *ipc_spec_probe(void *param)
{
	ipc_handle *icm=(ipc_handle *)param;
	while(1)
	{
		ipc_live(icm);
		usleep(50*1000);
	}
	return NULL;
}


void *ipc_recv_msg_alloc(void *param)
{
	ipc_handle *icm=(ipc_handle *)param;
	char *recvBuffer = malloc(eipc_conf_p->ipc_msg_packet_max_size);
	int i,recvLen,mlist_size;
	if(NULL == recvBuffer)
	{
		printf("ipc_recv_msg_alloc malloc buffer fail\n");
		exit(0);
	}
	
	while(1)
	{
		while(1)
		{
			memset(recvBuffer,0,eipc_conf_p->ipc_msg_packet_max_size);

#if USE_UDP
			if((recvLen = recvfrom(icm->msg_recv_sock,recvBuffer,eipc_conf_p->ipc_msg_packet_max_size,0,NULL,NULL))<0)
#endif			
			{
	 			printf("()recvfrom()函数使用失败了.\r\n");
				break;
			}

			if(recvLen==0)
			{
				printf("easyipc daemon exit ! \r\n");
				exit(0);
			}

			ipc_recv_analysis(icm,recvBuffer,recvLen);
		}
	}
	free(recvBuffer);
	return NULL;
}


void ipc_msg_push(ipc_handle *icm,_ipc_msg *data , int recvLen)
{
	_msg_push_data *mpd = (_msg_push_data *)malloc(recvLen+sizeof(int));
	if(!mpd)
	{
		printf("ipc_msg_push malloc(%d) fail\n",recvLen);
		exit(0);
	}
	memset(mpd,0,(recvLen+sizeof(int)));
	mpd->size = recvLen;
	memcpy(&mpd->data,data,recvLen);
	pthread_mutex_lock(&icm->msg_recv_mutex);
	dlist_attach(icm->msg_recv_list,mpd);
	pthread_mutex_unlock(&icm->msg_recv_mutex);
	sem_post(&icm->msg_recv_sem);
}

void ipc_api_push(ipc_handle *icm,_ipc_api *data , int recvLen)
{
	_msg_push_data *mpd = (_msg_push_data *)malloc(recvLen+sizeof(int));
	if(!mpd)
	{
		printf("ipc_api_push malloc(%d) fail\n",recvLen);
		exit(0);
	}
	memset(mpd,0,(recvLen+sizeof(int)));
	mpd->size = recvLen;
	memcpy(&mpd->data,data,recvLen);
	pthread_mutex_lock(&icm->api_recv_mutex);
	dlist_attach(icm->api_recv_list,mpd);
	pthread_mutex_unlock(&icm->api_recv_mutex);
	sem_post(&icm->api_recv_sem);
}

void ipc_recv_analysis(ipc_handle *icm,void *data,int size)
{
		int sendsize = size;
		_ipc_packet *ipc_packet = (_ipc_packet *)data;
		//printf("recvdata:cnt=%d , size = %d ipc_packet->iit=%d\r\n",i++,size,ipc_packet->iit);

		switch(ipc_packet->iit)
		{
			case ENUM_PROCESS_MSG_RELAY:
			{
				_ipc_msg *tipc_msg = (_ipc_msg *)&ipc_packet->data;
				ipc_msg_push(icm, tipc_msg,ipc_packet->size);
				break;
			}
			case ENUM_IPC_CLI_API_RELAY:
			{
				_ipc_api *tipc_api = (_ipc_api *)&ipc_packet->data;
				ipc_api_push(icm,tipc_api,ipc_packet->size);
				break;
			}
			case ENUM_IPC_CLI_API_ACK_RELAY:
			{
				_ipc_api_ack_list *iaal = (_ipc_api_ack_list *)&ipc_packet->data;

				int acklist_size = dlist_len(icm->api_ack_wait_list);
				int k=0;
				
				_ipc_api_ack_list *tiaal=NULL;
				for(k=0;k<acklist_size;k++)
				{
					tiaal = dlist_get_data_safe(icm->api_ack_wait_list,k);
					if(!tiaal) continue;
					if(iaal->ack_id==tiaal->ack_id)
					{
						dlist_remove(icm->api_ack_wait_list,k);
						tiaal->ack_result = iaal->ack_result;
						tiaal->ack_size = iaal->ack_size;
						void *p =NULL;
						if(tiaal->ack_size>0)
						{
							p= malloc(tiaal->ack_size);
							memcpy(p,&iaal->ack_data,tiaal->ack_size);
						}
						tiaal->ack_data_point=p;
						sem_post(&tiaal->ack_sem);
						return;
					}
				}
				break;
			}
			case ENUM_PROCESS_JOIN_ACK:
				sem_post(&icm->connect_sem);
				break;
			case ENUM_PROCESS_JOIN_ACK_FAIL:
				//exit(0);
				break;
			default:
				break;
		}

		

}

void *ipc_recv_api_alloc(void *param)
{
	ipc_handle *icm=(ipc_handle *)param;
	char *recvBuffer = malloc(eipc_conf_p->ipc_msg_packet_max_size);
	int recvLen,i,alist_size;
	_ipc_api_map_member *imm=NULL;
	struct timespec s_time;
	while(1)
	{
		while(1)
		{
			memset(recvBuffer,0,eipc_conf_p->ipc_msg_packet_max_size);
#if USE_UDP
			if((recvLen = recvfrom(icm->api_recv_sock,recvBuffer,eipc_conf_p->ipc_msg_packet_max_size,0,NULL,NULL))<0)
#endif			
			{
	 			printf("()recvfrom()函数使用失败了.\r\n");
				break;
			}

			if(recvLen==0)
			{
				printf("easyipc daemon exit ! \r\n");
				exit(0);
			}


			ipc_recv_analysis(icm,recvBuffer,recvLen);

		}
	}

	free(recvBuffer);
	return NULL;
}


ipc_handle* ipc_creat(const char *pname)
{
	static int pid_magic_number=1;

	_ipc_init();
	if(NULL==pname || strlen(pname)==0)
	{
		return NULL;
	}
	ipc_handle *ipc_cli_member=malloc(sizeof(ipc_handle));
	
	memset(ipc_cli_member,0,sizeof(ipc_handle));
	ipc_cli_member->msg_list = dlist_create();
	ipc_cli_member->api_list = dlist_create();
	ipc_cli_member->msg_recv_list = dlist_create();
	ipc_cli_member->api_recv_list = dlist_create();
	ipc_cli_member->api_ack_wait_list = dlist_create();
	sem_init(&ipc_cli_member->msg_recv_sem,0,0);
	sem_init(&ipc_cli_member->api_recv_sem,0,0);
	sem_init(&ipc_cli_member->connect_sem,0,0);
	pthread_mutex_init(&ipc_cli_member->msg_recv_mutex,NULL);
	pthread_mutex_init(&ipc_cli_member->api_recv_mutex,NULL);
	
	snprintf(ipc_cli_member->alias_name,PROCESS_NAME_MAX_SIZE,"%s",pname);
	ipc_cli_member->pid = getpid()+pid_magic_number*1000000;
	if(!(ipc_cli_member->msg_list&&ipc_cli_member->api_list))
	{
		printf("ipc_init dlist_create fail\r\n");
		exit(0);
	}

	int i,ipc_size = dlist_len(ipc_list);

	for(i=0;i<ipc_size;i++)
	{
		ipc_handle * p = dlist_get_data_safe(ipc_list,i);
		if(!p) continue;
		if(!strcmp(p->alias_name,pname))
		{
			printf("error! ipc creat name[%s] repet\r\n",pname);
			free(ipc_cli_member);
			return NULL;
		}
	}	

	int port=IPC_CLI_PORT_BASE;

	int sock;
	struct sockaddr_un fromAddr;
#if USE_UDP	
	sock = socket(AF_UNIX,SOCK_DGRAM,0);
#endif
	if(sock < 0)
	{
		printf("errno=%d\n",errno);
		exit(0);
	}else
	{
		//printf("ipc_creat msg sock ok\r\n");
	}

	memset(&fromAddr,0,sizeof(fromAddr));
	fromAddr.sun_family=AF_UNIX;
	char ipc_msg_socket_name[108]={0};
msg_re_bind:	
	sprintf(ipc_msg_socket_name,"%s%s%s",IPC_CLI_SOCKET_PREFIX,pname,IPC_CLI_SOCKET_MSG_SUFFIX);
	strcpy(fromAddr.sun_path, ipc_msg_socket_name);
	unlink(fromAddr.sun_path);
	if(bind(sock,(struct sockaddr*)&fromAddr,sizeof(fromAddr))<0)
	{
		//printf("errno=%d\n",errno);
		//close(sock);
		port++;
		goto msg_re_bind;
	}else
	{
		//printf("ipc_creat msg bind ok sock = %d port = %d IPC_CLI_PATH = %s\r\n",sock,port,IPC_CLI_PATH);
	}

	ipc_cli_member->msg_recv_port = port;
	ipc_cli_member->msg_recv_sock = sock;
	port++;
#if USE_UDP	
	sock = socket(AF_UNIX,SOCK_DGRAM,0);
	//opt =1;
	//setsockopt(sock,SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
	if(sock < 0)
	{
		printf("errno=%d\n",errno);
		exit(0);
	}
	else
	{
		//printf("ipc_creat api sock ok\r\n");
	}

	
	memset(&fromAddr,0,sizeof(fromAddr));
	fromAddr.sun_family=AF_UNIX;
	char ipc_api_socket_name[108]={0};
api_re_bind:	
	sprintf(ipc_api_socket_name,"%s%s%s",IPC_CLI_SOCKET_PREFIX,pname,IPC_CLI_SOCKET_API_SUFFIX);
	strcpy(fromAddr.sun_path, ipc_api_socket_name);
	unlink(fromAddr.sun_path);
	if(bind(sock,(struct sockaddr*)&fromAddr,sizeof(fromAddr))<0)
	{
		//printf("errno=%d\n",errno);
		//close(sock);
		port++;
		goto api_re_bind;
	}else
	{
		//printf("ipc_creat api bind ok, sock=%d port = %d IPC_CLI_API_PATH = %s\r\n",sock,port,IPC_CLI_API_PATH);
	}

	ipc_cli_member->api_recv_port = port;
	ipc_cli_member->api_recv_sock = sock;

	_msg_thread_strart(ipc_cli_member);
	_api_thread_strart(ipc_cli_member);

	pthread_mutex_init(&ipc_cli_member->send_mutex,NULL);
	dlist_attach(ipc_list,ipc_cli_member);
	usleep(10*1000);	//wait recv thread start!

	int s;
	struct timespec outtime;
	struct timeval now;
retry_connect_daemon:

	gettimeofday(&now, NULL);
	timeraddMS(&now, 2000);
	outtime.tv_sec = now.tv_sec;
	outtime.tv_nsec = now.tv_usec * 1000;
	ipc_connect_daemon(ipc_cli_member);

	//sem_wait(&ipc_cli_member->connect_sem);
	
	while ((s = sem_timedwait(&ipc_cli_member->connect_sem,&outtime)) == -1 && errno == EINTR)
		continue;		/* Restart if interrupted by handler */

	if(-1==s) // timeout
	{
		printf("[%s] connect time out , retry!\r\n",pname);
		goto retry_connect_daemon;
	}
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);

	if(0 !=pthread_create(&tid_token, &threadAttr, ipc_spec_probe,ipc_cli_member))
	{  
		pthread_attr_destroy(&threadAttr);
		return NULL;
	}	
	pthread_attr_destroy(&threadAttr);		

	//printf("[%s] easyipc creat success\r\n",pname);

	default_ipc = ipc_cli_member;
	pid_magic_number++;

	return ipc_cli_member;
}


void ipc_log(ipc_handle *ipc,IPC_LOG_LEVEL level,char *fmt, ...)
{
	char sprint_buf[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	if(!ipc)
		return;
    va_list args;
	int n;
    va_start(args, fmt);
    n = vsprintf(sprint_buf, fmt, args);
    va_end(args);
	int size = sizeof(_ipc_log)+strlen(sprint_buf)+1;
	_ipc_log *il=malloc(size);
	memset(il,0,size);
	memcpy(&il->data,sprint_buf,strlen(sprint_buf));
	il->level=level;
	il->size=strlen(sprint_buf)+1;
	ipc_data_to_daemon(ipc,ENUM_PROCESS_LOG,size,(char *)il);
}

void _ipc_dbg(ipc_handle *ipc,char *_file,char *_fun,int _line,char *fmt, ...)
{
	char sprint_buf_all[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	
	char *sprint_buf = NULL;

	sprintf(sprint_buf_all,"[dbg] %s->%s:%d ",_file,_fun,_line);
	int size=strlen(sprint_buf_all);
	sprint_buf = &sprint_buf_all[size];
    va_list args;
	int n;
    va_start(args, fmt);
    n = vsprintf(sprint_buf, fmt, args);
    va_end(args);
	size = sizeof(_ipc_log)+strlen(sprint_buf_all)+1;
	_ipc_log *il=malloc(size);
	memset(il,0,size);
	memcpy(&il->data,sprint_buf_all,strlen(sprint_buf_all));
	il->level=ENUM_IPC_LOG_LEVEL_DBG;
	il->size=strlen(sprint_buf_all)+1;
	ipc_data_to_daemon(ipc,ENUM_PROCESS_LOG,size,(char *)il);
}

void ipc_print(ipc_handle *ipc,char *fmt, ...)
{
	char sprint_buf[IPC_DEBUG_PRINT_MAX_SIZE]={0};
	if(!ipc)
		return;
    va_list args;
	int n;
    va_start(args, fmt);
    n = vsprintf(sprint_buf, fmt, args);
    va_end(args);
	int size = sizeof(_ipc_log)+strlen(sprint_buf)+1;
	_ipc_log *il=malloc(size);
	memset(il,0,size);
	memcpy(&il->data,sprint_buf,strlen(sprint_buf));
	il->level=ENUM_IPC_LOG_LEVEL_NORMAL;
	il->size=strlen(sprint_buf)+1;
	ipc_data_to_daemon(ipc,ENUM_PROCESS_LOG,size,(char *)il);
}




void ipc_daemon_syscmd(ipc_handle *ipc ,char *syscmd)
{
	if(!ipc)
		return;
	_ipc_daemon_info idi;
	memset(&idi,0,sizeof(_ipc_daemon_info));
	idi.pid = ipc->pid;
	snprintf(idi.daemon_cmd,IPC_SYSCMD_SIZE,"%s",syscmd);
	ipc_data_to_daemon(ipc,ENUM_IPC_DAEMON,sizeof(_ipc_daemon_info),(char *)&idi);
}


