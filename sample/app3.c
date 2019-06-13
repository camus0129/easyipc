#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "easyipc_base.h"

void app3_msg_proc(char *mname,void *data,int size)
{
	printf("app3 recv msg:[%s],size:%d\r\n",mname,size);
}

IPC_API_RET app3_api_proc(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	printf("app3 recv msg:[%s],size:%d\r\n",mname,size);
	return ENUM_APT_ACK_OK;
}

int main(int argc , char *argv[])
{
	ipc_init();
	ipc_handle * ipc_app3 = ipc_creat("test_server_app3");
	ipc_register_msg(ipc_app3,"app3_msg1",app3_msg_proc);
	ipc_register_msg(ipc_app3,"app3_msg2",app3_msg_proc);
	ipc_register_msg(ipc_app3,"app3_msg3",app3_msg_proc);
	ipc_register_api(ipc_app3,"app3_api1",app3_api_proc);


	char *a=NULL;

	memset(a,0,100);

	while(1)
	{
		ipc_send_msg(ipc_app3,"app4_msg2",NULL,0);
		ipc_send_msg(ipc_app3,"app1_msg2",NULL,0);
	
		ipc_send_msg(ipc_app3,"app2_msg2",NULL,0);
		ipc_call_api(ipc_app3,"test_server_app2","app2_api2",NULL,0,NULL,NULL,100);

		sleep(3);
	}
	return 0;
}


