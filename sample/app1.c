#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "easyipc_base.h"

typedef struct
{
	int vol;
	int power;
}gva_state;

void app1_msg_proc(char *mname,void *data,int size)
{
	printf("app1 app1_msg_proc recv msg:[%s],size:%d\r\n",mname,size);
}


void app1_msg_proc1(char *mname,void *data,int size)
{
	printf("app1 app1_msg_proc1 recv msg:[%s],size:%d\r\n",mname,size);
}
void app1_msg_proc2(char *mname,void *data,int size)
{
	printf("app1 app1_msg_proc2 recv msg:[%s],size:%d\r\n",mname,size);
}
void app1_msg_proc3(char *mname,void *data,int size)
{
	printf("app1 app1_msg_proc3 recv msg:[%s],size:%d\r\n",mname,size);
}
void app1_msg_proc4(char *mname,void *data,int size)
{
	printf("app1 app1_msg_proc4 recv msg:[%s],size:%d\r\n",mname,size);
}




IPC_API_RET app1_api_proc(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
{
	printf("app1 recv msg:[%s],size:%d\r\n",mname,size);
	return ENUM_APT_ACK_OK;
}



int i=0;
int main(int argc , char *argv[])
{
	char str[20]={0};

	ipc_handle * ipc_app1 = ipc_creat("test_server_app1");

	ipc_register_msg(ipc_app1,"app1_msg",app1_msg_proc1);
	ipc_register_msg(ipc_app1,"app1_msg",app1_msg_proc2);

	
	ipc_register_msg(ipc_app1,"app1_msg1",app1_msg_proc);
	ipc_register_msg(ipc_app1,"app1_msg2",app1_msg_proc);
	ipc_register_msg(ipc_app1,"app1_msg3",app1_msg_proc);
	ipc_register_api(ipc_app1,"app1_api1",app1_api_proc);
	ipc_register_api(ipc_app1,"app1_api2",app1_api_proc);
	ipc_register_api(ipc_app1,"app1_api3",app1_api_proc);
	ipc_register_msg(ipc_app1,"app1_msg",app1_msg_proc3);
	ipc_register_msg(ipc_app1,"app1_msg",app1_msg_proc4);
	
	//ipc_send_msg(ipc_app1,"app2_msg2",NULL,0);
	pause();
	int count=0;
	while(1)
	{
		sleep(1);
		ipc_print(ipc_app1,"is print test , line=%d , cnt=%d\n",__LINE__,count++);
		sleep(1);
		ipc_dbg(ipc_app1,"is print test , line=%d , cnt=%d\n",__LINE__,count++);
	}

	while(1)
	{
		int size;
		void *recv_data=NULL;
		snprintf(str,20,"str msg:%06d",i);
		IPC_API_RET ret;

		ret=ipc_call_api(ipc_app1,"led","get_gva_state",NULL,0,&recv_data,&size,0);
		if(ENUM_APT_ACK_OK == ret)
		{
			if(recv_data&&(size==sizeof(gva_state)))
			{
				gva_state *gsp = recv_data;

				printf("ipc_call_api led get_gva_state success , vol=%d , power=%d\r\n",gsp->vol,gsp->power);
				
			}
		}
		ipc_send_msg(ipc_app1,"app2_msg3",str,strlen(str));
		ret=ipc_call_api(ipc_app1,"led","light_yellow_falsh","this is light red test!",strlen("this is light red test!")+1,&recv_data,&size,0);
		switch(ret)
		{
			case	ENUM_APT_ACK_OK:
				printf("ret = ENUM_APT_ACK_OK ");
				printf(", api return:%s , size=%d\r\n",(char *)recv_data,size);
				break;
			case	ENUM_APT_ACK_FAIL:
				printf("ret = ENUM_APT_ACK_FAIL ");
				printf(", api return:%s , size=%d\r\n",(char *)recv_data,size);
				break;
			case 	ENUM_API_IN_PARAM_ERROR:
				printf("ret = ENUM_API_IN_PARAM_ERROR \r\n");
				break;
			case 	ENUM_APT_ACK_CANTFIND_API:
				printf("ret = ENUM_APT_ACK_CANTFIND_API \r\n");
				break;
			case 	ENUM_APT_ACK_CANTFIND_PROGRAM:
				printf("ret = ENUM_APT_ACK_CANTFIND_PROGRAM \r\n");
				break;
			case 	ENUM_APT_ACK_TIMEOUT:
				printf("ret = ENUM_APT_ACK_TIMEOUT \r\n");
				break;
		}

		if(++i>=10)
		{
			ipc_log(ipc_app1, ENUM_IPC_LOG_LEVEL_INFO, "ipc log test !!");
			//printf("%s\r\n");
			//char *p;
			//memset(p,0,1000);
			pause();
			return 0;
		}

	}

	
	return 0;
}

