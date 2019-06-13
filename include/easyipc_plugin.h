#ifndef __easyipc_plugin_h__
#define __easyipc_plugin_h__

#include "easyipc_debug.h"

int ipcd_plugin_trigger(char *monitor_mname);
void ipcd_plugin_monitor_msg_view();
void ipcd_plugin_monitor_msg_clean();
void ipcd_plugin_monitor_add_file(char *filename);
void ipcd_plugin_monitor_add_dir(char * dirname);
void ipcd_plugin_monitor_clean();
void ipcd_plugin_view();
void ipcd_plugin_refresh();
void ipcd_log_save_and_print(IPC_LOG_TYPE tlt,IPC_LOG_LEVEL level,int pid,char *fmt, ...);

#endif
