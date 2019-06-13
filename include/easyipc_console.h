#ifndef __easyipc_console_h__
#define __easyipc_console_h__


typedef enum
{
	ENUM_IPC_CLI_HELP,
	ENUM_IPC_CLI_CAT,
	ENUM_IPC_CLI_LS,
	ENUM_IPC_CLI_PRINT,
	ENUM_IPC_CLI_PLUGIN,
	ENUM_IPC_CLI_SCRIPT,
	ENUM_IPC_CLI_API,
	ENUM_IPC_CLI_MSG,
	ENUM_IPC_CLI_RPC,
}IPC_CLI_TYPE;


typedef enum
{
	ENUM_IPC_PLUGIN_FILE,
	ENUM_IPC_PLUGIN_DIR,
	ENUM_IPC_PLUGIN_VIEW,
	ENUM_IPC_PLUGIN_REFRESH,
	ENUM_IPC_PLUGIN_CLEAN,
}IPC_PLUGIN_TYPE;


typedef struct
{
	IPC_CLI_TYPE ict;
	int size;
	char data;
}ipc_cli_packet;


typedef struct
{
	char pname[PROCESS_NAME_MAX_SIZE];
}ipc_cli_ls_packet;


#define key_word_max_size 64
#define key_word_max_index 6
typedef struct
{
	char keyword[key_word_max_index][key_word_max_size];
}ipc_cli_msg_q;


typedef struct
{
	char printf_log_level_flag[16];
	char printf_log_type_flag[16];
	char printf_log_pname[PROCESS_NAME_MAX_SIZE];
	char key_word[key_word_max_size];
	char printf_broadcast_flag;
}ipc_cli_print_packet;

typedef struct
{
	char path[IPC_LOG_SAVE_PATH_MAX_LENS];
	IPC_PLUGIN_TYPE type;
}ipc_cli_plugin_packet;


void ipcd_console_local_broadcast(char *fmt, ...);

#define ipcd_console_print(...)  \
	printf(__VA_ARGS__); \
	ipcd_console_local_broadcast(__VA_ARGS__);

void ipcd_misc_cat(ipc_cli_print_packet *icpp);


#endif
