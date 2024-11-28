#include "easyipc_base.h"
#include "easyipc_daemon.h"
#include "easyipc_console.h"
#include "dlist.h"
#include "easyipc_debug.h"
#include "easyipc_misc.h"
#include <netinet/if_ether.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <sys/stat.h>  

static int ipcd_debug_cat_is_enable_flag=0;
static int ipcd_debug_cat_pid=0;
extern int debug_log_size;
extern char printf_broadcast_flag;
extern char printf_rpc_flag;

int ipcd_debug_log_memory_pull(char *str,int maxsize);


int ipcd_debug_get_cat_pid()
{
	return ipcd_debug_cat_pid;
}

int ipcd_debug_cat_is_enable()
{
	return ipcd_debug_cat_is_enable_flag;
}


void ipcd_misc_help()
{
	ipcd_console_print(
		"\r\n\r\n"
		"easyipc version: %s\r\n"
		"eipchelp:  help程序\r\n"
		"eipcls:    查看所有进程消息状态统计\r\n"
		"eipccat:   打印所有进程/指定进程执行记录\r\n"
		"eipcplugin:指定插件目录或者文件,json格式\r\n"
		"eipcdebug: 开启/关闭 debug功能\r\n"
		"eipcapi:   发送指定的api,用于调试\r\n"
		"eipcmsg:   发送指定的消息,用于调试\r\n"
		"eipcscript:执行指定脚本\r\n"
		"eipcprint: 指定打印的等级"
		"\r\n\r\n"
		,eipc_version
		);
}


static void ipcd_misc_del_tail_spec(char *str)
{
	int i,size = strlen(str);
	for(i=size;i>0;i--)
	{
		if(*(str+i)==' ')
			*(str+i)=0;
	}
}


unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return filesize;  
    }else{  
        filesize = statbuff.st_size;  
    }  
    return filesize;  
}    

char* strstri(char* str, const char* subStr)
{
   int len = strlen(subStr);
   if(len == 0)
   {
	   return NULL;
   }

   while(*str)
   {
	   if(strncasecmp(str, subStr, len) == 0)
	   {
		   return str;
	   }
	   ++str;
   }
   return NULL;
}
void ipcd_misc_cat(ipc_cli_print_packet *icpp)
{
	char org_path[128]={0};
	char cmd[256]={0};
	char readline[IPC_CMDLINE_MAX_SIZE];
	size_t len = 0;
	char temp_str_pname[33];
	char tmplevel[2];
	char tmptype[2];
	int readsize;
	char last_color;

	if(debug_log_size==0)
	{
		

		snprintf(org_path,128,"%s",eipc_conf_p->ipc_log_save_path);
		FILE *cat_log_fp = fopen(org_path, "r"); 
		if(!cat_log_fp)
		{
			ipcd_console_print("log file not exist\r\n");
			return;
		}
		
		//fseek(cat_log_fp,0L,SEEK_SET);
		usleep(100*1000);
		ipcd_console_print("\r\n");
		while (fgets(readline, IPC_CMDLINE_MAX_SIZE, cat_log_fp))
		{  
			memset(temp_str_pname,0,33);
			memcpy(temp_str_pname,readline+4,32);

			ipcd_misc_del_tail_spec(temp_str_pname);
			
			tmptype[0]=*(readline+0);
			tmplevel[0]=*(readline+2);
			tmptype[1]=0;tmplevel[1]=0;
			
			if(strlen(icpp->printf_log_pname)>0)
			{
				if(strcmp(icpp->printf_log_pname,temp_str_pname))
					continue;
			}

			if(strlen(icpp->key_word)>0)
			{
				if(strstri(readline+33,icpp->key_word)==NULL)
					continue;
			}
			
			if(strlen(readline)<=48)
			{
				switch(last_color)
				{
					case 'E':
						ipcd_console_print(RED);
						break;
					case 'W':
						ipcd_console_print(YELLOW);
						break;
					case 'N':
						ipcd_console_print(L_GREEN);
						break;
					case 'I':
						ipcd_console_print(BOLD);
						break;
					case 'D':
						ipcd_console_print(L_PURPLE);
						break;
					default:
						ipcd_console_print(RED);
						break;
				}
			
				ipcd_console_print("%s",readline);
				ipcd_console_print(NONE);	
				continue;
			}
			else
			{
				if(!strstr("USP",tmptype)||!strstr("EWNI",tmplevel))
				{
					switch(last_color)
					{
						case 'E':
							ipcd_console_print(RED);
							break;
						case 'W':
							ipcd_console_print(YELLOW);
							break;
						case 'N':
							ipcd_console_print(L_GREEN);
							break;
						case 'I':
							ipcd_console_print(BOLD);
							break;
						case 'D':
							ipcd_console_print(L_PURPLE);
							break;
						default:
							ipcd_console_print(RED);
							break;
					}
					ipcd_console_print("%s",readline);
					ipcd_console_print(NONE);	
					continue;				
				}
			}
			
			// "PSEWNI"
			if(strstr(icpp->printf_log_type_flag,tmptype)&&strstr(icpp->printf_log_level_flag,tmplevel))
			{
				last_color = tmplevel[0];
				switch(tmplevel[0])
				{
					case 'E':
						ipcd_console_print(RED);
						break;
					case 'W':
						ipcd_console_print(YELLOW);
						break;
					case 'N':
						ipcd_console_print(L_GREEN);
						break;
					case 'I':
						ipcd_console_print(BOLD);
						break;
					case 'D':
						ipcd_console_print(L_PURPLE);
						break;
					default:
						ipcd_console_print(RED);
						break;
				}
			
				ipcd_console_print("%s",readline);
				ipcd_console_print(NONE);
			}
			
		} 
		//if (readline)  
		//	free(readline);  
		ipcd_console_print("\r\n");
	}
	else if(debug_log_size>0)
	{

		printf("\r\n");
		while (ipcd_debug_log_memory_pull(readline, IPC_CMDLINE_MAX_SIZE))
		{  
			memset(temp_str_pname,0,33);
			memcpy(temp_str_pname,readline+4,32);

			ipcd_misc_del_tail_spec(temp_str_pname);
			
			tmptype[0]=*(readline+0);
			tmplevel[0]=*(readline+2);
			tmptype[1]=0;tmplevel[1]=0;
			
			if(strlen(icpp->printf_log_pname)>0)
			{
				if(strcmp(icpp->printf_log_pname,temp_str_pname))
					continue;
			}

			if(strlen(icpp->key_word)>0)
			{
				if(strstri(readline+33,icpp->key_word)==NULL)
					continue;
			}
			
			if(strlen(readline)<=48)
			{
				switch(last_color)
				{
					case 'E':
						ipcd_console_print(RED);
						break;
					case 'W':
						ipcd_console_print(YELLOW);
						break;
					case 'N':
						ipcd_console_print(L_GREEN);
						break;
					case 'I':
						ipcd_console_print(BOLD);
						break;
					case 'D':
						ipcd_console_print(L_PURPLE);
						break;
					default:
						ipcd_console_print(RED);
						break;
				}
			
				ipcd_console_print("%s\n",readline);
				ipcd_console_print(NONE);	
				continue;
			}
			else
			{
				if(!strstr("USP",tmptype)||!strstr("EWNI",tmplevel))
				{
					switch(last_color)
					{
						case 'E':
							ipcd_console_print(RED);
							break;
						case 'W':
							ipcd_console_print(YELLOW);
							break;
						case 'N':
							ipcd_console_print(L_GREEN);
							break;
						case 'I':
							ipcd_console_print(BOLD);
							break;
						case 'D':
							ipcd_console_print(L_PURPLE);
							break;
						default:
							ipcd_console_print(RED);
							break;
					}
					ipcd_console_print("%s\n",readline);
					ipcd_console_print(NONE);	
					continue;				
				}
			}
			
			// "PSEWNID"
			if(strstr(icpp->printf_log_type_flag,tmptype)&&strstr(icpp->printf_log_level_flag,tmplevel))
			{
				last_color = tmplevel[0];
				switch(tmplevel[0])
				{
					case 'E':
						ipcd_console_print(RED);
						break;
					case 'W':
						ipcd_console_print(YELLOW);
						break;
					case 'N':
						ipcd_console_print(L_GREEN);
						break;
					case 'I':
						ipcd_console_print(BOLD);
						break;
					case 'D':
						ipcd_console_print(L_PURPLE);
						break;
					default:
						ipcd_console_print(RED);
						break;
				}
			
				ipcd_console_print("%s\n",readline);
				ipcd_console_print(NONE);
			}
			
		} 
		//if (readline)  
		//	free(readline);  
		ipcd_console_print("\r\n");
	}
	
}

void ipcd_misc_ls_all()
{
	DList *_mlist=ipcd_get_mlist();
	int member_size = dlist_len(ipcd_get_mlist());
	int i=0;
	
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(ipcd_get_mlist(),i);
		if(NULL == tmlm)
			continue;

		ipcd_console_print("\r\n\r\n");
		ipcd_console_print("===================================	progress state	==================================\r\n");
		ipcd_console_print("progress name: %s\r\n",tmlm->pname);
		ipcd_console_print("progress id: %d\r\n",tmlm->pid);
		ipcd_console_print("progress send count: %d\r\n",tmlm->send_cnt);
		ipcd_console_print("progress recv count: %d\r\n",tmlm->recv_cnt);
		ipcd_console_print("\r\n");
		ipcd_console_print("%-32s%-12s%-12s\r\n","api name","call count","used time");
		ipcd_console_print("--------------------------------------------------------------------------------------\r\n");
		
		int j,api_list_size=dlist_len(tmlm->api_list);
		for(j=0;j<api_list_size;j++)
		{
			_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->api_list,j);
			if(!p) continue;
			ipcd_console_print("%-32s%-12d%4ld.%03ld\r\n",p->msg_name,p->process_cnt,
				p->process_time.tv_sec,p->process_time.tv_nsec/1000000);
		}
		ipcd_console_print("\r\n\r\n");
		ipcd_console_print("%-32s%-12s%-12s\r\n","msg name","recv count","used time");
		ipcd_console_print("--------------------------------------------------------------------------------------\r\n");
		api_list_size=dlist_len(tmlm->pmsg_list);
		for(j=0;j<api_list_size;j++)
		{
			_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->pmsg_list,j);
			if(!p) continue;
			ipcd_console_print("%-32s%-12d%4ld.%03ld\r\n",p->msg_name,p->process_cnt,
				p->process_time.tv_sec,p->process_time.tv_nsec/1000000);
		}
		
	}
	ipcd_console_print("\r\n\r\n");
}


void ipcd_misc_ls_pname(char *pname)
{
	int member_size = dlist_len(ipcd_get_mlist());
	int i=0;
	_mlist_member * tmlm;
	for(i=0;i<member_size;i++)
	{
		tmlm = dlist_get_data_safe(ipcd_get_mlist(),i);
		if(NULL == tmlm)
			continue;

		if(!strcmp(tmlm->pname,pname))
		{
			ipcd_console_print("\r\n\r\n");
			ipcd_console_print("==============================  progress state  ======================================\r\n");
			ipcd_console_print("progress name: %s\r\n",tmlm->pname);
			ipcd_console_print("progress id: %d\r\n",tmlm->pid);
			ipcd_console_print("progress send count: %d\r\n",tmlm->send_cnt);
			ipcd_console_print("progress recv count: %d\r\n",tmlm->recv_cnt);
			ipcd_console_print("\r\n");
			ipcd_console_print("%-32s%-12s%-12s\r\n","api name","call count","used time");
			ipcd_console_print("--------------------------------------------------------------------------------------\r\n");
			
			int j,api_list_size=dlist_len(tmlm->api_list);
			for(j=0;j<api_list_size;j++)
			{
				_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->api_list,j);
				if(!p) continue;
				ipcd_console_print("%-32s%-12d%4ld.%03ld\r\n",p->msg_name,p->process_cnt,
					p->process_time.tv_sec,p->process_time.tv_nsec/1000000);
			}
			ipcd_console_print("\r\n\r\n");
			ipcd_console_print("%-32s%-12s%-12s\r\n","msg name","recv count","used time");
			ipcd_console_print("--------------------------------------------------------------------------------------\r\n");
			for(j=0;j<api_list_size;j++)
			{
				_m_member *p = (_m_member *)dlist_get_data_safe(tmlm->pmsg_list,j);
				if(!p) continue;
				ipcd_console_print("%-32s%-12d%4ld.%03ld\r\n",p->msg_name,p->process_cnt,
					p->process_time.tv_sec,p->process_time.tv_nsec/1000000);
			}
			ipcd_console_print("\r\n\r\n");
			return;
		}
	}
	ipcd_console_print("not find progress [%s]\r\n",pname);
}

void ipcd_console_local_broadcast(char *fmt, ...)
{
	static int flag=0;
	static int sock = -1;
	static struct sockaddr_un addrto;
	static int nlen=sizeof(addrto);

	static int sock_rpc = -1;
	static struct sockaddr_un addrto_rpc;
	static int nlen_rpc=sizeof(addrto_rpc);
	if(printf_rpc_flag != 0)
	{
		if(sock_rpc<=0)
		{
			bzero(&addrto_rpc, sizeof(struct sockaddr_un));
			addrto_rpc.sun_family=AF_UNIX;
			if((sock_rpc = socket(AF_UNIX,SOCK_DGRAM,0))==-1)
			{	
				printf("ipcd local console socket fail\r\n");
				return ;
			}
		}

		if(sock_rpc>0)
		{
			char sprint_buf[IPC_DEBUG_PRINT_MAX_SIZE*4]={0};
			
			va_list args;
			int n;
			va_start(args, fmt);
			n = vsnprintf(sprint_buf, IPC_DEBUG_PRINT_MAX_SIZE*4,fmt, args);
			va_end(args);
			sendto(sock_rpc, sprint_buf, strlen(sprint_buf), 0, (struct sockaddr*)&addrto_rpc, nlen_rpc);
		}
	}

	if(printf_broadcast_flag != 0)
	{
		if(sock<=0)
		{
			bzero(&addrto, sizeof(struct sockaddr_un));
			addrto.sun_family=AF_UNIX;
#if 0	
			if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
			{	
				printf("ipcd local console socket fail\r\n");
				return ;
			}	
			
			const int opt = 1;
			int nb = 0;
			nb = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
			if(nb == -1)
			{
				printf("ipcd local console set socket error...");
				return ;
			}
		
			
			addrto.sin_addr.s_addr=htonl(INADDR_BROADCAST);
			
#else
			if((sock = socket(AF_UNIX,SOCK_DGRAM,0))==-1)
			{	
				printf("ipcd local console socket fail\r\n");
				return ;
			}
#endif
		}

		if(sock>0)
		{
			char sprint_buf[IPC_DEBUG_PRINT_MAX_SIZE*4]={0};
			strcpy(addrto.sun_path,IPC_CONSOLE_BROADCAST_SOCKET);
			va_list args;
			int n;
			va_start(args, fmt);
			n = vsnprintf(sprint_buf, IPC_DEBUG_PRINT_MAX_SIZE*4,fmt, args);
			va_end(args);
		
			sendto(sock, sprint_buf, strlen(sprint_buf), 0, (struct sockaddr*)&addrto, sizeof(addrto));
		}
	}
	
}


