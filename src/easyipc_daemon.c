#include "easyipc_base.h"
#include "easyipc_daemon.h"
#include "easyipc_debug.h"
#include "easyipc_console.h"
#include "easyipc_misc.h"
#include "easyipc_plugin.h"
#include "dlist.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include "easyipc_conf_support.h"

easyipc_config *eipc_conf_p = NULL;

DList * mlist = NULL;
static DList * recv_mlist = NULL;
static DList * recv_alist = NULL;
static DList * recv_acklist = NULL;

static pthread_mutex_t mlist_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t msg_list_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t api_list_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t api_ack_list_mutex=PTHREAD_MUTEX_INITIALIZER;

static sem_t msg_list_sem ;
static sem_t api_list_sem ;
static int eipcd_sock=-1;

int debug_log_size=0;

#define DEFAULT_CALL_TIMEOUT 5


void ipcd_api_ack_list_push(_ipc_api *ipc_api);
void ipcd_mlist_remove_via_pid(int pid);
void ipcd_relay_api(_mlist_member *mlm,char *mname,int size,void *data);
void ipcd_relay_msg(_mlist_member *mlm,char *mname,int size,void *data);
_ipc_msg * ipcd_msg_pull();
_ipc_api * ipcd_api_pull();
void ipcd_analysis_core(_ipc_packet *ipc_packet,struct sockaddr_un *toAddr);
void ipcctl_analysis_core(ipc_cli_packet *icp,struct sockaddr_un *toAddr);
void *_ipcd_msg_probe(void *param);
void ipcd_api_ack_relay(_mlist_member *mlm,int size,void *data);
char* strstri(char* str, const char* subStr);

int find_netif(char *ifname)
{
#define MAXINTERFACES 16
    int fd, interface;
    struct ifreq buf[MAXINTERFACES];
    struct ifconf ifc;
    char mac[32] = {0};
 
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        int i = 0;
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = (caddr_t)buf;
        if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
        {
            interface = ifc.ifc_len / sizeof(struct ifreq);
            //printf("interface num is %d\n", interface);
            while (i < interface)
            {
                //printf("net device %s\n", buf[i].ifr_name);
                if(!strcmp(buf[i].ifr_name,ifname))
                {
					return 0;
				}
#if 0				
                if (!(ioctl(fd, SIOCGIFHWADDR, (char *)&buf[i])))
                {
                    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[0],
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[1],
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[2],
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[3],
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[4],
                        (unsigned char)buf[i].ifr_hwaddr.sa_data[5]);
                    printf("HWaddr %s\n", mac);
                }
				printf("\n");
#endif				
                i++;
            }
        }
    }
	return -1;
}






DList* ipcd_get_mlist()
{
	return mlist;
}

char* ipcd_get_pname_via_pid(int pid,char *pname)
{
	int member_size = dlist_len(mlist);
	int i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = (_mlist_member *)dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == pid)
		{
			if(pname) snprintf(pname,PROCESS_NAME_MAX_SIZE,"%s",tmlm->pname);
			return tmlm->pname;
		}
	}
	return NULL;	
}

static _mlist_member * ipcd_get_mlist_via_pid(int pid)
{
	int member_size = dlist_len(mlist);
	int i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == pid)
		{
			return tmlm;
		}
	}
	return NULL;
}

static inline void _seconds_sleep (unsigned seconds)
{
    struct timeval tv;
    tv.tv_sec=seconds/1000;
    tv.tv_usec=(seconds%1000)*1000;
    int err;

    do
    {
        err=select (0,NULL,NULL,NULL,&tv);
    }
    while (err<0 && errno==EINTR);
}


void *ipc_live_thread(void *param)
{
	int member_size=0;
	int i=0;
	_mlist_member * tmlm = NULL;
	char daemon_cmd[IPC_SYSCMD_SIZE];
	while(1)
	{
		_seconds_sleep(300);
		pthread_mutex_lock(&mlist_mutex);
		member_size = dlist_len(mlist);
		for(i=0;i<member_size;i++)
		{
			tmlm = dlist_get_data_safe(mlist,i);
			if(NULL == tmlm)
				continue;

			if(tmlm->live_cnt++>IPC_LIVE_TIMEOUT_NUM && !tmlm->suspend)
			{
				memset(daemon_cmd,0,IPC_SYSCMD_SIZE);
				if(strlen(tmlm->daemon_cmd)>0)
				{
					snprintf(daemon_cmd,IPC_SYSCMD_SIZE,"%s",tmlm->daemon_cmd);
				}
				
                tmlm->suspend=1;
				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
						tmlm->pid,"[%s] process miss because timeout",tmlm->pname);
#if 0
				if(strlen(daemon_cmd)>0)
					ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
						tmlm->pid,"syscmd->%s",daemon_cmd);
				
				ipcd_mlist_remove_via_pid(tmlm->pid);
				if(strlen(daemon_cmd))
				{
					if(system(daemon_cmd)<0)
						_seconds_sleep(10);
				}
#endif                
			}
		}
		pthread_mutex_unlock(&mlist_mutex);
	}
}


void *ipc_api_timeout_check_thread(void *param)
{
	while(1)
	{
		usleep(100*1000);
		
		pthread_mutex_lock(&mlist_mutex);
		pthread_mutex_lock(&api_ack_list_mutex);
		int i=0;
		for(i=0;i<dlist_len(recv_acklist);)
		{
			_ipc_api * tipc_api = dlist_get_data_safe(recv_acklist,i);
			if(!tipc_api) continue;
			if(API_TIMEOUT_MAX!=tipc_api->daemon_count_number)
				tipc_api->daemon_count_number-=100;
			
			if(tipc_api->daemon_count_number<=0)
			{
				_ipc_api_ack_list *iaal=malloc(sizeof(_ipc_api_ack_list));
				memset(iaal,0,sizeof(_ipc_api_ack_list));
				iaal->ack_result = ENUM_APT_ACK_TIMEOUT;
				iaal->ack_size = 0;
				iaal->ack_id= tipc_api->iaal.ack_id;
				_mlist_member * relay_mlm = ipcd_get_mlist_via_pid(tipc_api->send_pid);
				if(relay_mlm)
					ipcd_api_ack_relay(relay_mlm,sizeof(_ipc_api_ack_list),iaal);
				free(iaal);


				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
						tipc_api->send_pid,
						"[%s] msg timeout , recv:[%s]",tipc_api->api_name,tipc_api->pname);
				dlist_delete(recv_acklist,i);
			}else
				i++;
		}
		pthread_mutex_unlock(&api_ack_list_mutex);	
		pthread_mutex_unlock(&mlist_mutex);
	}
}


void ipc_api_timeout_check_thread_creat()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, ipc_api_timeout_check_thread,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	}	
}


void ipc_live_thread_creat()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, ipc_live_thread,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	}	
}


void ipcd_register_api(_register_info *register_info)
{
	int member_size = dlist_len(mlist);
	int i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == register_info->register_process_pid)
		{
			int j,api_list_size=dlist_len(tmlm->api_list);
			for(j=0;j<api_list_size;j++)
			{
				_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->api_list,j);
				if(!p) continue;
				if(!strcmp(p->msg_name,register_info->register_msg_name))
				{
					ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
						register_info->register_process_pid,
						"[%s] register api repeat",register_info->register_msg_name);
					return;
				}

			}
		
			_m_member *m_member = malloc(sizeof(_m_member));
			memset(m_member,0,sizeof(_m_member));
			snprintf(m_member->msg_name,IPC_MSG_MAX_SIZE,"%s",register_info->register_msg_name);
			dlist_attach(tmlm->api_list,m_member);
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
						register_info->register_process_pid,
						"[%s] register api success",register_info->register_msg_name);
			return;
		}
	}	
}


void ipcd_register_msg(_register_info *register_info)
{
	int member_size = dlist_len(mlist);
	int i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == register_info->register_process_pid)
		{
			int j,api_list_size=dlist_len(tmlm->pmsg_list);
			for(j=0;j<api_list_size;j++)
			{
				_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->pmsg_list,j);
				if(!p) continue;
				if(!strcmp(p->msg_name,register_info->register_msg_name))
				{
					//ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
					//	register_info->register_process_pid,
					//	"[%s] register msg repeat",register_info->register_msg_name);
					return;
				}
			}

			_m_member *m_member = malloc(sizeof(_m_member));
			memset(m_member,0,sizeof(_m_member));
			snprintf(m_member->msg_name,IPC_MSG_MAX_SIZE,"%s",register_info->register_msg_name);
			dlist_attach(tmlm->pmsg_list,m_member);
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
						register_info->register_process_pid,
						"[%s] register msg success",register_info->register_msg_name);
			return;
		}
	}	
}


void ipcd_unregister_api(_register_info *register_info)
{
	int member_size = dlist_len(mlist);
	int j,i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get(mlist,i)->data;
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == register_info->register_process_pid)
		{
			for(j=0;j<dlist_len(tmlm->api_list);j++)
			{
				_m_member *m_member = dlist_get_data_safe(tmlm->api_list,j);
				if(!m_member) continue;
				if(!strcmp(register_info->register_msg_name,m_member->msg_name))
				{
					dlist_delete(tmlm->api_list,j);
					ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
						register_info->register_process_pid,
						"[%s] unregister api success",register_info->register_msg_name);	
					return;
				}
			}
			
		}
	}
	
	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
			register_info->register_process_pid,
			"[%s] unregister api fail",register_info->register_msg_name);	
}


void ipcd_unregister_msg(_register_info *register_info)
{
	int member_size = dlist_len(mlist);
	int j,i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == register_info->register_process_pid)
		{
			for(j=0;j<dlist_len(tmlm->pmsg_list);j++)
			{
				_m_member *m_member = dlist_get_data_safe(tmlm->pmsg_list,j);
				if(!m_member) continue;
				if(!strcmp(register_info->register_msg_name,m_member->msg_name))
				{
					dlist_delete(tmlm->pmsg_list,j);
					ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
							register_info->register_process_pid,
							"[%s] unregister msg success",register_info->register_msg_name);	
					return;
				}
			}
			
		}
	}
	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
		register_info->register_process_pid,
		"[%s] unregister msg fail",register_info->register_msg_name);
}


void ipcd_mlist_remove_via_pid(int pid)
{
	int member_size = dlist_len(mlist);
	int j,i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;

		if(tmlm->pid == pid)
		{
			for(j=0;j<dlist_len(tmlm->pmsg_list);j++)
			{
				dlist_delete(tmlm->pmsg_list,0);
			}
			dlist_destroy(tmlm->pmsg_list);

			for(j=0;j<dlist_len(tmlm->api_list);j++)
			{
				dlist_delete(tmlm->api_list,0);
			}
			dlist_destroy(tmlm->api_list);
			dlist_delete(mlist,i);
			break;
		}
	}
}





_mlist_member * ipcd_join(_join_exit_info *join_exit_info,struct sockaddr_un *toAddr)
{

	int member_size = dlist_len(mlist);
	int j,i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(mlist,i);
		if(NULL == tmlm)
			continue;
	
		if(tmlm->pid == join_exit_info->join_exit_process_pid)
		{
            if(!tmlm->suspend)
            {
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
				tmlm->pid,
				"[%s] join pid repeat, pid=%d , join refuse",join_exit_info->join_exit_process_name
				,join_exit_info->join_exit_process_pid);		
				return NULL;
            }else
            {   // 进程已经挂起,可以直接删除重建
                ipcd_mlist_remove_via_pid(tmlm->pid);
            }
		}
		if(!strcmp(tmlm->pname,join_exit_info->join_exit_process_name))
		{
            if(!tmlm->suspend)
            {
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
				tmlm->pid,
				"[%s] join name repeat, pid=%d , join refuse",join_exit_info->join_exit_process_name
				,join_exit_info->join_exit_process_pid);		
			return NULL;
            }else
            {   // 进程已经挂起,可以直接删除重建
                ipcd_mlist_remove_via_pid(tmlm->pid);
            }
		}

	}
	


	_mlist_member *mlist_member = (_mlist_member *)malloc(sizeof(_mlist_member));
	if(!mlist_member)
	{
		printf("ipcd_register_msg mlist_member malloc fail\r\n");
		exit(0);
	}

	memset(mlist_member,0,sizeof(_mlist_member));
	mlist_member->live_cnt=0;
    mlist_member->suspend=0;
    memset(mlist_member->daemon_cmd,0,IPC_SYSCMD_SIZE);
	mlist_member->pid = join_exit_info->join_exit_process_pid;
	snprintf(mlist_member->pname,PROCESS_NAME_MAX_SIZE,"%s",join_exit_info->join_exit_process_name);
	mlist_member->pmsg_list = dlist_create();
	mlist_member->api_list = dlist_create();
	mlist_member->msg_port = join_exit_info->msg_port;	
	mlist_member->api_port = join_exit_info->api_port;
	memcpy(&mlist_member->toAddr,toAddr,sizeof(struct sockaddr_un));
	mlist_member->mask_receiver = dlist_create();
	mlist_member->mask_sender = dlist_create();
	dlist_attach(mlist, mlist_member);

	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
		mlist_member->pid,
		"[%s] join , pid=%d",join_exit_info->join_exit_process_name
		,join_exit_info->join_exit_process_pid);
	//ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_SYSTEM,ENUM_IPC_LOG_LEVEL_INFO,
	//	mlist_member->pid,
	//	"progress [%s] join",join_exit_info->join_exit_process_name);

	return mlist_member;
}

void ipcd_exit(_join_exit_info *join_exit_info)
{
	ipcd_mlist_remove_via_pid(join_exit_info->join_exit_process_pid);
}



void *_ipcd_api_probe(void *param)
{
	_ipc_api *p = NULL;
	int relay_fun_flag=0;
	int relay_prg_flag=0;
	for(;;)
	{
		p = ipcd_api_pull();
		relay_fun_flag=0;
		relay_prg_flag=0;
		// distribute
		pthread_mutex_lock(&mlist_mutex);
		int member_size = dlist_len(mlist);
		int j,i=0;
		_mlist_member * tmlm;
		for(i=0;i<member_size;i++)
		{
			tmlm = dlist_get_data_safe(mlist,i);
			if(NULL == tmlm)
				continue;

			if(strcmp(tmlm->pname,p->pname))
				continue;
			
            if(tmlm->suspend)
            {
                i++;
                continue;
            }
			relay_prg_flag=1;
			
			for(j=0;j<dlist_len(tmlm->api_list);j++)
			{
				_m_member *m_member = dlist_get_data_safe(tmlm->api_list,j);
				if(m_member && !strcmp(p->api_name,m_member->msg_name))
				{
					//ipcd_relay_api(tmlm,m_member->msg_name,p->size,&p->data);
					ipcd_relay_api(tmlm,m_member->msg_name,p->size+sizeof(_ipc_api),p);
					ipcd_api_ack_list_push(p);
					//ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
					//	p->send_pid,
					//	"[%s] call api [%s]:[%s]",ipcd_get_pname_via_pid(p->send_pid,NULL),tmlm->pname,p->api_name);
					relay_fun_flag=1;
					break;
				}
			}
		}

		if(!relay_prg_flag)
		{
			_ipc_api_ack_list *iaal=malloc(sizeof(_ipc_api_ack_list));
			memset(iaal,0,sizeof(_ipc_api_ack_list));
			iaal->ack_result = ENUM_APT_ACK_CANTFIND_PROGRAM;
			iaal->ack_size = 0;
			iaal->ack_id= p->iaal.ack_id;
			_mlist_member * relay_mlm = ipcd_get_mlist_via_pid(p->send_pid);
			if(relay_mlm)
				ipcd_api_ack_relay(relay_mlm,sizeof(_ipc_api_ack_list),iaal);
			free(iaal);
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
				p->send_pid,
				"[%s] call api program[%s] miss",
				ipcd_get_pname_via_pid(p->send_pid,NULL),p->pname);		
			goto end;
		}
		
		if(!relay_fun_flag)
		{
			_ipc_api_ack_list *iaal=malloc(sizeof(_ipc_api_ack_list));
			memset(iaal,0,sizeof(_ipc_api_ack_list));
			iaal->ack_result = ENUM_APT_ACK_CANTFIND_API;
			iaal->ack_size = 0;
			iaal->ack_id= p->iaal.ack_id;
			_mlist_member * relay_mlm = ipcd_get_mlist_via_pid(p->send_pid);
			if(relay_mlm)
				ipcd_api_ack_relay(relay_mlm,sizeof(_ipc_api_ack_list),iaal);
			free(iaal);
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
				p->send_pid,
				"[%s] call api [%s] miss provider",
				ipcd_get_pname_via_pid(p->send_pid,NULL),p->api_name);
		}

end:
		free(p);
		pthread_mutex_unlock(&mlist_mutex);
	}
	return NULL;
}


void *_ipcd_msg_probe(void *param)
{
	_ipc_msg *p = NULL;
	int relay_flag=0;
	for(;;)
	{
		p = ipcd_msg_pull();
		if(NULL == p)
			continue;
		pthread_mutex_lock(&mlist_mutex);
		relay_flag=0;

		//if(ipcd_plugin_trigger(p->msg_name)>=0)
		//	relay_flag=1;

		// distribute
		int member_size = dlist_len(mlist);
		int j,i=0;
		_mlist_member * tmlm;
		for(i=0;i<member_size;)
		{
			tmlm  = dlist_get_data_safe(mlist,i);
			if(NULL == tmlm)
			{
				i++;
				continue;
			}			
            if(tmlm->suspend)
            {
                i++;
                continue;
            }
			
			int pmsg_list_size = dlist_len(tmlm->pmsg_list);
			for(j=0;j<pmsg_list_size;j++)
			{
				_m_member *m_member = dlist_get_data_safe(tmlm->pmsg_list,j);
				if(!m_member) continue;
				if(!strcmp(p->msg_name,m_member->msg_name) || !strcmp(m_member->msg_name,"*"))
				{
				/*
					int recv_mask_size =  dlist_len(tmlm->mask_receiver);
					for(j=0;j<recv_mask_size;j++)
					{
						DListNode *mask_name_node = dlist_get(tmlm->mask_receiver, j);
						if(mask_name_node)
						{
							if(!strcmp(mask_name_node->data,p->msg_name))
							{
								ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
								0,
								"[%s] recv mask msg form [%s]",p->msg_name,tmlm->pname);
								goto next;
							}
						}
					}
				*/
					ipcd_relay_msg(tmlm,p->msg_name,p->size+sizeof(_ipc_msg),p);
					relay_flag=1;
					ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
						p->send_pid,
						"[%s] send msg to [%s]",p->msg_name,tmlm->pname);
					break;
				}
			}
			i++;
		}

		if(!relay_flag)
		{
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
				p->send_pid,
				"[%s] send msg miss receiver",
				p->msg_name);
		}else
		{
		}
		if(p)
		{
			free(p);
			p=NULL;
		}
		pthread_mutex_unlock(&mlist_mutex);
	}
	return NULL;
}


void ipcd_api_ack_list_push(_ipc_api *ipc_api)
{
	if(!ipc_api)
		return;
	int size = sizeof(_ipc_api)+ipc_api->size;
	_ipc_api * tipc_api = (_ipc_api *)malloc(size);
	memcpy(tipc_api,ipc_api,size);
	pthread_mutex_lock(&api_ack_list_mutex);
	dlist_attach(recv_acklist,tipc_api);
	pthread_mutex_unlock(&api_ack_list_mutex);
}


void ipcd_api_push(_ipc_api *ipc_api)
{
	if(!ipc_api)
		return;
	int size = sizeof(_ipc_api)+ipc_api->size;
	_ipc_api * tipc_api = (_ipc_api *)malloc(size);
	memcpy(tipc_api,ipc_api,size);
	pthread_mutex_lock(&api_list_mutex);
	dlist_attach(recv_alist,tipc_api);
	pthread_mutex_unlock(&api_list_mutex);
	sem_post(&api_list_sem);
}

_ipc_api * ipcd_api_pull()
{
	_ipc_api * ret;
	sem_wait(&api_list_sem);
	pthread_mutex_lock(&api_list_mutex);
	_ipc_api * p = dlist_get_data_safe(recv_alist,0);
	if(p)
	{
		int size = sizeof(_ipc_api)+p->size;
		ret = (_ipc_api *)malloc(size);
		memcpy(ret,p,size);
		dlist_delete(recv_alist, 0);
	}
	pthread_mutex_unlock(&api_list_mutex);
	return ret;
}



void ipcd_msg_push(_ipc_msg *ipc_msg)
{
	if(!ipc_msg)
		return;
	int size = sizeof(_ipc_msg)+ipc_msg->size;
	_ipc_msg * tipc_msg = (_ipc_msg *)malloc(size);
	memcpy(tipc_msg,ipc_msg,size);
	pthread_mutex_lock(&msg_list_mutex);
	dlist_attach(recv_mlist,tipc_msg);
	pthread_mutex_unlock(&msg_list_mutex);
	sem_post(&msg_list_sem);
}

_ipc_msg * ipcd_msg_pull()
{
	_ipc_msg * ret;
	sem_wait(&msg_list_sem);
	pthread_mutex_lock(&msg_list_mutex);
	_ipc_msg *p = dlist_get_data_safe(recv_mlist,0);
	if(p)
	{
		int size = sizeof(_ipc_msg)+p->size;
		ret = (_ipc_msg *)malloc(size);
		memcpy(ret,p,size);
		dlist_delete(recv_mlist, 0);
	}
	pthread_mutex_unlock(&msg_list_mutex);
	return ret;
}


void ipcd_relay_msg(_mlist_member *mlm,char *mname,int size,void *data)
{	
	int sendsize = sizeof(_ipc_packet)+size;
	_ipc_packet *ipc_packet = malloc(sendsize);
	memset(ipc_packet,0,sendsize);
	ipc_packet->iit = ENUM_PROCESS_MSG_RELAY;
	ipc_packet->send_pid = mlm->pid;
	ipc_packet->size = size;
	memcpy(&ipc_packet->data,data,size);
	//mlm->toAddr.sin_port = htons(mlm->msg_port);
#if USE_UDP	
	sendto(eipcd_sock,ipc_packet,sendsize,0,(struct sockaddr*)&mlm->toAddr,sizeof(mlm->toAddr));
#endif
	free(ipc_packet);
}

void ipcd_relay_api(_mlist_member *mlm,char *mname,int size,void *data)
{
	int sendsize = sizeof(_ipc_packet)+size;
	_ipc_packet *ipc_packet = malloc(sendsize);
	memset(ipc_packet,0,sendsize);
	ipc_packet->iit = ENUM_IPC_CLI_API_RELAY;
	ipc_packet->send_pid = mlm->pid;
	ipc_packet->size = size;
	memcpy(&ipc_packet->data,data,size);
	//mlm->toAddr.sin_port = htons(mlm->api_port);
#if USE_UDP	
	sendto(eipcd_sock,ipc_packet,sendsize,0,(struct sockaddr*)&mlm->toAddr,sizeof(mlm->toAddr));
#endif
	free(ipc_packet);
}

void ipcd_join_ack(_mlist_member *mlm)
{
	int sendsize = sizeof(_ipc_packet);
	_ipc_packet *ipc_packet = malloc(sendsize);
	memset(ipc_packet,0,sendsize);
	ipc_packet->iit = ENUM_PROCESS_JOIN_ACK;
	ipc_packet->send_pid = mlm->pid;
	ipc_packet->size = 0;
	mlm->toAddr.sun_family = AF_UNIX;
	char ack_path[108]={0};
	sprintf(ack_path,"%s%s%s",IPC_CLI_SOCKET_PREFIX,mlm->pname,IPC_CLI_SOCKET_MSG_SUFFIX);
	strcpy(mlm->toAddr.sun_path,ack_path);
#if USE_UDP	
	//printf("ipcd_join_ack to addr %s\n",mlm->toAddr.sun_path);
	int ret = sendto(eipcd_sock,ipc_packet,sendsize,0,(struct sockaddr*)&mlm->toAddr,sizeof(mlm->toAddr));
	if(ret<0)
	{
		printf("ipcd_join_ack send ack fail\n");
	}
#endif
	free(ipc_packet);
}


void ipcd_join_ack_fail(int pid, int port,struct sockaddr_un *toAddr)
{
	int sendsize = sizeof(_ipc_packet);
	_ipc_packet *ipc_packet = malloc(sendsize);
	memset(ipc_packet,0,sendsize);
	ipc_packet->iit = ENUM_PROCESS_JOIN_ACK_FAIL;
	ipc_packet->send_pid = pid;
	ipc_packet->size = 0;
	//toAddr->sin_port = htons(port);
#if USE_UDP	
	sendto(eipcd_sock,ipc_packet,sendsize,0,(struct sockaddr*)toAddr,sizeof(struct sockaddr_un));
#endif
	free(ipc_packet);
}


void ipcd_api_ack_relay(_mlist_member *mlm,int size,void *data)
{
	int sendsize = sizeof(_ipc_packet)+size;

	if(!mlm)
		return;
	
	_ipc_packet *ipc_packet = malloc(sendsize);
	memset(ipc_packet,0,sendsize);
	ipc_packet->iit = ENUM_IPC_CLI_API_ACK_RELAY;
	ipc_packet->send_pid = mlm->pid;
	ipc_packet->size = size;
	memcpy(&ipc_packet->data,data,size);
	//mlm->toAddr.sin_port = htons(mlm->api_port);
	
#if USE_UDP	
	sendto(eipcd_sock,ipc_packet,sendsize,0,(struct sockaddr*)&mlm->toAddr,sizeof(struct sockaddr));
#endif
	free(ipc_packet);
}




void *ipcd_ctl_recv(void *param)
{
	int sock;
	struct sockaddr_un toAddr;
	struct sockaddr_un fromAddr;
	int recvLen;
	unsigned int addrLen;
	unlink(IPC_CTL_SOCKET);
	char *recvBuffer = malloc(IPC_CMDLINE_MAX_SIZE);
	sock = socket(AF_UNIX,SOCK_DGRAM,0);
	if(sock < 0)
	{
		printf("eipcd creat ctl socket fail.\r\n");
 		exit(0);
	}
	memset(&fromAddr,0,sizeof(fromAddr));
	fromAddr.sun_family=AF_UNIX;
	strcpy(fromAddr.sun_path, IPC_CTL_SOCKET);
	if(bind(sock,(struct sockaddr*)&fromAddr,sizeof(fromAddr))<0)
	{
		printf("eipcd bind ctl socket fail.\r\n");
 		close(sock);
 		exit(1);
	}
	while(1)
	{
		addrLen = sizeof(toAddr);
		memset(recvBuffer,0,IPC_CMDLINE_MAX_SIZE);
		if((recvLen = recvfrom(sock,recvBuffer,IPC_CMDLINE_MAX_SIZE,0,(struct sockaddr*)&toAddr,&addrLen))<0)
		{
			free(recvBuffer);
			printf("eipcd recvfrom ctl socket fail.\r\n");
	 		close(sock);
	 		exit(1);
		}
		if(recvLen>0)
		{
			pthread_mutex_lock(&mlist_mutex);
			ipcctl_analysis_core((ipc_cli_packet *)recvBuffer,&toAddr);
			pthread_mutex_unlock(&mlist_mutex);
		}
	}
}

#define MAX_IPC_CLIENT_NUM 256
void *ipcd_msg_recv(void *param)
{
	int sock;
	struct sockaddr_un toAddr;
	struct sockaddr_un fromAddr;
	int recvLen;
	unsigned int addrLen;
	char *recvBuffer = malloc(eipc_conf_p->ipc_msg_packet_max_size);
	unlink(IPC_MSG_SOCKET);
#if USE_UDP	
	sock = socket(AF_UNIX,SOCK_DGRAM,0);
#endif
	if(sock < 0)
	{
 		printf("eipcd creat msg recv socket fail.\r\n");
 		exit(0);
	}
	memset(&fromAddr,0,sizeof(fromAddr));
	fromAddr.sun_family=AF_UNIX;
	strcpy(fromAddr.sun_path, IPC_MSG_SOCKET);
	if(bind(sock,(struct sockaddr*)&fromAddr,sizeof(fromAddr))<0)
	{
 		printf("eipcd bind msg recv socket fail.\r\n");
 		close(sock);
 		exit(1);
	}else
	{
		//printf("ipcd_msg_recv bind ok\r\n");
	}

	
	while(1)
	{
#if USE_UDP
		addrLen = sizeof(toAddr);
		memset(recvBuffer,0,eipc_conf_p->ipc_msg_packet_max_size);
		if((recvLen = recvfrom(sock,recvBuffer,eipc_conf_p->ipc_msg_packet_max_size,0,(struct sockaddr*)&toAddr,&addrLen))<0)
		{
			printf("eipcd recvfrom msg recv socket fail.\r\n");
	 		close(sock);
	 		exit(1);
		}
		pthread_mutex_lock(&mlist_mutex);
		ipcd_analysis_core((_ipc_packet *)recvBuffer,&toAddr);
		pthread_mutex_unlock(&mlist_mutex);
#endif			
	}

}



void ipcctl_analysis_core(ipc_cli_packet *icp,struct sockaddr_un *toAddr)
{
	if(ENUM_IPC_CLI_HELP == icp->ict)
	{
		ipcd_misc_help();
		return;
	}else
	if(ENUM_IPC_CLI_CAT == icp->ict)
	{
		ipc_cli_print_packet *iclp = (ipc_cli_print_packet *)&icp->data;
		ipcd_misc_cat(iclp);
	}else
	if(ENUM_IPC_CLI_LS == icp->ict)
	{
		ipc_cli_ls_packet *iclp = (ipc_cli_ls_packet *)&icp->data;
		if(strlen(iclp->pname)==0)
		{
			ipcd_misc_ls_all();
		}else
		{
			ipcd_misc_ls_pname(iclp->pname);
		}
		return;

	}else
	if(ENUM_IPC_CLI_PRINT == icp->ict)
	{
		ipc_cli_print_packet *icpp = (ipc_cli_print_packet *)&icp->data;
		memcpy(printf_log_type_flag,icpp->printf_log_type_flag,16);
		memcpy(printf_log_level_flag,icpp->printf_log_level_flag,16);
		if(icpp->printf_broadcast_flag == 1)
		{
			printf("printf_broadcast_flag =1\r\n");
			printf_broadcast_flag = 1;
		}else
		{
			printf("printf_broadcast_flag =0\r\n");
			printf_broadcast_flag = 0;
		}
		
	}else
	if(ENUM_IPC_CLI_RPC == icp->ict)
	{
		ipc_cli_print_packet *icpp = (ipc_cli_print_packet *)&icp->data;
		memcpy(printf_log_type_flag,icpp->printf_log_type_flag,16);
		memcpy(printf_log_level_flag,icpp->printf_log_level_flag,16);
		if(printf_rpc_flag == 0)
		{
			printf_rpc_flag = 1;
			ipcd_console_print("eipc rpc enable\r\n");
		}
		else
		{
			ipcd_console_print("eipc rpc disable\r\n");
			printf_rpc_flag = 0;
		}
	}else
	if(ENUM_IPC_CLI_PLUGIN == icp->ict)
	{
		ipc_cli_plugin_packet *icpp=(ipc_cli_plugin_packet *)&icp->data;
		switch(icpp->type)
		{
			case ENUM_IPC_PLUGIN_FILE:
				ipcd_plugin_monitor_add_file(icpp->path);
				break;
			case ENUM_IPC_PLUGIN_DIR:
				ipcd_plugin_monitor_add_dir(icpp->path);
				break;
			case ENUM_IPC_PLUGIN_CLEAN:
				ipcd_plugin_monitor_msg_clean();
				ipcd_plugin_monitor_clean();
				break;
			case ENUM_IPC_PLUGIN_VIEW:
				ipcd_plugin_view();
				ipcd_plugin_monitor_msg_view();
				break;
			case ENUM_IPC_PLUGIN_REFRESH:
				ipcd_plugin_refresh();
				break;
			default:
				return;
		}
	}else
	if(ENUM_IPC_CLI_SCRIPT == icp->ict)
	{

	}else
	if(ENUM_IPC_CLI_API == icp->ict)
	{

	}else
	if(ENUM_IPC_CLI_MSG == icp->ict)
	{
		int member_size = dlist_len(mlist);
		int i=0;
		_mlist_member * tmlm=NULL;

		ipc_cli_msg_q *icmq = (ipc_cli_msg_q *)&icp->data;

		if(strlen(icmq->keyword[0])>0)
		{
			for(i=0;i<member_size;i++)
			{
				tmlm = dlist_get_data_safe(mlist,i);
				if(NULL == tmlm)
					continue;

				int j,msg_list_size=dlist_len(tmlm->pmsg_list);
				for(j=0;j<msg_list_size;j++)
				{
					_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->pmsg_list,j);
					if(!p) continue;
					if((!strcmp(p->msg_name,icmq->keyword[0])))
					{
						return;
					}
				}
			}	

			ipcd_console_print("\r\n");
			
			for(i=0;i<member_size;i++)
			{
				tmlm = dlist_get_data_safe(mlist,i);
				if(NULL == tmlm)
					continue;

				int j,msg_list_size=dlist_len(tmlm->pmsg_list);
				for(j=0;j<msg_list_size;j++)
				{
					_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->pmsg_list,j);
					if(!p) continue;
					int disflag=1;

					int kk=0;
					for(kk=0;kk<key_word_max_index;kk++)
					{	
						if(strlen(icmq->keyword[kk])>0)
						{
							if(!strstri(p->msg_name,icmq->keyword[kk]))
							{
								disflag=0;
							}
						}
					}
					if(disflag==1)
						ipcd_console_print("%s -> %s\r\n",tmlm->pname ,p->msg_name);
				}
			}
			ipcd_console_print("\r\n");
		}
		else
		{
			int dflag=0;
			ipcd_console_print("========================= message list =========================\r\n\r\n");
			for(i=0;i<member_size;i++)
			{
				tmlm = dlist_get_data_safe(mlist,i);
				if(NULL == tmlm)
					continue;
				dflag=0;
				int j,msg_list_size=dlist_len(tmlm->pmsg_list);
				for(j=0;j<msg_list_size;j++)
				{
					_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->pmsg_list,j);
					if(!p) continue;
					ipcd_console_print("%s -> %s\r\n",tmlm->pname,p->msg_name);
					dflag=1;
				}
				if(dflag==0)
				{
					ipcd_console_print("NULL\r\n");
				}
				ipcd_console_print("\n");
			}
			ipcd_console_print("\n\n");
			return;
		}
	}
	else
	{
		;
	}
}



void ipcd_analysis_core(_ipc_packet *ipc_packet,struct sockaddr_un *toAddr)
{
	static int api_public_magic_number=0;

	
	if(ENUM_IPC_API_REGISTER == ipc_packet->iit)
	{
		_register_info *register_info = (_register_info *)&ipc_packet->data;
		ipcd_register_api(register_info);
		return;
	}else
	if(ENUM_IPC_MSG_REGISTER == ipc_packet->iit)
	{
		_register_info *register_info = (_register_info *)&ipc_packet->data;
		ipcd_register_msg(register_info);
		return;
	}else
	if(ENUM_IPC_API_UNREGISTER == ipc_packet->iit)
	{
		_register_info *register_info = (_register_info *)&ipc_packet->data;
		ipcd_unregister_api(register_info);
		return;
	}else		
	if(ENUM_IPC_MSG_UNREGISTER == ipc_packet->iit)
	{
		_register_info *register_info = (_register_info *)&ipc_packet->data;
		ipcd_unregister_msg(register_info);
		return;
	}else
	if(ENUM_PROCESS_JOIN == ipc_packet->iit)
	{
		_join_exit_info *join_exit_info =(_join_exit_info *)&ipc_packet->data;
		_mlist_member * mlm = ipcd_join(join_exit_info,toAddr);
		if(mlm)
		{
			ipcd_join_ack(mlm);
		}else
			ipcd_join_ack_fail( join_exit_info->join_exit_process_pid,
								join_exit_info->msg_port,toAddr);
		return;
	}else

	if(ENUM_IPC_DAEMON == ipc_packet->iit)
	{
		_ipc_daemon_info * idi =(_ipc_daemon_info *)&ipc_packet->data;
		_mlist_member *mlm = ipcd_get_mlist_via_pid(idi->pid);
		if(mlm)
		{
			snprintf(mlm->daemon_cmd,IPC_SYSCMD_SIZE,"%s",idi->daemon_cmd);
		}
		return;
	}else
	if(ENUM_PROCESS_EXIT == ipc_packet->iit)
	{
		_join_exit_info *join_exit_info =(_join_exit_info *)&ipc_packet->data;
		//ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
		//	ipc_packet->send_pid,
		//	"[%s] progress exit",join_exit_info->join_exit_process_name);
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_SYSTEM,ENUM_IPC_LOG_LEVEL_INFO,
			ipc_packet->send_pid,
			"[%s] progress exit",join_exit_info->join_exit_process_name);
		ipcd_exit(join_exit_info);
		return;
	}else
	if(ENUM_PROCESS_LIVE == ipc_packet->iit)
	{
		_join_exit_info *join_exit_info =(_join_exit_info *)&ipc_packet->data;
		_mlist_member *t_mlm = ipcd_get_mlist_via_pid(join_exit_info->join_exit_process_pid);
		if(t_mlm==NULL)
		{
			//printf("-------- %s not live\n",join_exit_info->join_exit_process_name);
			return;
		}
		t_mlm->live_cnt=0;
        t_mlm->suspend=0;
		return;
	}
	if(ENUM_PROCESS_HANGUP == ipc_packet->iit)
	{
		_join_exit_info *join_exit_info =(_join_exit_info *)&ipc_packet->data;
		ipcd_exit(join_exit_info);
		char exit_reason[IPC_MSG_MAX_SIZE]={0};
		switch(join_exit_info->signal_no)
		{
			case 2:
				snprintf(exit_reason,IPC_MSG_MAX_SIZE,"user ctrl^c exit");
				break;
			case 20:
				snprintf(exit_reason,IPC_MSG_MAX_SIZE,"user ctrl^z exit");
				break;
			case 15:
				snprintf(exit_reason,IPC_MSG_MAX_SIZE,"user execute kill");
				break;
			case 11:
				snprintf(exit_reason,IPC_MSG_MAX_SIZE,"memory overflow");
				break;
			default:
				snprintf(exit_reason,IPC_MSG_MAX_SIZE,"%d",join_exit_info->signal_no);
				break;
		}
		
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_SYSTEM,ENUM_IPC_LOG_LEVEL_ERR,
			ipc_packet->send_pid,
			"[%s] progress hangup , reason:[%s]",
			join_exit_info->join_exit_process_name,exit_reason);
		return;
	}else
	if(ENUM_PROCESS_API_CALL == ipc_packet->iit)
	{
		_ipc_api *ipc_api = (_ipc_api *)&ipc_packet->data;
		_mlist_member *mmlist = ipcd_get_mlist_via_pid(ipc_packet->send_pid);

		if(NULL!=mmlist)
		{
			mmlist->live_cnt=0;
			mmlist->suspend=0;
			mmlist->send_cnt++;
			ipc_api->daemon_magic_number=api_public_magic_number++;
			//ipc_api->daemon_count_number=DEFAULT_CALL_TIMEOUT;
			ipcd_api_push(ipc_api);
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
				ipc_packet->send_pid,
				"[%s] call api [%s]:[%s]",
				ipcd_get_pname_via_pid(ipc_api->send_pid,NULL),ipc_api->pname,ipc_api->api_name);
		}
		return;
	}else		
	if(ENUM_PROCESS_MSG_SEND == ipc_packet->iit)
	{
		_ipc_msg *ipc_msg = (_ipc_msg *)&ipc_packet->data;
		_mlist_member *mmlist = ipcd_get_mlist_via_pid(ipc_packet->send_pid);

		/*
		int j=0,send_mask_size = dlist_len(mmlist->mask_sender);
		for(j=0;j<send_mask_size;j++)
		{
			DListNode *mask_name_node = dlist_get(mmlist->mask_sender, j);
			if(mask_name_node)
			{
				if(!strcmp(mask_name_node->data,ipc_msg->msg_name))
				{
					ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
						0,
						"[%s] send mask msg form [%s]",ipc_msg->msg_name,mmlist->pname);
					break;
				}
			}
		}
		*/
		if(NULL!=mmlist)
		{
			mmlist->live_cnt=0;
			mmlist->suspend=0;
			mmlist->send_cnt++;
			ipcd_msg_push(ipc_msg);
		}
		return;
	}else
	if(ENUM_PROCESS_API_RUNTIME_RECODE == ipc_packet->iit)
	{
		_ipc_runtime *ipc_runtime = (_ipc_runtime *)&ipc_packet->data;
		int ack_succeess_flag=0;
		if(ipc_runtime->flag)
		{	// process finish
			_mlist_member *mmlist = ipcd_get_mlist_via_pid(ipc_packet->send_pid);
			if(NULL!=mmlist)
			{
				int i=0,api_size = dlist_len(mmlist->api_list);
				mmlist->recv_cnt++;

				for(i=0;i<api_size;i++)
				{
					_m_member *a_member = dlist_get_data_safe(mmlist->api_list,i);
					if(!a_member) continue;
					if((!strcmp(a_member->msg_name,ipc_runtime->msg_name)))
					{
						a_member->process_time = timespec_sum(a_member->process_time,ipc_runtime->ptime);
						a_member->process_cnt++;

						pthread_mutex_lock(&api_ack_list_mutex);
						int acklist_size=dlist_len(recv_acklist),k=0;
						_ipc_api *ipc_api = NULL;
						for(k=0;k<acklist_size;k++)
						{
							ipc_api = dlist_get_data_safe(recv_acklist,k);
							if(!ipc_api) continue;
							if((ipc_runtime->daemon_magic_number==ipc_api->daemon_magic_number))
							{
								dlist_remove(recv_acklist,k);
								_ipc_api_ack_list *iaal=malloc(ipc_runtime->ret_size+sizeof(_ipc_api_ack_list));
								memset(iaal,0,ipc_runtime->ret_size+sizeof(_ipc_api_ack_list));
								iaal->ack_result = ipc_runtime->ret;
								iaal->ack_size = ipc_runtime->ret_size;
								iaal->ack_id= ipc_runtime->caller_magic_number;
								if(iaal->ack_size)
									memcpy(&iaal->ack_data,&ipc_runtime->ret_data,ipc_runtime->ret_size);
								_mlist_member * relay_mlm = ipcd_get_mlist_via_pid(ipc_api->send_pid);
								if(relay_mlm)
									ipcd_api_ack_relay(relay_mlm,ipc_runtime->ret_size+sizeof(_ipc_api_ack_list),iaal);
								
								free(iaal);
								free(ipc_api);
								ack_succeess_flag=1;
								break;
							}
						}
						pthread_mutex_unlock(&api_ack_list_mutex);
						if(!ack_succeess_flag)
						{
							ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
								ipc_packet->send_pid,
								"[%s] api finish , time:%05d.%03d , time is too long, api caller miss!!",
								ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
							return;
						}else
						{

							if(ipc_runtime->ptime.tv_sec>=1||(ipc_runtime->ptime.tv_nsec/1000000)>500)
							{
								ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
									ipc_packet->send_pid,
									"[%s] api finish , time:%05d.%03d , time is too long",
									ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
							}else
							{			
								ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
									ipc_packet->send_pid,
									"[%s] api finish , time:%05d.%03d",
									ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
							}
						}
						return;
					}
				}
			}

			_ipc_api_ack_list *iaal=malloc(ipc_runtime->ret_size+sizeof(_ipc_api_ack_list));
			memset(iaal,0,ipc_runtime->ret_size+sizeof(_ipc_api_ack_list));
			iaal->ack_result = ipc_runtime->ret;
			iaal->ack_size = ipc_runtime->ret_size;
			iaal->ack_id= ipc_runtime->caller_magic_number;
			if(iaal->ack_size)
				memcpy(&iaal->ack_data,&ipc_runtime->ret_data,ipc_runtime->ret_size);
			_mlist_member * relay_mlm = ipcd_get_mlist_via_pid(ipc_packet->send_pid);
			if(relay_mlm)
			{
				ipcd_api_ack_relay(relay_mlm,ipc_runtime->ret_size+sizeof(_ipc_api_ack_list),iaal);

				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
							ipc_packet->send_pid,
							"[%s] api finish miss , time:%05d.%03d",
							ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
			}
		}
		else
		{	// process begin
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
				ipc_packet->send_pid,
				"[%s] api begin",
				ipc_runtime->msg_name);
		}
	}
	else
	if(ENUM_PROCESS_MSG_RUNTIME_RECODE == ipc_packet->iit)
	{
		_ipc_runtime *ipc_runtime = (_ipc_runtime *)&ipc_packet->data;
		int General_matching_en=0;
		if(ipc_runtime->flag)
		{	// process finish
			_mlist_member *mmlist = ipcd_get_mlist_via_pid(ipc_packet->send_pid);
			
			if(NULL!=mmlist)
			{
				int i=0,pmsg_size = dlist_len(mmlist->pmsg_list);
				mmlist->recv_cnt++;

				for(i=0;i<pmsg_size;i++)
				{
					_m_member *a_member =  dlist_get_data_safe(mmlist->pmsg_list,i);
					if(!a_member) continue;
					if((!strcmp(a_member->msg_name,ipc_runtime->msg_name)))
					{
						a_member->process_time = timespec_sum(a_member->process_time,ipc_runtime->ptime);
						a_member->process_cnt++;

						if(ipc_runtime->ptime.tv_sec>=1||(ipc_runtime->ptime.tv_nsec/1000000)>500)
						{
							ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
								ipc_packet->send_pid,
								"[%s] msg finish , time:%05d.%03d , time is too long",
								ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
						}else
						{			
							ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
								ipc_packet->send_pid,
								"[%s] msg finish , time:%05d.%03d",
								ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
						}
						return;
					}

					if(!strcmp(a_member->msg_name,"*"))
					{
						General_matching_en=1;
					}
				}				
				
			}

			if(General_matching_en==0)	// find register * give all msg 
				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_ERR,
							ipc_packet->send_pid,
							"[%s] msg finish miss , time:%05d.%03d",
							ipc_runtime->msg_name,ipc_runtime->ptime.tv_sec,ipc_runtime->ptime.tv_nsec/1000000);
		}
		else
		{	// process begin
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
				ipc_packet->send_pid,
				"[%s] msg begin",
				ipc_runtime->msg_name);
		}
	}
	if(ENUM_PROCESS_LOG == ipc_packet->iit)
	{
		
		_ipc_log *il = (_ipc_log *)&ipc_packet->data;
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_USER,il->level,
						ipc_packet->send_pid,
						&il->data);
		return;
	}else
	{
	}

}



void ipcd_init()
{
	if(!mlist)
	{
		mlist = dlist_create();
		if(!mlist)
		{
			printf("ipcd_init mlist creat fail\r\n");
			exit(0);
		}
	}

	sem_init(&msg_list_sem,1,0);
	sem_init(&api_list_sem,1,0);

	if(!recv_mlist)
	{
		recv_mlist = dlist_create();
		if(!recv_mlist)
		{
			printf("ipcd_init recv_mlist creat fail\r\n");
			exit(0);
		}
	}
	
	if(!recv_alist)
	{
		recv_alist = dlist_create();
		if(!recv_alist)
		{
			printf("ipcd_init recv_mlist creat fail\r\n");
			exit(0);
		}
	}

	if(!recv_acklist)
	{
		recv_acklist = dlist_create();
		if(!recv_acklist)
		{
			printf("ipcd_init recv_mlist creat fail\r\n");
			exit(0);
		}		
	}


socket_retry_01:
	eipcd_sock = socket(AF_UNIX,SOCK_DGRAM,0);
	if(eipcd_sock<=0)
	{
		printf("eipcd_sock faile , socket_retry_01\n");
		usleep(10*1000);
		goto socket_retry_01;
	}
}


void ipcd_msg_loop()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, ipcd_msg_recv,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	} 
	pthread_attr_destroy(&threadAttr);	
}

void ipcd_ctl_loop()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, ipcd_ctl_recv,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	} 
	pthread_attr_destroy(&threadAttr);	
}


void ipcd_api_probe()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, _ipcd_api_probe,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	} 
	pthread_attr_destroy(&threadAttr);	
}


void ipcd_msg_probe()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr, _ipcd_msg_probe,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	} 
	pthread_attr_destroy(&threadAttr);	
}

void ipcd_ctl_probe()
{
	pthread_t tid_token;
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);
	if(0 !=pthread_create(&tid_token, &threadAttr,ipcd_ctl_recv,NULL))	
	{  
		pthread_attr_destroy(&threadAttr);
		return;
	} 
	pthread_attr_destroy(&threadAttr);	
}

int main(int argc , char *argv[])
{
	int oc; 					/*选项字符 */
	char *b_opt_arg;			/*选项参数字串 */
	int daemon_flag = 0;
	while((oc = getopt(argc, argv, "l:t:L:dh")) != -1)
	{
		switch(oc)
		{		
			case 't':
				debug_log_size = atoi(optarg);
				break;
			case 'L':
				snprintf(printf_log_level_flag,16,"%s",optarg);	// 默认打印error的log信息
				break;
			//case 'd':
			//	daemon_flag=1;
			//	break;
			case 'h':
			case '?':
				printf("arguments error!\n");
				printf("-l , specify log file save path if you need , defalut path is /tmp/easyipc/easyipc.log\r\n");
				printf("-L , specify log printf level: EWNID\r\n");
				printf("-t , type of log , -1:not save , 0: save to /tmp , big than 1: save to memory and max size is you input value \r\n");
				//printf("-d , execute at background\r\n");
				printf("%s [-l log path] [-t x.x.x.x] [-d] \n",argv[0]);
				exit(0);
		}	
	}	

	//if(daemon_flag==1)
	//	daemon(0,0);
	#if 0 //disable for local socket
	if(find_netif("lo")<0)
	{
		printf("eipcd start fail , netcard lo cant find!!!\n");
		return 0;
	}
	#endif

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
		printf("easyipc reset packet_max_size = %d\n",value);
		eipc_conf_p->ipc_msg_packet_max_size = value;
	}

	ret = GetConfigIntValue(IPC_CONF_FILE_PATH,"Config","ipc_syslog_enable",&value);
	if(!ret)
	{
		printf("easyipc reset ipc_syslog_enable = %d\n",value);
		eipc_conf_p->ipc_syslog_enable = value;
	}

	char save_path[IPC_LOG_SAVE_PATH_MAX_LENS]={0};
	ret = GetConfigStringValue(IPC_CONF_FILE_PATH,"Config","ipc_log_save_path",save_path);
	if(!ret)
	{
		printf("easyipc reset ipc_syslog_enable = %s\n",save_path);
		snprintf(eipc_conf_p->ipc_log_save_path,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",save_path);
	}	


	ipcd_init();
	ipcd_debug_init();
	ipcd_msg_loop();
	ipcd_ctl_loop();
	ipcd_msg_probe();
	ipcd_api_probe();
	ipc_api_timeout_check_thread_creat();
	ipc_live_thread_creat();
	pause();
	return 0;
}

void remove_socket_link_file(char *base_name,int port)
{
	char link_path[20]= {0};
	sprintf(link_path,"%s%d",base_name,port);
	printf("unlink linkpath = %s\n",link_path);
	unlink(link_path);
}


