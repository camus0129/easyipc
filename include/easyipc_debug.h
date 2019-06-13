#ifndef __easyipc_debug_h__
#define __easyipc_debug_h__



typedef enum
{
	ENUM_IPC_LOG_TYPE_USER,			// 各个进程或者服务的行为
	ENUM_IPC_LOG_TYPE_PROGESS,		// 各个进程或者服务的行为
	ENUM_IPC_LOG_TYPE_SYSTEM,		// 可能涉及到系统的行为
}IPC_LOG_TYPE;

void ipcd_debug_register();
void ipcd_debug_unregister();
void ipcd_debug_join();
void ipcd_debug_exit();
void ipcd_debug_hangup();
void ipcd_debug_msg_send();
void ipcd_debug_runtime_begin();
void ipcd_debug_runtime_finish();


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


extern char printf_log_level_flag[];
extern char printf_log_type_flag[];

extern char log_path[];
extern char tftp_hostname[];
extern char log_path[];
#endif
