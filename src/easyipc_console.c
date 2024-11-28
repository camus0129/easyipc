#include "easyipc_base.h"
#include "easyipc_daemon.h"
#include "easyipc_console.h"
#include <fcntl.h>

#define NONE                 "\e[0m"
#define BLACK                "\e[0;30m"
#define L_BLACK              "\e[1;30m"
#define RED                  "\e[0;31m"
#define L_RED                "\e[1;31m"
#define GREEN                "\e[0;32m"
#define L_GREEN              "\e[1;32m"
#define BROWN                "\e[0;33m"
#define YELLOW               "\e[1;33m"
#define BLUE                 "\e[0;34m"
#define L_BLUE               "\e[1;34m"
#define PURPLE               "\e[0;35m"
#define L_PURPLE             "\e[1;35m"
#define CYAN                 "\e[0;36m"
#define L_CYAN               "\e[1;36m"
#define GRAY                 "\e[0;37m"
#define WHITE                "\e[1;37m"

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" //or "\e[1K\r"

/*
 	eipchelp:									help程序
	eipcls:										查看所有进程消息状态统计
 	eipcls [-n pname]:							查看制定进程消息状态统计
 	eipccat [-n pname]:							打印所有进程/指定进程执行记录
 	eipcplugin [-n pname]:						挂载制定进程用于作为通讯插件
 	eipcapi [-s api -j param -f json参数文件]  		发送指定的api功能,参数使用json格式
 		不带任何参数的情况下打印所有可以执行的api,并打印JSON格式说明
	eipcmsg [-s msg -j param -f json参数文件]		广播制定的消息
		不带任何参数的情况下打印所有可以执行的msg,并打印JSON格式说明
	eipcscript [-f script文件]					执行指定脚本
		-h 基本语法提示
	eipcprint [-l 0/1/2 ]						查看程序指定进程
*/


struct sockaddr_un eipcc_toAddr;
static int eipcc_sock=-1;
easyipc_config *eipc_conf_p_4cat = NULL;

static void ipc_misc_del_tail_spec(char *str)
{
	int i,size = strlen(str);
	for(i=size;i>0;i--)
	{
		if(*(str+i)==' ')
			*(str+i)=0;
	}
}


void eipcc_connect_daemon()
{
	eipcc_sock = socket(AF_UNIX,SOCK_DGRAM,0);
	if(eipcc_sock<0)
	{
		printf("ipc_connect_daemon udp socket creat fail\r\n");
		return;
	}

	eipcc_toAddr.sun_family=AF_UNIX;
	strncpy(eipcc_toAddr.sun_path, IPC_CTL_SOCKET, sizeof(eipcc_toAddr.sun_path) - 1);
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

void ipc_cat_deamon(char *level,char *mode,char *pname,char *keyword,int offset)
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
	int flags;

	snprintf(org_path,128,"%s",eipc_conf_p_4cat->ipc_log_save_path);

	FILE *cat_log_fp = fopen(org_path, "r"); 
	if(!cat_log_fp)
	{
		printf("log file not exist\r\n");
		return;
	}

	if(offset > 0)
	{
		fseek(cat_log_fp,offset,SEEK_SET);
	}else
	if(offset <0)
	{
		fseek(cat_log_fp,offset,SEEK_END);
	}

	usleep(100*1000);
	printf("\r\n");
	while (1)
	{  
		if(!fgets(readline, IPC_CMDLINE_MAX_SIZE, cat_log_fp))
		{	
			usleep(100*100);
			continue;
		}

		if(strlen(keyword)>0)
		{
			if(strstri(readline+33,keyword)==NULL)
				continue;
		}
	
		memset(temp_str_pname,0,33);
		memcpy(temp_str_pname,readline+4,32);

		ipc_misc_del_tail_spec(temp_str_pname);
		
		tmptype[0]=*(readline+0);
		tmplevel[0]=*(readline+2);
		tmptype[1]=0;tmplevel[1]=0;
		
		if(strlen(pname)>0)
		{
			if(strcmp(pname,temp_str_pname))
				continue;
		}

		if(strlen(readline)<=48)
		{
			switch(last_color)
			{
				case 'E':
					printf(RED);
					break;
				case 'W':
					printf(YELLOW);
					break;
				case 'N':
					printf(L_GREEN);
					break;
				case 'I':
					printf(BOLD);
					break;
				case 'D':
					printf(L_PURPLE);
					break;
				default:
					printf(RED);
					break;
			}
		
			printf("%s",readline);
			printf(NONE);	
			continue;
		}
		else
		{
			if(!strstr("USP",tmptype)||!strstr("EWNI",tmplevel))
			{
				switch(last_color)
				{
					case 'E':
						printf(RED);
						break;
					case 'W':
						printf(YELLOW);
						break;
					case 'N':
						printf(L_GREEN);
						break;
					case 'I':
						printf(BOLD);
						break;
					case 'D':
						printf(L_PURPLE);
						break;
					default:
						printf(RED);
						break;
				}
				printf("%s",readline);
				printf(NONE);	
				continue;				
			}
		}
		
		// "PSEWNI"
		if(strstr(mode,tmptype)&&strstr(level,tmplevel))
		{
			last_color = tmplevel[0];
			switch(tmplevel[0])
			{
				case 'E':
					printf(RED);
					break;
				case 'W':
					printf(YELLOW);
					break;
				case 'N':
					printf(L_GREEN);
					break;
				case 'I':
					printf(BOLD);
					break;
				case 'D':
					printf(L_PURPLE);
					break;
				default:
					printf(RED);
					break;
			}
		
			printf("%s",readline);
			printf(NONE);
		}
	} 

	printf("\r\n");
	
}


void eipcc_ctr_daemon(IPC_CLI_TYPE type,int size,void *data)
{
	ipc_cli_packet *icp = malloc(sizeof(ipc_cli_packet)+size);
	memset(icp,0,sizeof(ipc_cli_packet)+size);
	icp->ict=type;
	icp->size=size;
	if(size>0&&data)
	{
		memcpy(&icp->data,data,size);
	}
	sendto(eipcc_sock,icp,sizeof(ipc_cli_packet)+size,0,(struct sockaddr *)&eipcc_toAddr,sizeof(struct sockaddr_un));
	free(icp);
}


void eipcc_print_console_display()
{
	struct sockaddr_un addrto;
	memset(&addrto,0,sizeof(addrto));
	addrto.sun_family = AF_UNIX;
	strcpy(addrto.sun_path, IPC_CONSOLE_BROADCAST_SOCKET);

	int sock = -1;
	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) 
	{	
		printf("socket error\r\n");
		return;
	}	
	unlink(IPC_CONSOLE_BROADCAST_SOCKET);
	if(bind(sock,(struct sockaddr *)&addrto, sizeof(struct sockaddr_un)) == -1) 
	{	
		printf("bind error...\r\n");
		return ;
	}
	while(1)
	{
		char smsg[IPC_DEBUG_PRINT_MAX_SIZE*4] = {0};
		int ret=recvfrom(sock, smsg, IPC_DEBUG_PRINT_MAX_SIZE*4, 0,NULL,NULL);
		if(ret<=0)
		{
			printf("read error....\r\n");
		}
		else
		{		
			printf("%s", smsg);	
		}
	}
}


int main(int argc , char * argv[])
{
	int oc; 					/*选项字符 */
	char *b_opt_arg;			/*选项参数字串 */

	eipcc_connect_daemon();

	if(!strcmp(argv[0],"eipchelp"))
	{
		eipcc_ctr_daemon(ENUM_IPC_CLI_HELP,0,NULL);
		return 0;
	}else
	if(!strcmp(argv[0],"eipcls"))
	{
		ipc_cli_ls_packet *iclp = malloc(sizeof(ipc_cli_ls_packet));
		memset(iclp,0,sizeof(ipc_cli_ls_packet));

    	while((oc = getopt(argc, argv, "n:")) != -1)
    	{
        	switch(oc)
        	{
            	case 'n':
                	snprintf(iclp->pname,PROCESS_NAME_MAX_SIZE,"%s",optarg);
                	break;
				case '?':
					printf("arguments error!\n");
					printf("%s [-n servername]\n",argv[0]);
					exit(0);
        	}
   	 	}
		eipcc_ctr_daemon(ENUM_IPC_CLI_LS,sizeof(ipc_cli_ls_packet),iclp);
		free(iclp);
	}else
	if(!strcmp(argv[0],"eipccat"))
	{
		ipc_cli_print_packet icpp;
		int cat_deamon=0;
		int d_offset = 0;
		memset(&icpp,0,sizeof(ipc_cli_print_packet));
		snprintf(icpp.printf_log_type_flag,16,"UPS");
		snprintf(icpp.printf_log_level_flag,16,"EWNID");
		while((oc = getopt(argc, argv, "l:t:p:q:hdD:")) != -1)
    	{
        	switch(oc)
        	{
        		case 'l':
					memset(icpp.printf_log_level_flag,0,16);
					snprintf(icpp.printf_log_level_flag,16,"%s",(char *)optarg);
					break;				
            	case 'p':
					snprintf(icpp.printf_log_pname,PROCESS_NAME_MAX_SIZE,"%s",(char *)optarg);
                	break;
				case 't':
					memset(icpp.printf_log_type_flag,0,16);
					snprintf(icpp.printf_log_type_flag,16,"%s",(char *)optarg);
					break;
				case 'q':
					snprintf(icpp.key_word,key_word_max_size,"%s",(char *)optarg);
					break;
				case 'h':
				case '?':
					printf("arguments error!\n");
					printf("-l , printf level      E: error , W: warning , N:normal , I:info  , D:debug\r\n");
					printf("-t , printf type       P: pocess , S: system \r\n");
					printf("-p , reivew sepcify program name\r\n");
					printf("-d , keep print eipcd log in local\r\n");
					printf("-D , keep print eipcd log in local with offset ,Negative Numbers start at the end\r\n");
					printf("-q , search key word\r\n");
					printf("%s [-l e|w|n|i] [-t p|s] [-p program name]\n",argv[0]);
					exit(0);
				case 'd':
					cat_deamon = 1;
					if(optarg != NULL)
					printf("loacl print , offset = %d\n",d_offset);
					break;
				case 'D':
					cat_deamon = 1;
					if(optarg != NULL)
					d_offset = atoi((char *)optarg);
					printf("loacl print , offset = %d\n",d_offset);
					break;
        	}
   	 	}
		if(cat_deamon)
		{
			//daemon(0,0);
			eipc_conf_p_4cat = malloc(sizeof(easyipc_config));
			memset(eipc_conf_p_4cat,0,sizeof(easyipc_config));
			snprintf(eipc_conf_p_4cat->ipc_log_save_path,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",IPC_DEBUG_SAVE_PATH);
			eipc_conf_p_4cat->ipc_msg_packet_max_size = IPC_MAX_PACKET;
			eipc_conf_p_4cat->ipc_syslog_enable=0;

			int value;
			int ret;
			ret = GetConfigIntValue(IPC_CONF_FILE_PATH,"Config","packet_max_size",&value);
			if(!ret)
			{
				eipc_conf_p_4cat->ipc_msg_packet_max_size = value;
				//printf("easyipc reset packet_max_size = %d\n",eipc_conf_p->ipc_msg_packet_max_size);
			}

			ret = GetConfigIntValue(IPC_CONF_FILE_PATH,"Config","ipc_syslog_enable",&value);
			if(!ret)
			{
				eipc_conf_p_4cat->ipc_syslog_enable = value;
				//printf("easyipc reset ipc_syslog_enable = %d\n",eipc_conf_p->ipc_syslog_enable);
			}

			char save_path[IPC_LOG_SAVE_PATH_MAX_LENS]={0};
			ret = GetConfigStringValue(IPC_CONF_FILE_PATH,"Config","ipc_log_save_path",save_path);
			if(!ret)
			{
				printf("easyipc reset ipc_log_save_path = %s\n",save_path);
				snprintf(eipc_conf_p_4cat->ipc_log_save_path,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",save_path);
			}
			ipc_cat_deamon(icpp.printf_log_level_flag,icpp.printf_log_type_flag,icpp.printf_log_pname,icpp.key_word,d_offset);
			return 0;
		}
		eipcc_ctr_daemon(ENUM_IPC_CLI_CAT,sizeof(ipc_cli_print_packet),&icpp);		
	}
	else
	if(!strcmp(argv[0],"eipcdebug"))
	{
		
	}else
	if(!strcmp(argv[0],"eipcapi"))
	{
		if(argc==1)
		{
			printf("usage: eipcapi ipcname msgname\r\n");
		}
		ipc_handle * ipcapi = ipc_creat("eipcc_api");
		ipc_call_api(ipcapi,argv[1],argv[2],NULL,0,NULL,0,1000);
		usleep(10*1000);
		ipc_exit(ipcapi);
	}else
	if(!strcmp(argv[0],"eipcmsg"))
	{
		
		// I32 , I16 , I8 , U32 , U16 , U8
		// S1~512
		// eipcmsg msgname param1_type param1 param2_type param2
		ipc_cli_msg_q icmq;
		
		if(argc==1)
		{
			// printf all msg if support
			eipcc_ctr_daemon(ENUM_IPC_CLI_MSG,0,NULL);
			return 0;
		}


		if(argc==2 && (!strcmp("-h",argv[1])||!strcmp("--help",argv[1])))
		{
			printf("eipcmsg -q keyword1 keyword2 \r\n");
			printf("eipcmsg message_name -S?/U8/U16/U32/I8/I16/I32 param1 -S?/U8....\r\n");
			printf("S? : ? = 1~1024 , string mode\r\n");
			printf("U8 / U16 / U32 / I8 / I16 / I32 , integer mode\r\n");
			return 0;
		}

		if(argc>2 && (!strcmp("-q",argv[1])))
		{
			int tmpi=0;
			memset(&icmq,0,sizeof(ipc_cli_msg_q));
			if(argc>2)
			{
				for(tmpi=0;tmpi<key_word_max_index;tmpi++)
				{
					if(tmpi<argc-2)
					{
						snprintf(icmq.keyword[tmpi],key_word_max_size,"%s",argv[tmpi+2]);
					}
				}
				eipcc_ctr_daemon(ENUM_IPC_CLI_MSG,sizeof(ipc_cli_msg_q),&icmq);
			}			
			return 0;
		}


		

		if(argc>2 && argc%2==0)
		{
			ipc_handle * ipcmsg = ipc_creat("eipcc_msg");
			int malloc_size=0;
			int i,param_num = (argc/2)-1;
			char *param_buffer=NULL;
			for(i=0;i<param_num;i++)
			{
				if(!strcmp(argv[(i+1)*2],"I32"))
				{
					malloc_size+=4;
				}else
				if(!strcmp(argv[(i+1)*2],"I16"))
				{
					malloc_size+=2;
				}else
				if(!strcmp(argv[(i+1)*2],"I8"))
				{
					malloc_size+=1;
				}else
				if(!strcmp(argv[(i+1)*2],"C"))
				{
					malloc_size+=1;
				}else
				if(!strcmp(argv[(i+1)*2],"U32"))
				{
					malloc_size+=4;
				}else
				if(!strcmp(argv[(i+1)*2],"U16"))
				{
					malloc_size+=2;
				}else
				if(!strcmp(argv[(i+1)*2],"U8"))
				{
					malloc_size+=1;
				}else
				if(*argv[(i+1)*2]=='S')
				{

					int tmpsize = atoi((argv[(i+1)*2]+1));

					if(tmpsize==0)
					{

						printf("param %d [%s] error!\r\n",(i+1)*2,argv[(i+1)*2]);
						ipc_exit(ipcmsg);
						return 0;
					}
					malloc_size += tmpsize;
				}else
				{
					printf("param %d [%s] error!\r\n",(i+1)*2,argv[(i+1)*2]);
					ipc_exit(ipcmsg);
					return 0;
				}
			}
			
			param_buffer = malloc(malloc_size);
			bzero(param_buffer,malloc_size);
			int till_size=0;
			for(i=0;i<param_num;i++)
			{
				if(!strcmp(argv[(i+1)*2],"I32"))
				{
					till_size+=4;
					*(int *)(param_buffer+till_size-4) = (signed int)atoi(argv[(i+1)*2+1]);
				}else
				if(!strcmp(argv[(i+1)*2],"I16"))
				{
					till_size+=2;
					*(short *)(param_buffer+till_size-2) = (signed short)atoi(argv[(i+1)*2+1]);
				}else
				if(!strcmp(argv[(i+1)*2],"I8"))
				{
					till_size+=1;
					*(char *)(param_buffer+till_size-1) = (signed char)atoi(argv[(i+1)*2+1]);
				}else
				if(!strcmp(argv[(i+1)*2],"C"))
				{
					till_size+=1;
					*(char *)(param_buffer+till_size-1) = *(char *)argv[(i+1)*2+1];
				}else
				if(!strcmp(argv[(i+1)*2],"U32"))
				{
					till_size+=4;
					*(unsigned int *)(param_buffer+till_size-4) = (unsigned int)atoi(argv[(i+1)*2+1]);
				}else
				if(!strcmp(argv[(i+1)*2],"U16"))
				{
					till_size+=2;
					*(unsigned short *)(param_buffer+till_size-2) = (unsigned short)atoi(argv[(i+1)*2+1]);
				}else
				if(!strcmp(argv[(i+1)*2],"U8"))
				{
					till_size+=1;
					*(unsigned char *)(param_buffer+till_size-1) = (unsigned char)atoi(argv[(i+1)*2+1]);
				}else
				if(*argv[(i+1)*2]=='S')
				{
					int tmpsize = atoi(argv[(i+1)*2]+1);
					till_size += tmpsize;
					snprintf(param_buffer+till_size-tmpsize,tmpsize,"%s",argv[(i+1)*2+1]);
					printf(" str content:[%s]\r\n",argv[(i+1)*2+1]);
				}else
				{
					printf("param %d [%s] error!\r\n",(i+1)*2,argv[(i+1)*2]);
					ipc_exit(ipcmsg);
					exit(0);
				}
			}
			
#if 0
			int k=0;
			for(k=0;k<malloc_size;k++)
			{
				if(k%32==0)
					printf("-------------------------------------\r\n");
				printf("%02x ",param_buffer[k]);
			}
			printf("\r\n");
#endif
			
			ipc_send_msg(ipcmsg,argv[1],param_buffer,malloc_size);
			usleep(200*1000);
			free(param_buffer);
			ipc_exit(ipcmsg);
			return 0;
		}
		else
		if(argc ==2)
		{
			ipc_handle * ipcmsg = ipc_creat("eipcc_msg");
			ipc_send_msg(ipcmsg,argv[1],NULL,0);
			usleep(200*1000);
			ipc_exit(ipcmsg);
			return 0;
		}
		usleep(10*1000);

	}else
	if(!strcmp(argv[0],"eipcprint"))
	{
		ipc_cli_print_packet icpp;
		memset(&icpp,0,sizeof(ipc_cli_print_packet));
		snprintf(icpp.printf_log_type_flag,16,"UPS");
		snprintf(icpp.printf_log_level_flag,16,"EWNID");
		icpp.printf_broadcast_flag = 0;
		while((oc = getopt(argc, argv, "l:t:p:dhur")) != -1)
    	{
        	switch(oc)
        	{
        		case 'u':
					icpp.printf_broadcast_flag = 1;
					eipcc_ctr_daemon(ENUM_IPC_CLI_PRINT,sizeof(ipc_cli_print_packet),&icpp);
					eipcc_print_console_display();
					break;
				case 'r':
					eipcc_ctr_daemon(ENUM_IPC_CLI_RPC,sizeof(ipc_cli_print_packet),&icpp);
					usleep(10*1000);
					return;	
        		case 'l':
					memset(icpp.printf_log_level_flag,0,16);
					snprintf(icpp.printf_log_level_flag,16,"%s",optarg);
					break;
            	case 'd':
					memset(icpp.printf_log_type_flag,0,16);
					memset(icpp.printf_log_level_flag,0,16);
                	break;					
            	case 'p':
					snprintf(icpp.printf_log_pname,PROCESS_NAME_MAX_SIZE,"%s",optarg);
                	break;
				case 't':
					memset(icpp.printf_log_type_flag,0,16);
					snprintf(icpp.printf_log_type_flag,16,"%s",optarg);
					break;
				case 'h':
				case '?':
					printf("arguments error!\n");
					printf("-l , printf level      E: error , W: warning , N:normal , I:info\r\n");
					printf("-t , printf type       U: user  , P: pocess , S: system \r\n");
					printf("-n , disable all printf\r\n");
					printf("-d , display eipcd console log in background\r\n");
					printf("-p , reivew sepcify program name\r\n");
					printf("-r , open remote listen mode\r\n");
					printf("%s [-l e|w|n|i] [-t p|s] [-p program name]\n",argv[0]);
					exit(0);
        	}
   	 	}		

		eipcc_ctr_daemon(ENUM_IPC_CLI_PRINT,sizeof(ipc_cli_print_packet),&icpp);
		
	}else
	{

	}
	
	return 0;
}


