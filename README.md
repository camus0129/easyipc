# **easyipc**
A simple ipc lib, it can listen to the number of inter-process message execution and log view and inter-process program calls, and provides several good tools to simulate the message sent;There are three main files in the output, eipcd for daemon, libeipc.so for process integration library, and eipc for debugging tool set (eipcls, eipccat, eipcprint, eipcmsg are soft connections to eipc)
>###### ln -s eipc eipcls
>###### ln -s eipc eipccat
>###### ln -s eipc eipcmsg
>###### ln -s eipc eipcprint

---
一个简单的进程间通讯的工具,它可以监听进程间消息执行的次数以及log的查看以及进程间的程序调用,并提供了几个比较好的工具用来模拟消息发送,输出一共有三个主要文件, eipcd 用于守护进程 , libeipc.so 用于进程的集成库 , eipc 用于调试工具集(eipcls , eipccat , eipcprint , eipcmsg 都是eipc的软连接)
>######  ln -s eipc eipcls
>######  ln -s eipc eipccat
>######  ln -s eipc eipcmsg
>######  ln -s eipc eipcprint

### eipcd   守护进程,需要提前启动,开机启动(daemon process , run after system poweron)
easyipc守护进程,需要在其它用到easyipc功能的进程启动之前启动(推荐),并一般在后台运行 (使用 &)
#### 运行参数
##### -L < EWNID>log打印级别] 如果不添加任何参数,则默认打印级别是不打印
	ERROR   WARM    NORMAL    INFO   DEBUG
##### -h 打印帮助信息
##### -t size : log类型 , -1:不保存 , 0: 不限制保存  >1: 保存设置的n条log信息,不保存到tmp下

#### 运行方式
1. nohup eipcd & 
2. 添加入启动service中,然后start

#### eipcd配置文件说明 : 存放路径(可以不用,如果有会生效)
> ##### [Config]
> ##### ipc_log_save_path=/tmp/aabb.log
> ##### ipc_syslog_enable=0
> ##### packet_max_size=128000
> ##### ipc_log_save_path  log默认存放路径
> ##### ipc_syslog_enable  syslog模式是否打开,默认关闭
> ##### packet_max_size   最大允许msg的data size  , 根据实际packet以及内存设置


### eipcmsg  调制工具 , 模拟发送消息
##### 用于在控制台下调试发送(查找)指定消息,并允许发送指定支持类型的参数
##### eipcmsg <MSG_NAME> [参数类型 参数1]  [参数类型 参数2]  …
##### 参数类型支持:  U8 , U16 , U32 , I8 , I16 , I32 ,S?  ,其中U以及I表示无符号和有符号整形 , S表示字符串?是字符串长度
##### ps: 如果不支持任何参数,则是打印当前所有注册的进程支持的消息
>##### 例:
>#####	eipcmsg connect_route S32 test S32 11223344	使用指定SSID以及PSK连接路由器
>#####	eipcmsg set_dev_volume U8 90			使用指定数值设置音量
>#####	eipcmsg -q keyword1 keyword2 keyword3 keyword4	消息查找或执行,如果存在改消息则执行,否则进行模糊查找
>#####	eipcmsg						打印所有支持的消息

### eipcprint 调制工具 , 设置打印信息
##### 重新设置log打印级别
##### eipcprint  [-l <EWID>log打印级别] [-t <UPS>打印类型] [-d设置为不打印] [-p ename 打印指定名称的客户端消息] [-d] 
>##### 例子:
>##### eipcprint -l EW -p app_main	只打印 客户端app_main的错误和警告消息
>##### eipcprint -t S			打印所有客户端的系统消息(一般为死机,退出,注册等)
>##### eipcprint -d			关闭所有打印
>##### eipcprint 			打开所有打印
>##### eipcprint -u			adb模式下本地打印控制台反馈(一般用于adb模式下本地打印工具信息)

### eipccat 调制工具 , 设置log信息
##### eipccat用于review之前的打印 , 类似于log查询
>##### eipccat  
>##### -l <EWID>log打印级别 
>##### -t <UPS>打印类型 
>##### -d 从头开始本地打印log (一般用于adb模式下本地打印log信息)
>##### -D 从指定位置本地打印log ,  num =0时 , 不限制,  num>0 在log 开始后num字节后开始打印 , num <0 , 距结束num字节开始打印(一般用于adb模式下本地打印log信息)
>##### -p ename 打印指定名称的客户端消息 
>##### -q “想要查询的关键字信息”

### eipcls 调制工具 , 打印各个进程统计信息
##### eipcls 用于列出每个进程的消息调用次数以及总时间统计

### 库函数使用 libeipc.so
##### 详细使用参考app1.c   app2.c 
##### void ipc_init();
##### 进程的ipclib的初始化函数,一个进程中调用一次即可,需要在任何ipc的其它函数使用前被调用

##### ipc_handle * ipc_creat(const char * pname);
##### 创建一个服务句柄,如果失败则返回NULL, pname是服务句柄名称,进程之间不要冲突,这个pname也用于ipc_api_call的识别
>##### 用法:   
>#####     ipc_handle *myhandle = ipc_creat(“demo”);
##### void ipc_register_msg(ipc_handle *ipc ,const char *msg_name,ipc_msg_proc fun);
##### 注册一个关注的消息在easyipc系统,当系统中有服务广播此消息时会被注册消息的回调调用
>##### 用法:
>##### 	   ipc_register_msg( myhandle , ”IPC_DEMO_MSG”, ipc_demo_msg_proc );

##### void ipc_register_api(ipc_handle *ipc,const char *msg_name,ipc_api_proc fun);
##### 声明一个远程API给外界调用,当外界调用msg_name时,回调函数fun则会被触发
>##### 用法:
>#####    Ipc_register_api( myhandle , ”ipc_demo_api”, ipc_demo_api );

##### void ipc_send_msg(ipc_handle *ipc,const char *msg_name,void *data,int size);
##### 发送一个消息到系统,系统会自动将消息转发给注册了该消息的服务,可以携带参数 data并需要描述参数的长度
>##### 用法:
>#####   	Ipc_send_msg( myhandle , your_struct , sizeof( your_struct ) );

##### IPC_API_RET ipc_call_api(ipc_handle *ipc,const char *pname,const char *api_name,void *data,int size,void **ret_data,int *ret_size,int timeout);
##### 远程调用指定服务名称的指定函数名称,该函数需要在它所在的服务中被声明过,可以携带参数发送给被调用者,同时也可以接受调用者的返回数据,需要注意,如果有返回值,则该返回值是需要调用者主动释放的,否则会发生内存泄漏问题
>##### pname: 被调用的eipc name
>##### api_name: 被调用的函数名称
>##### data: 发送给被调用的函数的数据
>##### size: 发送给被调用函数的数据长度
>##### ret_data: 被调用函数返回的数据 
>##### ret_size: 被调用函数返回数据的长度
>##### timeout: 允许最大的等待超时时间 ,单位ms
>##### 用法:
>#####     Void * retdata=NULL;  int retsize = 0;
>#####     IPC_API_RET ret = ipc_call_api( myhandle , “led”, “led_power_on”, NULL , 0 , &retdata , &retsize , 200);

##### void ipc_daemon_syscmd(ipc_handle *ipc ,char *syscmd);
##### 防止守护命令,当进程退出时,会执行syscmd的shell , 常用于死机后重新拉起进程
>##### 用法:
>#####   	Ipc_daemon_syscmd( myhandle , “/data/bin/demo &”);

##### API_EXPORT(api_fun)
##### API声明宏方式,用于将本进程的函数声明给eipc,允许其他进程使用ipc_api_call进行调用
>##### 用法:

>###### typedef struct
>###### {
>###### 	int vol;
>###### 	int power;
>###### }state;
>###### IPC_API_RET get_state(char *mname,void *data,int size,void **ret_data,int *ret_size,int timeout)
>###### {
>###### 	// 返回值必须要 malloc出来, eipc会自动释放 , timeout不用处理,这个是eipc用的
>###### 	// 如果不需要返回值,则不需要malloc , *ret_data=NULL; *ret_size=0; 即可
>###### 	state *p=(state *)malloc(sizeof(state));
>###### 	memset(p,0,sizeof(state));
>###### 	p->vol=95;
>###### 	p->power=30;
>###### 	*ret_data=p;
>###### 	*ret_size=sizeof(gva_state);	
>###### 	return ENUM_APT_ACK_OK;
>###### }
>###### Int main()
>###### {
>###### 	…
>###### API_EXPORT( get_state );
>###### 	…
>###### }


##### void ipc_print(ipc_handle * ipc, char * fmt, ...);
##### 打印用户log , 级别是N(Normal) , 不支持换行

##### void ipc_log(ipc_handle *ipc,IPC_LOG_LEVEL level,char *fmt, ...);
##### 打印一个log到 ipc的log系统,至于是否可以打印出来, 则需要看ipcd的打印级别设置,具体可以参考 eipcprint 指令帮助,暂时不支持换行 , 可以自定义打印级别
>##### 用法:
>#####   	ipc_log (youripchandle, ENUM_IPC_LOG_LEVEL_NORMAL,”this is test %d”,123);

##### ipc_dbg_ext(x,...)
##### 指定handel打印debug信息, 会自包含行号,文件名,函数名信息,不支持换行
>#####用法:
>#####  	Ipc_dbg_ext(youripchandle,”this is test %d”,123);

##### ipc_dbg(x,...)
##### 打印debug信息, 会自包含行号,文件名,函数名信息,不支持换行
>#####用法:
>#####  	Ipc_dbg(”this is test %d”,123);
