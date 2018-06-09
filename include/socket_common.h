#ifndef SOCKET_COMMON_INCLUDE_H
#define SOCKET_COMMON_INCLUDE_H

int create_and_bind_socket_server(char *protocol, char *node, int port);
int create_and_bind_socket(char *protocol, char *local_addr, int port);
int make_socket_non_blocking(int sock);

#endif
