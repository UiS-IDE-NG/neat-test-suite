#include "common.h"
#include "socket_common.h"
#include "util_new.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int
create_and_bind_socket_server(char *protocol, char *node, int port)
{
    struct sockaddr_in local_addr4;
    int sock;
    int s;
    int yes = 1;

    if (strcmp(protocol, "TCP") == 0) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	user_log(LOG_LEVEL_DEBUG, "socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) -> %d\n", sock);
    } else if (strcmp(protocol, "SCTP") == 0) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	user_log(LOG_LEVEL_DEBUG, "socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP) -> %d\n", sock);
    } else {
	goto error;
    }

    if (sock == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("socket");
	}
	goto error;
    }

    s = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes));
    user_log(LOG_LEVEL_DEBUG, "setsockopt(%d, SOL_SOCKET, SO_REUSEADDR, 1, %lu) -> %d\n", sock, sizeof (yes), s);
	
    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("setsockopt SO_REUSEADDR");
	}
        goto error;
    }

    s = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof (yes));
    user_log(LOG_LEVEL_DEBUG, "setsockopt(%d, SOL_SOCKET, SO_REUSEPORT, 1, %lu) -> %d\n", sock, sizeof (yes), s);
	
    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_WARNING) {
	    perror("setsockopt SO_REUSEPORT");
	}
	goto error;
    }

#if defined(SO_NOSIGPIPE)
    s = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof (yes));
    user_log(LOG_LEVEL_DEBUG, "setsockopt(%d, SOL_SOCKET, SO_NOSIGPIPE, 1, %lu) -> %d\n", sock, sizeof (yes), s);
	
    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_WARNING) {
	    perror("setsockopt SO_NOSIGPIPE");
	}
	goto error;
    }
#endif //defined(SO_NOSIGPIPE)

    local_addr4.sin_family = AF_INET;
    local_addr4.sin_port = htons(port);
    if (node) {
	inet_pton(AF_INET, node, &local_addr4.sin_addr);
    } else {
	local_addr4.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    s = bind(sock, (struct sockaddr *)&local_addr4, sizeof (local_addr4));
    user_log(LOG_LEVEL_DEBUG, "bind(%d, ..., %d) -> %d\n", sock, sizeof (local_addr4), s);
	
    if (s < 0) {
	if (config_user_log_level >= LOG_LEVEL_WARNING) {
	    perror("bind");
	}
	goto error;
    }
    
    return sock;
error:
    if (sock > 0) {
	close(sock);
	user_log(LOG_LEVEL_DEBUG, "close(%d)\n", sock);
    }
    set_error_time(1);
    return -1;
}

int
create_and_bind_socket(char *protocol, char *local_addr, int port)
{
    struct sockaddr_in local_addr4;
    int sock = -1;
    int s;
    int yes = 1;

    if (strcmp(protocol, "TCP") == 0) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	user_log(LOG_LEVEL_DEBUG, "socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) -> %d\n", sock);
    } else if (strcmp(protocol, "SCTP") == 0) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	user_log(LOG_LEVEL_DEBUG, "socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP) -> %d\n", sock);
    } else {
	user_log(LOG_LEVEL_ERROR, "Invalid protocol \"%s\"\n", protocol);
	goto error;
    }

    if (sock == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("socket");
	}
	goto error;
    }

    s = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes));
    user_log(LOG_LEVEL_DEBUG, "setsockopt(%d, SOL_SOCKET, SO_REUSEADDR, 1, %lu) -> %d\n", sock, sizeof (yes), s);
	
    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("setsockopt SO_REUSEADDR");
	}
        goto error;
    }

    s = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof (yes));
    user_log(LOG_LEVEL_DEBUG, "setsockopt(%d, SOL_SOCKET, SO_REUSEPORT, 1, %lu) -> %d\n", sock, sizeof (yes), s);
	
    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_WARNING) {
	    perror("setsockopt SO_REUSEPORT");
	}
	goto error;
    }

#if defined(SO_NOSIGPIPE)
    s = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof (yes));
    user_log(LOG_LEVEL_DEBUG, "setsockopt(%d, SOL_SOCKET, SO_NOSIGPIPE, 1, %lu) -> %d\n", sock, sizeof (yes), s);
	
    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_WARNING) {
	    perror("setsockopt SO_NOSIGPIPE");
	}
	goto error;
    }
#endif //defined(SO_NOSIGPIPE)

    if (local_addr) {
	local_addr4.sin_family = AF_INET;
	local_addr4.sin_port = htons(port);
	inet_pton(AF_INET, local_addr, &local_addr4.sin_addr);

	s = bind(sock, (struct sockaddr *)&local_addr4, sizeof (local_addr4));
	user_log(LOG_LEVEL_DEBUG, "bind(%d, ..., %d) -> %d\n", sock, sizeof (local_addr4), s);
	
	if (s < 0) {
	    if (config_user_log_level >= LOG_LEVEL_WARNING) {
		perror("bind");
	    }
	    goto error;
	}
    }

    return sock;
error:
    if (sock > 0) {
	close(sock);
	user_log(LOG_LEVEL_DEBUG, "close(%d)\n", sock);
    }
    return -1;
}

int
make_socket_non_blocking(int sock)
{
    int flags;
    int s;

    flags = fcntl(sock, F_GETFL, 0);
    user_log(LOG_LEVEL_DEBUG, "fcntl(%d, F_GETFL, 0) -> %d\n", sock, flags);

    if (flags == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("fcntl");
	}
	return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sock, F_SETFL, flags);
    user_log(LOG_LEVEL_DEBUG, "fcntl(%d, F_SETFL, %d) -> %d\n", sock, flags, s);

    if (s == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("fcntl");
	}
	return -1;
    }

    return 0;
}
