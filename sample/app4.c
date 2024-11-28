#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "easyipc_base.h"

void app4_msg_proc(char *mname,void *data,int size)
{
	printf("app4 app4_msg_proc recv msg:[%s],size:%d\r\n",mname,size);
}


int main(int argc , char *argv[])
{
	int ret=0;
	char str[20]={0};

	ipc_handle * ipc_app4 = ipc_creat("test_server_app");

	ipc_register_msg(ipc_app4,"MSG_TEST",app4_msg_proc);

	while(1)
	{
		pause();
	}
	return ret;

}

