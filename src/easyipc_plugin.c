#include "easyipc_base.h"
#include "easyipc_daemon.h"
#include "easyipc_console.h"
#include "dlist.h"
#include "easyipc_debug.h"
#include "cjson.h"
#include "easyipc_plugin.h"
#include "easyipc_misc.h"
#include <dirent.h>

typedef struct 
{
	char monitor_msg_name[IPC_MSG_MAX_SIZE];
	DList *broadcast_msglist;
	DList *broadcast_syscmdlist;
	DList *broadcast_shell;
	DList *broadcast_uiled;
}monitor_msg;

typedef struct
{
	char filename[IPC_LOG_SAVE_PATH_MAX_LENS];
	cJSON *root;
}plugin_unit;

typedef enum
{
	PLUGIN_BROADCAT_MSG,
	PLUGIN_BROADCAT_UILED,
	PLUGIN_BROADCAT_SYSCMD,
	PLUGIN_BROADCAT_SHELL,
}PLUGIN_BROADCAT_TYPE;

typedef struct 
{
	char msg[IPC_PLUGIN_MSG_MAX_SIZE];
	PLUGIN_BROADCAT_TYPE type;
}broadcast_unit;




static DList * monitor_msglist = NULL;
static DList * plugin_config_jsonlist = NULL;
//extern unsigned long get_file_size(const char *path);
void ipcd_plugin_monitor_add_dir(char * dirname);
void ipcd_plugin_parse_m2m(cJSON *obj);
void ipcd_plugin_parse_mask(cJSON *obj);
void ipcd_plugin_parse_led(cJSON *obj);
void ipcd_plugin_monitor_add_file(char *filename);
void ipcd_plugin_monitor_msg_insert(char *monitor_mname,char *broadcat_mname,PLUGIN_BROADCAT_TYPE type);
int ipcd_plugin_parse_root(cJSON *root);
void ipcd_relay_msg(_mlist_member *mlm,char *mname,int size,void *data);


unsigned long ipcd_plugin_get_file_size(const char *path)  
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



char *ipcd_plugin_getmsgtype_str(PLUGIN_BROADCAT_TYPE type)
{
			switch(type)
			{
				case PLUGIN_BROADCAT_MSG:
					return "msg";
				case PLUGIN_BROADCAT_UILED:
					return "uiled";
				case PLUGIN_BROADCAT_SYSCMD:
					return "syscmd";
				case PLUGIN_BROADCAT_SHELL:
					return "shell";
					
			}
	return NULL;
}

void ipcd_plugin_monitor_msg_insert(char *monitor_mname,char *broadcat_mname,PLUGIN_BROADCAT_TYPE type)
{	
	if(NULL == monitor_msglist)
		monitor_msglist = dlist_create();

	int monitor_msg_size = dlist_len(monitor_msglist);
	for(;monitor_msg_size>0;monitor_msg_size--)
	{
		DListNode *dmm = dlist_get(monitor_msglist, monitor_msg_size-1);
		if(NULL == dmm)
			continue;

		monitor_msg *mm = dmm->data;
		if(!mm) continue;
		if(!strcmp(monitor_mname,mm->monitor_msg_name))
		{
			int broadcast_msg_size =dlist_len(mm->broadcast_msglist);

			for(;broadcast_msg_size>0;broadcast_msg_size--)
			{
				DListNode *dbm = dlist_get(mm->broadcast_msglist,broadcast_msg_size-1);
				if(NULL == dbm)
					continue;
				broadcast_unit *bu = dbm->data;
				if(!strcmp(bu->msg,broadcat_mname)&&(type==bu->type))
				{
					return;
				}
			}
			broadcast_unit *new_bu = malloc(sizeof(broadcast_unit));
			memset(new_bu,0,sizeof(broadcast_unit));
			snprintf(new_bu->msg,IPC_PLUGIN_MSG_MAX_SIZE,"%s",broadcat_mname);
			new_bu->type=type;
			dlist_attach(mm->broadcast_msglist,new_bu);
			ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_SYSTEM,ENUM_IPC_LOG_LEVEL_INFO,
				0,"register plugin: [%s]  insert to  <%s>:[%s]",broadcat_mname,ipcd_plugin_getmsgtype_str(type),monitor_mname);
			return;
		}
	}

	monitor_msg *new_mm=malloc(sizeof(monitor_msg));
	memset(new_mm,0,sizeof(monitor_msg));
	broadcast_unit *new_bu = malloc(sizeof(broadcast_unit));
	memset(new_bu,0,sizeof(broadcast_unit));
	snprintf(new_bu->msg,IPC_PLUGIN_MSG_MAX_SIZE,"%s",broadcat_mname);
	new_bu->type=type;
	DList*new_broadcast_msglist = dlist_create();
	dlist_attach(new_broadcast_msglist,new_bu);
	new_mm->broadcast_msglist = new_broadcast_msglist;
	snprintf(new_mm->monitor_msg_name,IPC_MSG_MAX_SIZE,"%s",monitor_mname);
	dlist_attach(monitor_msglist,new_mm);
	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_SYSTEM,ENUM_IPC_LOG_LEVEL_INFO,
		0,"register plugin : [%s] insert to <%s>:[%s]",broadcat_mname,ipcd_plugin_getmsgtype_str(type),monitor_mname);

}


int ipcd_plugin_exec_msg(char *src_msg,char *dest_str)
{
	_mlist_member * tmlm;
	int i,j;
	int member_size = dlist_len(mlist);
	for(i=0;i<member_size;i++)
	{
		DListNode *dnm = dlist_get(mlist,i);
		if(NULL == dnm)
			continue;
		tmlm = dnm->data;
		if(NULL == tmlm)
			continue;
		for(j=0;j<dlist_len(tmlm->pmsg_list);j++)
		{
			_m_member *m_member = dlist_get(tmlm->pmsg_list,j)->data;
			if(!strcmp(dest_str,m_member->msg_name))
			{
				_ipc_msg sim ; 
				snprintf(sim.msg_name,IPC_MSG_MAX_SIZE,"%s",m_member->msg_name);
				sim.send_pid=0;
				sim.data=0;
				sim.size=0;
				ipcd_relay_msg(tmlm,m_member->msg_name,sizeof(_ipc_msg),&sim);
				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
					0,
					"plugin trigger msg : [%s] -> [%s]",src_msg,m_member->msg_name);
				
				return 0;;
			}
		}
	}
	return 0;
}

int ipcd_plugin_exec_uiled(char *src_msg,char *dest_str)
{
	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
					0,
					"plugin trigger uiled : [%s] -> [%s]",src_msg,dest_str);
	return 0;
}

int ipcd_plugin_exec_syscmd(char *src_msg,char *dest_str)
{
	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
					0,
					"plugin trigger syscmd : [%s] -> [%s]",src_msg,dest_str);
	if(system(dest_str)<0)
	{
		printf("system:%s exec fail\r\n",dest_str);
	}
	return 0;
}


int ipcd_plugin_exec_shell(char *src_msg,char *dest_str)
{
	char newcmd[IPC_MSG_MAX_SIZE+4]={0};
	ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
					0,
					"plugin trigger shell : [%s] -> [%s]",src_msg,dest_str);
	snprintf(newcmd,IPC_MSG_MAX_SIZE+4,"sh %s",dest_str);
	if(system(newcmd)<0)
	{
		printf("shell:%s exec fail\r\n",newcmd);
	}	
	return 0;
}


int ipcd_plugin_trigger(char *monitor_mname)
{
	int sendflag=-1;
	if(NULL == monitor_msglist)
		return -1;
	
	int monitor_msg_size = dlist_len(monitor_msglist);
	for(;monitor_msg_size>0;monitor_msg_size--)
	{
		DListNode *dmm = dlist_get(monitor_msglist, monitor_msg_size-1);
		if(NULL == dmm)
			continue;
	
		monitor_msg *mm = dmm->data;
		if(NULL == mm->broadcast_msglist)
			continue;

		if(!strcmp(monitor_mname,mm->monitor_msg_name))
		{
			int broadcast_msg_size =dlist_len(mm->broadcast_msglist);
			for(;broadcast_msg_size>0;broadcast_msg_size--)
			{
				DListNode *dbm = dlist_get(mm->broadcast_msglist,broadcast_msg_size-1);
				if(NULL == dbm)
					continue;

				broadcast_unit *new_bu = dbm->data;
				switch(new_bu->type)
				{
					case PLUGIN_BROADCAT_MSG:
						ipcd_plugin_exec_msg(monitor_mname,new_bu->msg);
						break;
					case PLUGIN_BROADCAT_UILED:
						ipcd_plugin_exec_uiled(monitor_mname,new_bu->msg);
						break;
					case PLUGIN_BROADCAT_SYSCMD:
						ipcd_plugin_exec_syscmd(monitor_mname,new_bu->msg);
						break;
					case PLUGIN_BROADCAT_SHELL:
						ipcd_plugin_exec_shell(monitor_mname,new_bu->msg);
						break;
				}
				sendflag=1;
			}
			return sendflag;
		}
	}
	return sendflag;
}

void ipcd_plugin_monitor_msg_view()
{
	int i,j,k=0;
	DListNode *dn = NULL;
	_mlist_member * tmlm;
	ipcd_console_print("====================== monitor msg ========================\r\n");
	if(NULL == monitor_msglist)
	{
		ipcd_console_print("NULL\r\n\r\n");
		return;
	}

	int monitor_msg_size = dlist_len(monitor_msglist);
	for(;monitor_msg_size>0;monitor_msg_size--)
	{
		DListNode *dmm = dlist_get(monitor_msglist, monitor_msg_size-1);
		if(NULL == dmm)
			continue;

		monitor_msg *mm = dmm->data;
		ipcd_console_print(BOLD);
		ipcd_console_print("%s\r\n",mm->monitor_msg_name);
		ipcd_console_print(NONE);

		if(NULL == mm->broadcast_msglist)
			continue;

		int broadcast_msg_size =dlist_len(mm->broadcast_msglist);

		for(;broadcast_msg_size>0;broadcast_msg_size--)
		{
			DListNode *dbm = dlist_get(mm->broadcast_msglist,broadcast_msg_size-1);
			if(NULL == dbm)
				continue;
			broadcast_unit *new_bu = dbm->data;
			ipcd_console_print("\t[%s] %s\r\n",ipcd_plugin_getmsgtype_str(new_bu->type),new_bu->msg);
		}
		ipcd_console_print("\r\n");
	}
	
	ipcd_console_print("\r\n====================== mask msg ========================\r\n");

	int member_size = dlist_len(mlist);
	for(k=0;k<member_size;k++)
	{
		dn = dlist_get(mlist,k);
		if(dn && dn->data)
		{
			tmlm = dn->data;
			//ipcd_console_print("pname: %s\r\n",tmlm->pname);
			if(tmlm->mask_sender)
			{
				i = dlist_len(tmlm->mask_sender);
				for(j=0;j<i;j++)
				{
					ipcd_console_print(RED);
					ipcd_console_print("[%s -> sender   mask] %s\r\n",tmlm->pname,(char *)dlist_get(tmlm->mask_sender,j)->data);
					ipcd_console_print(NONE);
				}
			}
			if(tmlm->mask_receiver)
			{
				i = dlist_len(tmlm->mask_receiver);
				for(j=0;j<i;j++)
				{
					ipcd_console_print(RED);
					ipcd_console_print("[%s -> receiver mask] %s\r\n",tmlm->pname,(char *)dlist_get(tmlm->mask_receiver,j)->data);
					ipcd_console_print(NONE);
				}
			}
		}
		ipcd_console_print("\r\n");
	}

	
}

void ipcd_plugin_monitor_msg_clean()
{
	if(NULL == monitor_msglist)
	{
		return;
	}

	int monitor_msg_size = dlist_len(monitor_msglist);
	for(;monitor_msg_size>0;monitor_msg_size--)
	{
		DListNode *dmm = dlist_get(monitor_msglist, monitor_msg_size-1);
		if(NULL == dmm)
			continue;

		monitor_msg *mm = dmm->data;
		if(NULL == mm->broadcast_msglist)
			continue;

		int broadcast_msg_size =dlist_len(mm->broadcast_msglist);

		for(;broadcast_msg_size>0;broadcast_msg_size--)
		{
			DListNode *dbm = dlist_get(mm->broadcast_msglist,broadcast_msg_size-1);
			if(NULL == dbm)
				continue;
			
			dlist_delete(mm->broadcast_msglist,broadcast_msg_size-1);
		}

		dlist_destroy(mm->broadcast_msglist);
		dlist_delete(monitor_msglist,monitor_msg_size-1);
	}
	
	dlist_destroy(monitor_msglist);
	monitor_msglist = NULL;
	
}



int ipcd_plugin_parse_root(cJSON *root)
{
	cJSON *root_list = cJSON_GetObjectItem(root, "list");
	if(NULL == root_list)
		return -1;

	int root_list_size = cJSON_GetArraySize(root_list);
	if(root_list<=0)
		return -1;

	int i=0;
	for(i=0;i<root_list_size;i++)
	{
		cJSON *root_list_item = cJSON_GetArrayItem(root_list,i);
		if(NULL == root_list_item)
			continue;

		cJSON *type_name_obj = cJSON_GetObjectItem(root_list_item, "type");
		if(NULL == type_name_obj)
			continue;

		char *type_name = type_name_obj->valuestring;

		if(!strcmp(type_name,"m2m"))
		{
			ipcd_plugin_parse_m2m(root_list_item);
		}else
		if(!strcmp(type_name,"mask_msg"))
		{
			ipcd_plugin_parse_mask(root_list_item);
		}
	}
	return 0;
}

void ipcd_plugin_monitor_add_file(char *filename)
{
	int filesize,fd=-1;
	char *path=NULL;
	if (NULL == (path=realpath(filename, NULL)))
	{
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
			0,"plugin config file [%s] path error",filename);
		ipcd_console_print("plugin config file [%s] path error\r\n",filename);
		return;
	}

	ipcd_console_print("plugin search file [%s]\r\n",path);

	if(NULL != plugin_config_jsonlist)
	{
		int k=0;
		int plugin_config_jsonlist_size = dlist_len(plugin_config_jsonlist);
		for(;k<plugin_config_jsonlist_size;k++)
		{
			plugin_unit *ppu  = dlist_get(plugin_config_jsonlist,k)->data;
			if(!strcmp(ppu->filename,path))
				return;
		}
	}

	if((fd = open (path,O_RDONLY)) == -1)
	{
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
			0,"plugin config file [%s] not find",path);
		ipcd_console_print("plugin config file [%s] not find\r\n",path);
		free(path);
		return;
	}

	if((filesize = ipcd_plugin_get_file_size(path))<=0)
	{
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
			0,"plugin config file [%s] size error",path);
		ipcd_console_print("plugin config file [%s] size error\r\n",path);
		free(path);
		return;
	}

	char *buffer = malloc(filesize+1);
	memset(buffer,0,filesize+1);
	if(read(fd,buffer,filesize)<=0)
		return;

	cJSON *root = cJSON_Parse(buffer);
	if(root==NULL)
	{
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
			0,"plugin config file [%s] parse fail",path);
		ipcd_console_print("plugin config file [%s] parse fail\r\n",path);
		free(path);
		free(buffer);
		return;
	}

	if(NULL == plugin_config_jsonlist)
		plugin_config_jsonlist = dlist_create();

	if(ipcd_plugin_parse_root(root)>=0)
	{
		plugin_unit *ppu = malloc(sizeof(plugin_unit));
		memset(ppu,0,sizeof(plugin_unit));
		ppu->root = root;
		snprintf(ppu->filename,IPC_LOG_SAVE_PATH_MAX_LENS,"%s",path);
		dlist_attach(plugin_config_jsonlist,ppu);
	}

	ipcd_console_print("plugin add file [%s] succeed \r\n",path);

	free(path);
	free(buffer);
}



void ipcd_plugin_refresh()
{
	int filesize,fd=-1;


	if(NULL != plugin_config_jsonlist)
	{
		int k=0;
		int plugin_config_jsonlist_size = dlist_len(plugin_config_jsonlist);
		for(k=0;k<plugin_config_jsonlist_size;k++)
		{
			plugin_unit *ppu  = dlist_get(plugin_config_jsonlist,k)->data;
			if(NULL == ppu)
				continue;
			if(NULL == ppu->root)
				continue;
			
			if((fd = open (ppu->filename,O_RDONLY)) == -1)
			{
				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
					0,"plugin config file [%s] not find",ppu->filename);
				return;
			}

			if((filesize = ipcd_plugin_get_file_size(ppu->filename))<=0)
			{
				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
					0,"plugin config file [%s] size error",ppu->filename);
				return;
			}

			char *buffer = malloc(filesize+1);
			memset(buffer,0,filesize+1);
			
			if(read(fd,buffer,filesize)<0)
				return;

			cJSON *root = cJSON_Parse(buffer);
			if(root==NULL)
			{
				ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
					0,"plugin config file [%s] parse fail",ppu->filename);
				return;
			}

			if(ipcd_plugin_parse_root(root)>=0)
			{
				cJSON_Delete(ppu->root);
				ppu->root = root;
			}
		}

	}
}



static int ec_filter(const struct dirent * filename)
{
	//printf("path = %s\r\n",filename->d_name);
	int lens = strlen(filename->d_name);
	if(lens>3 &&
		filename->d_name[lens-3] == '.' &&
		filename->d_name[lens-2] == 'e' &&
		filename->d_name[lens-1] == 'c' 
	) 
		return 1;
	return 0;
}


void ipcd_plugin_monitor_add_dir(char * dirname)
{
	char *path;
	if (NULL == (path=realpath(dirname, NULL)))
	{
		ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_WARN,
			0,"plugin config file [%s] path error",dirname);
		return;
	}

	//printf("path = %s\r\n",path);

	struct dirent **namelist;
	int j,itotalfile;
	itotalfile = scandir(path,&namelist,ec_filter,alphasort);
	for(j = 0; j<itotalfile; j++)
	{
		ipcd_plugin_monitor_add_file(namelist[j]->d_name);
	}
	free(path);
}

void ipcd_plugin_monitor_clean()
{
	int k,i=0;
	DListNode *dn=NULL;
	if(NULL == plugin_config_jsonlist)
		return;

	int size = dlist_len(plugin_config_jsonlist);
	for(i=0;i<size;i++)
	{
		DListNode *dn = dlist_get(plugin_config_jsonlist,i);
		if(!dn)
			continue;
		plugin_unit * ppu = dn->data;		
		if(ppu->root)
			cJSON_Delete(ppu->root);
		dlist_delete(plugin_config_jsonlist,i);
	}
	dlist_destroy(plugin_config_jsonlist);
	plugin_config_jsonlist = NULL;


	_mlist_member * tmlm = NULL;

	int member_size = dlist_len(mlist);
	for(k=0;k<member_size;k++)
	{
		dn = dlist_get(mlist,k);
		if(dn && dn->data)
		{
			tmlm = dn->data;
			if(tmlm->mask_sender)
			{
				dlist_destroy(tmlm->mask_sender);
				tmlm->mask_sender = dlist_create();
			}

			if(tmlm->mask_receiver)
			{
				dlist_destroy(tmlm->mask_receiver);
				tmlm->mask_receiver = dlist_create();
			}

		}
	}
}


void ipcd_plugin_parse_m2m(cJSON *obj)
{
	cJSON *monitor_obj = cJSON_GetObjectItem(obj,"monitor");
	if(NULL == monitor_obj)
		return;

	int monitor_obj_size = cJSON_GetArraySize(monitor_obj);
	if(monitor_obj_size<=0)
		return;
	

	int i=0;
	for(i=0;i<monitor_obj_size;i++)
	{
		char *monitor_obj_msg = cJSON_GetArrayItem(monitor_obj,i)->valuestring;
		cJSON *broadcast_obj = cJSON_GetObjectItem(obj,"broadcast");
		if(NULL == broadcast_obj)
			return;

		int broadcast_obj_size = cJSON_GetArraySize(broadcast_obj);
		if(broadcast_obj_size<=0)
			return;

		int j=0;
		PLUGIN_BROADCAT_TYPE item_type;
		for(j=0;j<broadcast_obj_size;j++)
		{
			cJSON *broadcast_obj_item = cJSON_GetArrayItem(broadcast_obj,j);

			if(!strcmp(broadcast_obj_item->child->string,"msg"))
			{
				item_type = PLUGIN_BROADCAT_MSG;
			}
			if(!strcmp(broadcast_obj_item->child->string,"uiled"))
			{
				item_type = PLUGIN_BROADCAT_UILED;
			}
			if(!strcmp(broadcast_obj_item->child->string,"syscmd"))
			{
				item_type = PLUGIN_BROADCAT_SYSCMD;
			}
			if(!strcmp(broadcast_obj_item->child->string,"shell"))
			{
				item_type = PLUGIN_BROADCAT_SHELL;
			}
			
			ipcd_plugin_monitor_msg_insert(monitor_obj_msg,broadcast_obj_item->child->valuestring,item_type);
		}
	}
}



void ipcd_plugin_parse_mask(cJSON *obj)
{
	cJSON *pnamep = NULL, *mnamep = NULL;
	cJSON *sendp = cJSON_GetObjectItem(obj,"send");
	cJSON *recvp = cJSON_GetObjectItem(obj,"recv");
	cJSON *itemp = NULL;
	int k,i=0,itemsize=0;
	DListNode *dn = NULL;
	_mlist_member * tmlm = NULL;
	if(sendp)
	{
		itemsize = cJSON_GetArraySize(sendp);
		for(i=0;i<itemsize;i++)
		{
			itemp = cJSON_GetArrayItem(sendp,i);
			if(itemp)
			{
				pnamep = cJSON_GetObjectItem(itemp,"pname");
				mnamep = cJSON_GetObjectItem(itemp,"mname");
				if(pnamep && mnamep)
				{
					int member_size = dlist_len(mlist);
					for(k=0;k<member_size;k++)
					{
						dn = dlist_get(mlist,k);
						if(dn && dn->data)
						{
							tmlm = dn->data;
							if(!strcmp(pnamep->valuestring,tmlm->pname) || !strcmp(pnamep->valuestring,"*"))
							{
								if(tmlm->mask_sender)
								{
									char *mname = strdup(mnamep->valuestring);
									dlist_attach(tmlm->mask_sender,mname);
									ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
										0,
										"insert sender mask [%s] -> [%s]",mname,tmlm->pname);
									
								}
							}
						}
					}
				}
			}
		}
	}

	if(recvp)
	{
		itemsize = cJSON_GetArraySize(recvp);
		for(i=0;i<itemsize;i++)
		{
			itemp = cJSON_GetArrayItem(recvp,i);
			if(itemp)
			{
				pnamep = cJSON_GetObjectItem(itemp,"pname");
				mnamep = cJSON_GetObjectItem(itemp,"mname");
				if(pnamep && mnamep)
				{
					int member_size = dlist_len(mlist);
					for(k=0;k<member_size;k++)
					{
						dn = dlist_get(mlist,k);
						if(dn && dn->data)
						{
							tmlm = dn->data;
							if(!strcmp(pnamep->valuestring,tmlm->pname) || !strcmp(pnamep->valuestring,"*"))
							{
								if(tmlm->mask_receiver)
								{
									char *mname = strdup(mnamep->valuestring);
									dlist_attach(tmlm->mask_receiver,mname);
									ipcd_log_save_and_print(ENUM_IPC_LOG_TYPE_PROGESS,ENUM_IPC_LOG_LEVEL_INFO,
										0,
										"insert receiver mask [%s] -> [%s]",mname,tmlm->pname);
							
								}
							}
						}
					}
				}
			}
		}
	}
}


void ipcd_plugin_view()
{
	int size, i =0;
	char * str=NULL;

	ipcd_console_print("=================== eipc plugin ====================\r\n");	
	if(NULL == plugin_config_jsonlist)
	{
		ipcd_console_print("NULL\r\n\r\n");
		return;
	}
	size = dlist_len(plugin_config_jsonlist);
	for(i=0;i<size;i++)
	{
		DListNode *node = dlist_get(plugin_config_jsonlist,i);
		plugin_unit *ppu = node->data;
		ipcd_console_print(BOLD);
		ipcd_console_print("[%s]:\r\n",ppu->filename);
		ipcd_console_print(NONE);
		str = cJSON_Print(ppu->root);
		ipcd_console_print("%s",str);
		ipcd_console_print("\r\n\r\n\r\n\r\n");
		free(str);
	}
}



