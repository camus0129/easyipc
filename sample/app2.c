#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "easyipc_base.h"
ipc_handle * ipc_app2 ;

typedef struct
{
	int vol;
	int power;
}gva_state;

int cnt=0;
void app2_msg_proc(char *mname,void *data,int size)
{
	static int i=0;
	printf("app2 recv msg: mname=[%s]  data=[%s] size=%d ,cnt=%d\r\n",mname,(char *)data,size,i++);
	//printf("app2 recv msg:[%s],size:%d\r\n",mname,size);
	usleep(100*1000);
	
}

IPC_API_RET light_red_falsh(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	static int i=0;
	printf("app2 recv api: mname=[%s]  data=[%s] size=%d ,cnt=%d\r\n",mname,(char *)data,size,i++);
	*ret_data="light_red_falsh return";
	*ret_size=strlen("light_red_falsh return")+1;
	return ENUM_APT_ACK_OK;
}

IPC_API_RET light_blue_falsh(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	static int i=0;
	printf("app2 recv api: mname=[%s]  data=[%s] size=%d ,cnt=%d\r\n",mname,(char *)data,size,i++);
	*ret_data="light_blue_falsh return";
	*ret_size=strlen("light_blue_falsh return")+1;
	return ENUM_APT_ACK_OK;
}

IPC_API_RET light_yellow_falsh(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	static int i=0;
	char *p=malloc(100);
	memset(p,0,100);
	snprintf(p,100,"light_yellow_falsh return");
	printf("app2 recv api: mname=[%s]  data=[%s] size=%d ,cnt=%d\r\n",mname,(char *)data,size,i++);
	*ret_data=p;
	//printf("*ret_data=[%s]\r\n",(char *)*ret_data);
	*ret_size=100;
	//usleep(490*1000);
	ipc_log(ipc_app2, ENUM_IPC_LOG_LEVEL_INFO, "ipc 休息休息 log test !!");
	printf("zzzzzzzzzzzzzzzzzzzzzzzz\r\n");

	ipc_send_msg(ipc_app2,"app2_msg2","哈哈哈哈哈哈",strlen("哈哈哈哈哈哈"));

	if(cnt++>3)
	{
		ipc_unregister_api(ipc_app2,"light_yellow_falsh");
		ipc_unregister_msg(ipc_app2,"app2_msg2");
	}

	return ENUM_APT_ACK_FAIL;
}


IPC_API_RET get_gva_state(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	static int i=0;
	gva_state *p=(gva_state *)malloc(sizeof(gva_state));
	
	memset(p,0,sizeof(gva_state));
	p->vol=95;
	p->power=30;
	*ret_data=p;
	*ret_size=sizeof(gva_state);

	sleep(3);
	
	return ENUM_APT_ACK_OK;
}





int main(int argc , char *argv[])
{
	ipc_init();
	ipc_app2 = ipc_creat("led");
	ipc_register_msg(ipc_app2,"app2_msg1",app2_msg_proc);
	//printf("%s:%d\r\n",__FUNCTION__,__LINE__);
	ipc_register_msg(ipc_app2,"app2_msg2",app2_msg_proc);
	ipc_register_msg(ipc_app2,"app2_msg3",app2_msg_proc);

	API_EXPORT(get_gva_state);
	//API_EXPORT(light_yellow_falsh);
	//ipc_register_api(ipc_app2,"light_red_falsh",light_red_falsh);
	//ipc_register_api(ipc_app2,"light_yellow_falsh",light_yellow_falsh);
	//ipc_register_api(ipc_app2,"light_blue_falsh",light_blue_falsh);
	while(1)
		pause();
	
	return 0;
}


