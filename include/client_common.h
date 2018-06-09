#ifndef CLIENT_COMMON_INCLUDE_H
#define CLIENT_COMMON_INCLUDE_H

#include "common.h"

extern int config_he_delay;
extern int config_number_of_requests;
extern int config_request_rate;

extern int config_he_delay_set;
extern int config_number_of_requests_set;
extern int config_request_rate_set;

int handle_command_line_arguments(int argc, char *argv[], char *options);
void free_resources(void);
void print_overview(int is_neat, char *hostname, int port);

#endif
