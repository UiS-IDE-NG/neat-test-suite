#ifndef HTTP_COMMON_INCLUDE_H
#define HTTP_COMMON_INCLUDE_H

#include <picohttpparser.h>

extern char *http_file;

int prepare_http_response(struct phr_header *headers, int num_headers, char *path, int path_len, char **header_buffer_arg, int *header_buffer_len_arg, int *payload_len_arg, int *keep_alive_enabled_arg);
int create_http_file(void);
int prepare_http_request(char *remote_hostname, char **request_arg, int *request_length_arg);
void free_resources_http(void);

#endif
