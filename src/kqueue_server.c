#include "common.h"
#include "server_common.h"
#include "socket_common.h"
#include "http_common.h"
#include "util_new.h"
#include <picohttpparser.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

struct server_ctx;
struct client_ctx;
typedef int (*server_io_function)(struct server_ctx *);
typedef int (*client_io_function)(struct client_ctx *);

struct server_ctx {
    int sock;
    server_io_function on_readable;
    server_io_function on_writable;
};

struct client_ctx {
    int tag;
    int id;
    int sock;
    char *http_response_header;
    int http_response_header_size;
    int http_response_header_offset;
    int http_response_header_sent;
    int http_file_size;
    int http_file_offset;
    int http_file_offset_prev;
    client_io_function on_readable;
    client_io_function on_writable;
    char *receive_buffer;
    size_t receive_buffer_size;
    int bytes_sent;
    int bytes_read;
    char *method;
    char *path;
    int minor_version;
    int pret;
    struct phr_header headers[100];
    size_t receive_buffer_filled;
    size_t receive_buffer_filled_prev;
    size_t method_len;
    size_t path_len;
    size_t num_headers;
    uint8_t keep_alive;
};

static int event_loop_closed = 0;
static int listen_socket_tcp = -1;
static int listen_socket_sctp = -1;
static struct server_ctx *server_ctx_tcp = NULL;
static struct server_ctx *server_ctx_sctp = NULL;
static int kq = -1;
static struct kevent *evlist = NULL;
static int evlist_length = 256;

static void on_client_close(struct client_ctx *client_ctx);
static int on_writable_http(struct client_ctx *client_ctx);
static int on_readable_connection(struct client_ctx *client_ctx);
static int on_readable_http(struct client_ctx *client_ctx);
static int on_readable_http_post(struct client_ctx *client_ctx);
static int on_connected(struct client_ctx *client_ctx);
static int pollable_fx(struct kevent *ev);
static void stop_event_loop(void);
static void start_event_loop(void);


static void
on_client_close(struct client_ctx *client_ctx)
{
    if (client_ctx) {
	if (client_ctx->sock > 0) {
	    close(client_ctx->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", client_ctx->sock);
	}

	if (client_ctx->receive_buffer) {
	    free(client_ctx->receive_buffer);
	    client_ctx->receive_buffer = NULL;
	}

	user_log(LOG_LEVEL_INFO, "Client %d (%d): Disconnected\n", client_ctx->id, client_ctx->sock);
	client_ctx->sock = -1;
	free(client_ctx);
    }
}

static int
on_writable_http(struct client_ctx *client_ctx)
{
    struct msghdr msghdr;
    struct iovec iov;
    int amount;
    char *buff_ptr;
    int buff_offset;
    int send_size;

    if (!has_written) {
	sample("beforefirstwrite", 1);
	has_written = 1;
    }

    if (client_ctx->http_response_header_sent) {
	buff_ptr = http_file;
	buff_offset = client_ctx->http_file_offset;
	send_size = client_ctx->http_file_size - buff_offset;
    } else {
	buff_ptr = client_ctx->http_response_header;
	buff_offset = client_ctx->http_response_header_offset;
	send_size = client_ctx->http_response_header_size - buff_offset;
    }

    if (send_size > config_send_chunk_size) {
	send_size = config_send_chunk_size;
    }

    memset(&msghdr, 0, sizeof (msghdr));
    memset(&iov, 0, sizeof (iov));

    iov.iov_base = (void *)(buff_ptr);
    iov.iov_len = send_size;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = NULL;
    msghdr.msg_controllen = 0;
    msghdr.msg_flags = 0;

#ifndef MSG_NOSIGNAL
    amount = sendmsg(client_ctx->sock, (const struct msghdr *)&msghdr, 0);
    user_log(LOG_LEVEL_DEBUG, "sendmsg(%d, ..., 0) -> %d\n", client_ctx->sock, amount);
#else
    amount = sendmsg(client_ctx->sock, (const struct msghdr *)&msghdr, MSG_NOSIGNAL);
    user_log(LOG_LEVEL_DEBUG, "sendmsg(%d, ..., MSG_NOSIGNAL) -> %d\n", client_ctx->sock, amount);
#endif

    if (amount < 0) {
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	    if (config_user_log_level >= LOG_LEVEL_WARNING) {
		perror("on_writable_http - sendmsg");
	    }
	    return 1;
	} else {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("on_writable_http - sendmsg");
	    }
	    goto error;
	}
    } else {
	total_bytes_sent += amount;
	if (client_ctx->http_response_header_sent) {
	    client_ctx->http_file_offset += amount;
	    if (client_ctx->http_file_offset == client_ctx->http_file_size) {
		user_log(LOG_LEVEL_INFO, "Client %d (%d): Sent HTTP response\n", client_ctx->id, client_ctx->sock);
		clients_completed++;
		sample("afterallwritten", clients_completed == config_expected_completing);
		user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
		on_client_close(client_ctx);
	    }
	} else {
	    client_ctx->http_response_header_offset += amount;
	    if (client_ctx->http_response_header_offset == client_ctx->http_response_header_size) {
		client_ctx->http_response_header_sent = 1;
	    }
	}	
    }

    return 1;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    stop_event_loop();
    return -1;
}

static int
on_readable_connection(struct client_ctx *client_ctx)
{
    int actual;

    actual = recv(client_ctx->sock, client_ctx->receive_buffer, client_ctx->receive_buffer_size, 0);
    user_log(LOG_LEVEL_DEBUG, "recv(%d, ..., %d, 0) -> %d\n", client_ctx->sock, client_ctx->receive_buffer_size, actual);

    if (actual == -1) {
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	    if (config_user_log_level >= LOG_LEVEL_WARNING) {
		perror("on_readable_connection - recv");
	    }
	    return 1;
	} else {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("on_readable_connection - recv");
	    }
	    goto error;
	}
    }

    if (actual == 0) {
	on_client_close(client_ctx);
	return 1;
    }

    return 1;
error:
    set_error_time(1);
    stop_event_loop();
    return -1;
}


static int
on_readable_http(struct client_ctx *client_ctx)
{
    int actual;
    int r;
    struct kevent newevs[2];
    int nchlist;
    int nev;

    if (!has_read) {
	sample("beforefirstread", 1);
	has_read = 1;
    }
    
    actual = recv(client_ctx->sock, client_ctx->receive_buffer + client_ctx->receive_buffer_filled, client_ctx->receive_buffer_size - client_ctx->receive_buffer_filled, 0);
    user_log(LOG_LEVEL_DEBUG, "recv(%d, ..., %d, 0) -> %d\n", client_ctx->sock, client_ctx->receive_buffer_size - client_ctx->receive_buffer_filled, actual);

    if (actual == -1) {
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	    if (config_user_log_level >= LOG_LEVEL_WARNING) {
		perror("on_readable_http - recv");
	    }
	    return 1;
	} else {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("on_readable_http - recv");
	    }
	    goto error;
	}
    }

    if (actual == 0) {
	user_log(LOG_LEVEL_DEBUG, "Client %d (%d): Read 0 bytes - end of file\n", client_ctx->id, client_ctx->sock);
	set_error_time(0);
	user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
	on_client_close(client_ctx);
	return 1;
    }

    client_ctx->receive_buffer_filled_prev = client_ctx->receive_buffer_filled;
    client_ctx->receive_buffer_filled += actual;
    client_ctx->bytes_read += actual;
    total_bytes_read += actual;
    client_ctx->num_headers = sizeof(client_ctx->headers) / sizeof(client_ctx->headers[0]);
    
    client_ctx->pret = phr_parse_request((const char *) client_ctx->receive_buffer,
					 client_ctx->receive_buffer_filled,
					 (const char **) &(client_ctx->method),
					 &(client_ctx->method_len),
					 (const char **) &(client_ctx->path),
					 &(client_ctx->path_len),
					 &(client_ctx->minor_version),
					 client_ctx->headers,
					 &(client_ctx->num_headers),
					 client_ctx->receive_buffer_filled_prev);

    /* If the HTTP request was successfully parsed */
    if (client_ctx->pret > 0) {
	char path_buffer[client_ctx->path_len+1];
	int request_length = 0;

	memcpy(path_buffer, client_ctx->path, client_ctx->path_len);
	path_buffer[client_ctx->path_len] = '\0';
	request_length = atoi(path_buffer);

	if (request_length <= 0) {
	    user_log(LOG_LEVEL_ERROR, "Client %d (%d): ERROR: Either a file of size 0 were requested, or an invalid file were requested\n", client_ctx->id, client_ctx->sock);
	    goto error;
	}

	/*
	 * Perform different actions based on the HTTP request method received.
	 */
	if (strncmp(client_ctx->method, "GET", client_ctx->method_len) == 0) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Received HTTP GET request for %d bytes\n", client_ctx->id, client_ctx->sock, request_length);
	    clients_completed_reading++;
	    sample("afterallread", clients_completed_reading == config_expected_completing);
	    if (prepare_http_response(client_ctx->headers, client_ctx->num_headers, client_ctx->path, client_ctx->path_len, &client_ctx->http_response_header, &client_ctx->http_response_header_size, &client_ctx->http_file_size, &client_ctx->keep_alive) < 0) {
		user_log(LOG_LEVEL_ERROR, "Client %d (%d): Failed to create HTTP response to GET request\n", client_ctx->id, client_ctx->sock);
		goto error;
	    }
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Started sending HTTP response with %d bytes...\n", client_ctx->id, client_ctx->sock, request_length);

	    client_ctx->on_writable = on_writable_http;
	    client_ctx->on_readable = NULL;
	    EV_SET(&newevs[0], client_ctx->sock, EVFILT_WRITE, EV_ADD, 0, 0, client_ctx);
	    nchlist = 1;	    
	} else if (strncmp(client_ctx->method, "POST", client_ctx->method_len) == 0) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Started receiving HTTP POST request with %d bytes...\n", client_ctx->id, client_ctx->sock,  request_length);
	    if ((client_ctx->pret + request_length) == actual) {
		user_log(LOG_LEVEL_INFO, "Client %d (%d): Received all data from POST request\n", client_ctx->id, client_ctx->sock);
		clients_completed++;
		sample("afterallread", clients_completed == config_expected_completing);
		client_ctx->on_writable = NULL;
		client_ctx->receive_buffer_filled = 0;
		nchlist = 0;
	    } else {
		client_ctx->http_file_size = request_length;
		client_ctx->http_file_offset = actual - client_ctx->pret;
		client_ctx->on_writable = NULL;
		client_ctx->on_readable = on_readable_http_post;
		nchlist = 0;
	    }
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d (%d): Invalid HTTP request method \"%.*s\" reeived\n", client_ctx->id, client_ctx->sock, client_ctx->method_len, client_ctx->method);
	    client_ctx->on_writable = NULL;
	    client_ctx->on_readable = NULL;
	    goto error;
	}

	if (nchlist > 0) {
	    nev = kevent(kq, newevs, nchlist, NULL, 0, NULL);
	    user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 1, NULL, 0, NULL) -> %d\n", kq, nev);

	    if (nev == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("kevent");
		}
		goto error;
	    }
	}

	return 1;
    } else if (client_ctx->pret == -1) {
	user_log(LOG_LEVEL_ERROR, "Client %d (%d): Received invalid HTTP request\n", client_ctx->id, client_ctx->sock);
	goto error;
    }

    if (client_ctx->pret != -2) {
	user_log(LOG_LEVEL_ERROR, "%s: Unexpected return code from HTTP parser!\n", __func__);
	goto error;
    }

    if (client_ctx->receive_buffer_filled == client_ctx->receive_buffer_size) {
	user_log(LOG_LEVEL_ERROR, "Client %d (%d): HTTP request header is longer than the receive buffer size!\n", client_ctx->id, client_ctx->sock);
	goto error;
    }

    return 1;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    stop_event_loop();
    return -1;
}

static int
on_readable_http_post(struct client_ctx *client_ctx)
{
    int actual;
    int r;

    actual = recv(client_ctx->sock, client_ctx->receive_buffer, client_ctx->receive_buffer_size, 0);
    user_log(LOG_LEVEL_DEBUG, "recv(%d, ..., %d, 0) -> %d\n", client_ctx->sock, client_ctx->receive_buffer_size, actual);

    if (actual == -1) {
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	    if (config_user_log_level >= LOG_LEVEL_WARNING) {
		perror("on_readable_http_post - recv");
	    }
	    return 1;
	} else {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("on_readable_post - recv");
	    }
	    goto error;
	}
    }

    if (actual == 0) {
	user_log(LOG_LEVEL_DEBUG, "Client %d (%d): Read 0 bytes - end of file\n", client_ctx->id, client_ctx->sock);
	set_error_time(0);
	on_client_close(client_ctx);
	return 1;
    }

    if (actual > 0) {
	client_ctx->bytes_read += actual;
	total_bytes_read += actual;
	client_ctx->http_file_offset += actual;
	if (client_ctx->http_file_offset == client_ctx->http_file_size) {
	    clients_completed++;
	    sample("afterallread", clients_completed == config_expected_completing);
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Received all data from POST request\n", client_ctx->id, client_ctx->sock);
	}
    } else {
	user_log(LOG_LEVEL_DEBUG, "Client %d (%d): Read 0 bytes - end of file\n", client_ctx->id, client_ctx->sock);
	set_error_time(0);
    }

    return 1;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    stop_event_loop();
    return -1;
}

static int
on_connected(struct client_ctx *client_ctx)
{
    struct kevent ev;
    int nev;
    int nchanges;

    clients_connected++;
    sample("afterallconnected", clients_connected == config_expected_connecting);

    if (config_run_mode == RUN_MODE_CONNECTION) {
	client_ctx->on_readable = on_readable_connection;
	client_ctx->on_writable = NULL;
	EV_SET(&ev, client_ctx->sock, EVFILT_READ, EV_ADD, 0, 0, client_ctx);
	nchanges = 1;
    } else if (config_run_mode == RUN_MODE_HTTP) {
	client_ctx->on_readable = on_readable_http;
	client_ctx->on_writable = NULL;
	EV_SET(&ev, client_ctx->sock, EVFILT_READ, EV_ADD, 0, 0, client_ctx);
	nchanges = 1;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode\n", __func__);
	goto error;
    }

    nev = kevent(kq, &ev, nchanges, NULL, 0, NULL);
    user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 1, NULL, 0, NULL) -> %d\n", kq, nev);

    if (nev == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("kevent");
	}
	set_error_time(1);
	return -1;
    }

    if ((client_ctx->receive_buffer = calloc(1, config_receive_buffer_size)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "Failed to allocate memory!\n");
	goto error;
    }

    client_ctx->id = clients_connected;
    client_ctx->receive_buffer_size = config_receive_buffer_size;
    user_log(LOG_LEVEL_INFO, "Client %d (%d): Connected\n", client_ctx->id, client_ctx->sock);   
    return 0;
error:
    set_error_time(1);
    return -1;
}

static int
pollable_fx(struct kevent *ev)
{
    struct sockaddr_storage addr;
    socklen_t socklen = sizeof (addr);
    struct client_ctx *client_ctx;
    struct server_ctx *server_ctx;

    if ((ev->ident == listen_socket_tcp) || (ev->ident == listen_socket_sctp)) {
	if (ev->ident == listen_socket_tcp) {
	    server_ctx = server_ctx_tcp;
	} else {
	    server_ctx = server_ctx_sctp;
	}

	if ((client_ctx = calloc(1, sizeof (*client_ctx))) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	    return -1;
	}

	if ((client_ctx->sock = accept(server_ctx->sock, (struct sockaddr *)&addr, &socklen)) == -1) {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("accept");
	    }
	    return -1;
	}

	if (on_connected(client_ctx) == -1) {
	    return -1;
	}

	if (ev->ident == listen_socket_tcp) {
	    tcp_connected++;
	} else {
	    sctp_connected++;
	}
    } else {
	client_ctx = ev->udata;

	if (ev->filter == EVFILT_WRITE) {
	    if (client_ctx->on_writable) {
		if (client_ctx->on_writable(client_ctx) == -1) {
		    return -1;
		}
	    }
	}

	if (ev->filter == EVFILT_READ) {
	    if (client_ctx->on_readable) {
		if (client_ctx->on_readable(client_ctx) == -1) {
		    return -1;
		}
	    }
	}

	if (ev->flags & EV_EOF) {
	    close(client_ctx->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", client_ctx->sock);
	    free(client_ctx);
	    return 0;
	}
    }

    return 0;
}

static void
stop_event_loop(void)
{
    event_loop_closed = 1;
}

static void
start_event_loop(void)
{
    int nev;

    for (;;) {
	if (event_loop_closed) {
	    break;
	}

	nev = kevent(kq, NULL, 0, evlist, evlist_length, NULL);
	user_log(LOG_LEVEL_DEBUG, "kevent(%d, NULL, 0, ..., %d, NULL) -> %d\n", kq, evlist_length, nev);

	if (nev == -1) {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("kevent");
	    }
	    set_error_time(1);
	    return;
	} else if (nev == 0) {
	    user_log(LOG_LEVEL_ERROR, "kevent returned 0 when no timeout were set. Aborting...\n");
	    set_error_time(1);
	    return;
	} else {
	    for (int i = 0; i < nev; i++) {
		if (evlist[i].flags & EV_ERROR) {
		    user_log(LOG_LEVEL_ERROR, "An error occured in kqueue. Aborting...\n");
		    set_error_time(1);
		    return;
		}

		if (evlist[i].filter == EVFILT_SIGNAL) {
		    user_log(LOG_LEVEL_DEBUG, "Received signal!\n");
		    return;
		} else if (evlist[i].filter == EVFILT_TIMER) {
		    user_log(LOG_LEVEL_DEBUG, "Statistics timer!\n");
		} else {
		    if (pollable_fx(&evlist[i]) == -1) {
			user_log(LOG_LEVEL_ERROR, "An error occured. Aborting event loop...\n");
			set_error_time(1);
			return;
		    }
		}
	    }
	}
    }
}

int
main(int argc, char *argv[])
{
    int r;
    int nev;
    struct kevent sig_evs[2];
    struct kevent ev;
    int listen_to_tcp = 0;
    int listen_to_sctp = 0;

    if (gettimeofday(&start, NULL) == -1) {
	user_log(LOG_LEVEL_ERROR, "Failed to get starting time!\n");
	error = 1;
	goto cleanup;
    }
    
    if (handle_command_line_arguments(argc, argv, "a:b:AH:sR:C:D:I:l:o:ck:M:p:S:u:T:") == -1) {
	user_log(LOG_LEVEL_ERROR, "%s: Error when parsing command-line arguments!\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (config_run_mode == RUN_MODE_HTTP) {
	/*
	 * Create the HTTP file that will be used as a data pool when responding
	 * with HTTP responses.
	 */
	if (create_http_file() < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to create the HTTP file\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    if (strcmp(config_transport, "TCP") == 0) {
	listen_to_tcp = 1;
    } else if (strcmp(config_transport, "SCTP") == 0) {
	listen_to_sctp = 1;
    } else if (strcmp(config_transport, "BOTH") == 0) {
	listen_to_tcp = 1;
	listen_to_sctp = 1;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid transport \"%s\"\n", __func__, config_transport);
	error = 1;
	goto cleanup;
    }

    /*
     * Print overview of the options that are set.
     */
    print_overview(0);

    /*
     * Sample the first data and create the results directory if it does
     * not exist already.
     */
    if (config_sample_data) {
	char command_string[500];

	// Create results directory if not already created
	snprintf(command_string, sizeof (command_string), "mkdir -p %s", config_results_dir);
	system(command_string);
    }

    sample("beforestart", 1);
    sample("start", 1);

    /*
     * Here we initialize the "event loop". We call it the event loop
     * since "uv_loop_init" in libuv initializes the kqueue when
     * running under FreeBSD, which is the heart of the event-loop.
     */
    user_log(LOG_LEVEL_DEBUG, "Initializing kqueue...\n");
    kq = kqueue();
    user_log(LOG_LEVEL_DEBUG, "kqueue() -> %d\n", kq);
    
    if (kq == -1) {
	perror("kqueue");
	error = 1;
	goto cleanup;
    }

    // This is part of the event loop initialization
    if ((evlist = calloc(evlist_length, sizeof (*evlist))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }

    if ((config_run_mode == RUN_MODE_HTTP) || (config_run_mode == RUN_MODE_CONNECTION)) {
	if (listen_to_tcp) {
	    if ((server_ctx_tcp = calloc(1, sizeof (*server_ctx_tcp))) == NULL) {
		user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    if ((listen_socket_tcp = create_and_bind_socket_server("TCP", config_local_ip, config_port)) == -1) {
		user_log(LOG_LEVEL_ERROR, "%s: Failed to create a listening TCP socket\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    server_ctx_tcp->sock = listen_socket_tcp;
	    r = listen(listen_socket_tcp, SOMAXCONN);
	    user_log(LOG_LEVEL_DEBUG, "listen(%d, %d) -> %d\n", listen_socket_tcp, SOMAXCONN, r);

	    if (r == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("listen");
		}
		error = 1;
		goto cleanup;
	    }

	    if (make_socket_non_blocking(server_ctx_tcp->sock) == -1) {
		user_log(LOG_LEVEL_ERROR, "%s: Failed to make socket %d non-blocking!\n", __func__, server_ctx_tcp->sock);
		error = 1;
		goto cleanup;
	    }

	    EV_SET(&ev, server_ctx_tcp->sock, EVFILT_READ, EV_ADD, 0, 0, server_ctx_tcp);
	    nev = kevent(kq, &ev, 1, NULL, 0, NULL);
	    user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 1, NULL, 0, NULL) -> %d\n", kq, nev);

	    if (nev == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("kevent");
		}
		error = 1;
		goto cleanup;
	    }

	    server_ctx_tcp->on_readable = on_connected;
	}

	if (listen_to_sctp) {
	    if ((server_ctx_sctp = calloc(1, sizeof (*server_ctx_sctp))) == NULL) {
		user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    if ((listen_socket_sctp = create_and_bind_socket_server("SCTP", config_local_ip, config_port)) == -1) {
		user_log(LOG_LEVEL_ERROR, "%s - error: Failed to create a listening SCTP socket\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    server_ctx_sctp->sock = listen_socket_sctp;
	    r = listen(listen_socket_sctp, SOMAXCONN);
	    user_log(LOG_LEVEL_DEBUG, "listen(%d, %d) -> %d\n", listen_socket_sctp, SOMAXCONN, r);

	    if (r == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("listen");
		}
		error = 1;
		goto cleanup;
	    }

	    if (make_socket_non_blocking(server_ctx_sctp->sock) == -1) {
		user_log(LOG_LEVEL_ERROR, "%s: Failed to make socket %d non-blocking!\n", __func__, server_ctx_sctp->sock);
		error = 1;
		goto cleanup;
	    }

	    EV_SET(&ev, server_ctx_sctp->sock, EVFILT_READ, EV_ADD, 0, 0, server_ctx_sctp);
	    nev = kevent(kq, &ev, 1, NULL, 0, NULL);
	    user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 1, NULL, 0, NULL) -> %d\n", kq, nev);

	    if (nev == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("kevent");
		}
		error = 1;
		goto cleanup;
	    }

	    server_ctx_sctp->on_readable = on_connected;
	}
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Unknown run mode specified!\n", __func__);
	error = 1;
	goto cleanup;
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    EV_SET(&sig_evs[0], SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
    EV_SET(&sig_evs[1], SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
    nev = kevent(kq, sig_evs, 2, NULL, 0, NULL);
    user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 2, NULL, 0, NULL) -> %d\n", kq, nev);

    if (nev == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("kevent");
	}
	error = 1;
	goto cleanup;
    }

    if (config_statistics_log_rate > 0) {
	EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, 1000 * config_statistics_log_rate, NULL);
	nev = kevent(kq, &ev, 1, NULL, 0, NULL);
	user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 1, NULL, 0, NULL) -> %d\n", kq, nev);

	if (nev == -1) {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("kevent");
	    }
	    error = 1;
	    goto cleanup;
	}
    }

    user_log(LOG_LEVEL_INFO, "Starting event loop...\n");
    sample("beforeloop", 1);
    start_event_loop();
    sample("afterloop", 1);
    user_log(LOG_LEVEL_INFO, "Left the event loop\n");
cleanup:
    set_error_time(0);

    if (evlist) {
	free(evlist);
	evlist = NULL;
    }

    if (kq != -1) {
	close(kq);
	user_log(LOG_LEVEL_DEBUG, "close(%d)\n", kq);
	kq = -1;
    }

    if (server_ctx_tcp) {
	if (server_ctx_tcp->sock > 0) {
	    close(server_ctx_tcp->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", server_ctx_tcp->sock);
	    server_ctx_tcp->sock = -1;
	}
	free(server_ctx_tcp);
	server_ctx_tcp = NULL;
    }

    if (server_ctx_sctp) {
	if (server_ctx_sctp->sock > 0) {
	    close(server_ctx_sctp->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", server_ctx_sctp->sock);
	    server_ctx_sctp->sock = -1;
	}
	free(server_ctx_sctp);
	server_ctx_sctp = NULL;
    }

    free_resources();
    free_resources_http();
    sanity_check(config_user_log_level);
    return EXIT_SUCCESS;
}
