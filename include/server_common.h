#ifndef SERVER_COMMON_INCLUDE_H
#define SERVER_COMMON_INCLUDE_H

#include "common.h"

extern int config_port;
extern int config_send_continuous;

extern int config_port_set;
extern int config_send_continuous_set;

int handle_command_line_arguments(int argc, char *argv[], char *options);
void free_resources(void);
void print_overview(int is_neat);

#endif
