#include "common.h"
#include "client_common.h"
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

struct client_ctx;
typedef int (*io_function)(struct client_ctx *);

struct client_ctx {
    int tag;
    int sock;
    int id;
    int indicative_id;
    io_function on_readable;
    io_function on_writable;
    char *http_request;
    int http_request_size;
    int http_request_offset;
    int http_file_size;
    int http_file_offset;
    struct timespec before_connected;
    struct timespec after_connected;
    uint32_t receive_buffer_filled;
    uint32_t receive_buffer_filled_prev;
    unsigned char *receive_buffer;
    int receive_buffer_size;
    int bytes_read;
    int bytes_sent;
    int minor_version;
    int status;
    char *msg;
    int msg_len;
    int pret;
    struct phr_header headers[100];
    int num_headers;
};

/*
 * General variables
 */

static struct client_ctx *client_contexts = NULL;
static int kq = -1;
static int event_loop_closed = 0;
static struct kevent *evlist = NULL;
static char *http_request = NULL;
static int http_request_length = 0;

static void on_client_close(struct client_ctx *client_ctx);
static int on_readable_connection(struct client_ctx *client_ctx);
static int on_readable_http(struct client_ctx *client_ctx);
static int on_readable_http_get(struct client_ctx *client_ctx);
static int on_writable_http(struct client_ctx *client_ctx);
static int on_connected(struct client_ctx *client_ctx);
static int pollable_fx(struct kevent *ev);
static void stop_event_loop(void);
static void start_event_loop(void);

static void
on_client_close(struct client_ctx *client_ctx)
{
    set_error_time(0);

    if (client_ctx) {
	if (client_ctx->sock > 0) {
	    close(client_ctx->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", client_ctx->sock);
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Disconnected\n", client_ctx->id, client_ctx->sock);
	    client_ctx->sock = -1;
	}

	if (client_ctx->receive_buffer) {
	    free(client_ctx->receive_buffer);
	    client_ctx->receive_buffer = NULL;
	}

	client_ctx->on_readable = NULL;
	client_ctx->on_writable = NULL;
    }

    if (clients_completed == config_number_of_requests) {
	user_log(LOG_LEVEL_INFO, "All clients are done!\n");
	user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
	stop_event_loop();
    }
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
	return 0;
    }

    return 1;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
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
	user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
	on_client_close(client_ctx);
	return 0;
    }

    client_ctx->receive_buffer_filled_prev = client_ctx->receive_buffer_filled;
    client_ctx->receive_buffer_filled += actual;
    client_ctx->bytes_read += actual;
    total_bytes_read += actual;
    client_ctx->num_headers = sizeof(client_ctx->headers) / sizeof(client_ctx->headers[0]);

    client_ctx->pret = phr_parse_response((const char *) client_ctx->receive_buffer,
        client_ctx->receive_buffer_filled,
	&(client_ctx->minor_version),
	&(client_ctx->status),
	&(client_ctx->msg),
	&(client_ctx->msg_len),
        client_ctx->headers,
        &(client_ctx->num_headers),
        client_ctx->receive_buffer_filled_prev);

    if (client_ctx->pret > 0) {
	char misc_buffer[DEFAULT_MAX_HTTP_HEADER];
	int request_length = 0;

	for (int i = 0; i < client_ctx->num_headers; i++) {
	    // Build string from name/value pair
	    snprintf(misc_buffer, DEFAULT_MAX_HTTP_HEADER, "%.*s: %.*s",
		     client_ctx->headers[i].name_len,
		     client_ctx->headers[i].name,
		     client_ctx->headers[i].value_len,
		     client_ctx->headers[i].value);

	    if (strncasecmp(misc_buffer, "Content-Length:", 15) == 0) {
		snprintf(misc_buffer, DEFAULT_MAX_HTTP_HEADER, "%.*s", client_ctx->headers[i].value_len, client_ctx->headers[i].value);
		request_length = atoi(misc_buffer);
	    }
	}

	if (request_length <= 0) {
	    user_log(LOG_LEVEL_ERROR, "Client %d (%d): ERROR: Either a file of size 0 were requested, or an invalid file were requested\n", client_ctx->id, client_ctx->sock);
	    goto error;
	}

	user_log(LOG_LEVEL_INFO, "Client %d (%d): Started receiving HTTP response with %d bytes...\n", client_ctx->id, client_ctx->sock, request_length);

	if ((client_ctx->pret + request_length) == actual) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Received all data from HTTP response\n", client_ctx->id, client_ctx->sock);
	    clients_completed++;
	    sample("afterallread", clients_completed == config_expected_completing);
	    client_ctx->receive_buffer_filled = 0;
	} else {
	    client_ctx->http_file_size = request_length;
	    client_ctx->http_file_offset = actual - client_ctx->pret;
	    client_ctx->on_readable = on_readable_http_get;
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
	user_log(LOG_LEVEL_ERROR, "Client %d (%d): HTTP request is longer than the receive buffer size!\n", client_ctx->id, client_ctx->sock);
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
on_readable_http_get(struct client_ctx *client_ctx)
{
    int actual;

    actual = recv(client_ctx->sock, client_ctx->receive_buffer, client_ctx->receive_buffer_size, 0);
    user_log(LOG_LEVEL_DEBUG, "Client %d (%d): recv(%d, ..., %d, 0) -> %d\n", client_ctx->id, client_ctx->sock, client_ctx->sock, client_ctx->receive_buffer_size, actual);

    if (actual == -1) {
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
	    if (config_user_log_level >= LOG_LEVEL_WARNING) {
		perror("on_readable_http_get - recv");
	    }
	    return 1;
	} else {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("on_readable_http_get - recv");
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
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Received all data from HTTP response\n", client_ctx->id, client_ctx->sock);
	    sample("afterallread", clients_completed == config_expected_completing);
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
on_writable_http(struct client_ctx *client_ctx)
{
    struct msghdr msghdr;
    struct iovec iov;
    int amount;
    int send_size;
    int r;
    struct kevent newevs[2];
    int nchlist;
    int nev;

    if (!has_written) {
	sample("beforefirstwrite", 1);
	has_written = 1;
    }

    send_size = client_ctx->http_request_size - client_ctx->http_request_offset;

    if (send_size > config_send_chunk_size) {
	send_size = config_send_chunk_size;
    }

    memset(&msghdr, 0, sizeof (msghdr));
    memset(&iov, 0, sizeof (iov));

    iov.iov_base = client_ctx->http_request + client_ctx->http_request_offset;
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
	client_ctx->http_request_offset += amount;
	total_bytes_sent += amount;

	if (client_ctx->http_request_offset == client_ctx->http_request_size) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Sent HTTP request\n", client_ctx->id, client_ctx->sock);
	    clients_completed_writing++;
	    sample("afterallwritten", clients_completed_writing == config_expected_completing);
	    client_ctx->on_writable = NULL;
	    EV_SET(&newevs[0], client_ctx->sock, EVFILT_WRITE, EV_DISABLE, 0, 0, client_ctx);
	    nchlist = 1;
	    nev = kevent(kq, newevs, nchlist, NULL, 0, NULL);
	    user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 2, NULL, 0, NULL) -> %d\n", kq, nev);

	    if (nev == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("kevent");
		}
		goto error;
	    }

	    if (config_run_mode == RUN_MODE_HTTP_POST) {;
		clients_completed++;
		user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
		on_client_close(client_ctx);
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
on_connected(struct client_ctx *client_ctx)
{
    struct kevent newevs[2];
    int nchlist;
    int nev;

    /*
     * Sample the time when the flow has established a connection.
     */
    if (get_time(&(client_ctx->after_connected)) == -1) {
	user_log(LOG_LEVEL_ERROR, "Client %d (%d): Failed to sample time data\n", clients_connected + 1, client_ctx->sock);
	goto error;
    }

    clients_connected++;
    sample("afterallconnected", clients_connected == config_number_of_requests);
    client_ctx->id = clients_connected;
    user_log(LOG_LEVEL_INFO, "Client %d (%d): Connected\n", client_ctx->id, client_ctx->sock);

    if ((client_ctx->receive_buffer = calloc(1, config_receive_buffer_size)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	goto error;
    }

    if (config_run_mode == RUN_MODE_HTTP_GET) {
	user_log(LOG_LEVEL_INFO, "Client %d (%d): Sending HTTP GET request...\n", client_ctx->id, client_ctx->sock);
	client_ctx->on_readable = on_readable_http;
	client_ctx->on_writable = on_writable_http;
	EV_SET(&newevs[0], client_ctx->sock, EVFILT_READ, EV_ADD, 0, 0, client_ctx);
	nchlist = 1;
    } else if (config_run_mode == RUN_MODE_HTTP_POST) {
	user_log(LOG_LEVEL_INFO, "Client %d (%d): Sending HTTP POST request...\n", client_ctx->id, client_ctx->sock);
	client_ctx->on_readable = NULL;
	client_ctx->on_writable = on_writable_http;
	nchlist = 0;
    } else if (config_run_mode == RUN_MODE_CONNECTION) {
	client_ctx->on_readable = on_readable_connection;
	client_ctx->on_writable = NULL;
	EV_SET(&newevs[0], client_ctx->sock, EVFILT_WRITE, EV_DISABLE, 0, 0, client_ctx);
	EV_SET(&newevs[1], client_ctx->sock, EVFILT_READ, EV_ADD, 0, 0, client_ctx);
	nchlist = 2;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode!\n", __func__);
	goto error;
    }

    if (nchlist > 0) {
	nev = kevent(kq, newevs, nchlist, NULL, 0, NULL);
	user_log(LOG_LEVEL_DEBUG, "kevent(%d, ..., 2, NULL, 0, NULL) -> %d\n", kq, nev);

	if (nev == -1) {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("kevent");
	    }
	    goto error;
	}
    }

    if (strcmp(config_transport, "TCP") == 0) {
	tcp_connected++;
    } else if (strcmp(config_transport, "SCTP") == 0) {
	sctp_connected++;
    } else {
	user_log(LOG_LEVEL_ERROR, "Invalid protocol \"%s\"\n", config_transport);
	goto error;
    }

    client_ctx->http_request = http_request;
    client_ctx->http_request_size = http_request_length;
    client_ctx->receive_buffer_size = config_receive_buffer_size;

    if (client_ctx->on_writable) {
	client_ctx->on_writable(client_ctx);
    }

    return 0;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop\n");
    return -1;
}

static int
pollable_fx(struct kevent *ev)
{
    struct client_ctx *client_ctx;

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
	client_ctx->sock = -1;
	return 0;
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

	nev = kevent(kq, NULL, 0, evlist, config_number_of_requests, NULL);
	user_log(LOG_LEVEL_DEBUG, "kevent(%d, NULL, 0, ..., %d, NULL) -> %d\n", kq, config_number_of_requests, nev);

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
		if (evlist[i].filter == EVFILT_SIGNAL) {
		    user_log(LOG_LEVEL_DEBUG, "Received signal!\n");
		    return;
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
    char *remote_hostname = NULL;
    char *remote_port = NULL;
    struct sockaddr_in server_addr;
    struct kevent sig_evs[2];
    FILE *connection_times_file;
    char connection_times_filename[500];

    if (gettimeofday(&start, NULL) == -1) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to get starting time!\n", __func__);
	error = 1;
	goto cleanup;
    }
    
    if (handle_command_line_arguments(argc, argv, "Aa:b:sR:C:D:H:h:i:l:o:n:M:u:S:T:") == -1) {
	user_log(LOG_LEVEL_ERROR, "%s: Error when parsing command-line arguments!\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (config_prefix_set) {
	remote_hostname = argv[argc - 3];
	remote_port = strtoul(argv[argc - 2], NULL, 0);
    } else {
	remote_hostname = argv[argc - 2];
	remote_port = strtoul(argv[argc - 1], NULL, 0);
    }

    if ((config_run_mode == RUN_MODE_HTTP_GET) || (config_run_mode == RUN_MODE_HTTP_POST)) {
	/*
	 * Create the HTTP request
	 */
	if (prepare_http_request(remote_hostname, &http_request, &http_request_length) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to construct HTTP request\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    if ((client_contexts = calloc(config_number_of_requests, sizeof (*client_contexts))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(remote_port);
    r = inet_pton(AF_INET, remote_hostname, &server_addr.sin_addr);
    user_log(LOG_LEVEL_DEBUG, "inet_pton(AF_INET, %s, ...) -> %d\n", remote_hostname, r);

    if (r != 1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("inet_pton");
	}
	error = 1;
	goto cleanup;
    }

    /*
     * Print overview of the options that are set.
     */
    print_overview(0, remote_hostname, remote_port);

    if (config_sample_data) {
	char command_string[500];

	// Create results directory if not already created
	snprintf(command_string, sizeof (command_string), "mkdir -p %s", config_results_dir);
	system(command_string);
    }

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
    if ((evlist = calloc(config_number_of_requests, sizeof (*evlist))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }

    /*
     * Sample current time + more before for-loop
     */
    sample("beforeforloop", 1);
    
    for (int i = 0; i < config_number_of_requests; i++) {
	struct client_ctx *client_ctx = (client_contexts + i);
	struct kevent ev;
	client_ctx->id = -1;
	client_ctx->indicative_id = i + 1;

	user_log(LOG_LEVEL_INFO, "Creating new socket (%d)...\n", i + 1);
	client_ctx->sock = create_and_bind_socket(config_transport, config_local_ip, 0);

	if (client_ctx->sock == -1) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to create socket!\n", __func__);
	    error = 1;
	    goto cleanup;
	}

	if (make_socket_non_blocking(client_ctx->sock) == -1) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to make socket %d non-blocking!\n", __func__, client_ctx->sock);
	    error = 1;
	    goto cleanup;
	}

	client_ctx->tag = CLIENT_TAG;

	if (get_time(&client_ctx->before_connected) == -1) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to sample time data for flow\n", __func__);
	    error = 1;
	    goto cleanup;
	}

	r = connect(client_ctx->sock, (struct sockaddr *)&server_addr, sizeof (server_addr));
	user_log(LOG_LEVEL_DEBUG, "connect(%d, ..., %d) -> %d\n", client_ctx->sock, sizeof (server_addr), r);

	if (errno != EINPROGRESS) {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("connect");
	    }
	    error = 1;
	    goto cleanup;
	}

	client_ctx->on_writable = on_connected;
	EV_SET(&ev, client_ctx->sock, EVFILT_WRITE, EV_ADD, 0, 0, client_ctx);
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

    /*
     * Sample current time + more after for-loop
     */
    sample("afterforloop", 1);

    /*
     * Handle signals in event-loop like in libuv and NEAT applications.
     */
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
    
    sample("beforeloop", 1);
    start_event_loop();
    sample("afterloop", 1);

    /*
     * Write connection times for each flow to file
     */
    snprintf(connection_times_filename, sizeof (connection_times_filename), "%s/%s_%s_%s.log", config_results_dir, config_prefix, "stats", "time_establishment");

    if (config_sample_data) {
	if ((connection_times_file = fopen(connection_times_filename, "a")) == NULL) {
	    perror("fopen");
	    set_error_time(1);
	}
    }

    if (config_sample_data) {
	for (int i = 0; i < config_number_of_requests; i++) {
	    struct timespec diff_time;
	    struct client_ctx *client_ctx = (client_contexts + i);
	    subtract_time(&client_ctx->before_connected, &client_ctx->after_connected, &diff_time);
	    fprintf(connection_times_file, "%d %ld %ld %ld %ld %ld %ld\n", client_ctx->indicative_id, diff_time.tv_sec, diff_time.tv_nsec, client_ctx->before_connected.tv_sec, client_ctx->before_connected.tv_nsec, client_ctx->after_connected.tv_sec, client_ctx->after_connected.tv_nsec);
	}
    }

    if (config_sample_data) {
	if (connection_times_file) {
	    fclose(connection_times_file);
	}
    }
cleanup:
    set_error_time(0);

    if (kq != -1) {
	close(kq);
	user_log(LOG_LEVEL_DEBUG, "close(%d)\n", kq);
    }

    if (client_contexts) {
	for (int i = 0; i < config_number_of_requests; i++) {
	    struct client_ctx *client_ctx = (client_contexts + i);
	
	    if (client_ctx->sock > 0) {
		close(client_ctx->sock);
		user_log(LOG_LEVEL_DEBUG, "close(%d)\n", client_ctx->sock);
	    }
	}

	free(client_contexts);
    }

    if (evlist) {
	free(evlist);
    }

    free_resources();
    free_resources_http();
    sanity_check(config_user_log_level);
    return EXIT_SUCCESS;
}
