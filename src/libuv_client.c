#include "common.h"
#include "client_common.h"
#include "socket_common.h"
#include "http_common.h"
#include "util_new.h"

#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <uv.h>
#include <picohttpparser.h>

struct client_ctx;
typedef int (*io_function)(struct client_ctx *);

struct client_ctx {
    int tag;
    uv_poll_t *handle;
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

static uv_loop_t *loop = NULL;
static struct sockaddr_in server_addr;
static struct client_ctx *client_contexts = NULL;
static int statistics_timer_enabled = 0;
static uv_timer_t statistics_timer;
static int statistics_count = 0;
static char *http_request = NULL;
static int http_request_length = 0;
static int signal_received = 0;

static void print_usage(void);
static void on_uv_walk(uv_handle_t *handle, void *arg);
static void on_connected(uv_poll_t *handle, int status, int events);
static void uvpollable_cb(uv_poll_t *handle, int status, int events);
static int on_readable_connection(struct client_ctx *client_ctx);
static int on_readable_http(struct client_ctx *client_ctx);
static int on_readable_http_get(struct client_ctx *client_ctx);
static int on_writable_http(struct client_ctx *client_ctx);
static void on_client_close(uv_handle_t *handle);
static void print_statistics(uv_timer_t *handle);
static void on_signal(uv_signal_t *handle, int signum);

static void
print_usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "$ ./libuv_server [options]\n");
    fprintf(stderr, "  -A                   Expect a filename prefix for sampling results to be\n");
    fprintf(stderr, "                       given as last command-line argument.\n");
    fprintf(stderr, "                       DEFAULT: \"%s\"\n", DEFAULT_PREFIX);
    fprintf(stderr, "  -s                   Enable sampling of CPU and memory data.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_SAMPLE_DATA);
    fprintf(stderr, "  -R <path>            The name of the sample results directory.\n");
    fprintf(stderr, "                       DEFAULT: \"%s\"\n", DEFAULT_RESULTS_DIR);
    fprintf(stderr, "  -C <flows>           Expect the specified number of connecting requests.\n");
    fprintf(stderr, "                       DEFAULT: %d%s\n", DEFAULT_EXPECTED_CONNECTING, DEFAULT_EXPECTED_CONNECTING ? "" : " (some CPU and memory samples disabled)");
    fprintf(stderr, "  -D <flows>           Expect the specified number of completing requests.\n");
    fprintf(stderr, "                       DEFAULT: %d%s\n", DEFAULT_EXPECTED_COMPLETING, DEFAULT_EXPECTED_COMPLETING ? "" : " (some CPU and memory samples disabled)");
    fprintf(stderr, "  -I <IP>              The local IP address to listen to.\n");
    fprintf(stderr, "                       DEFAULT: Listen to all local addresses\n");
    fprintf(stderr, "  -l <buffer size>     Sets the receive buffer size.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_RECEIVE_BUFFER_SIZE);
    fprintf(stderr, "  -c                   Continuously send data to the client until the client\n");
    fprintf(stderr, "                       disconnects.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_SEND_CONTINUOUS);
    fprintf(stderr, "  -k                   Use Keep-Alive semantics for HTTP.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_KEEP_ALIVE);
    fprintf(stderr, "  -M <protocol>        Use the specified transport protocol.\n");
    fprintf(stderr, "                       DEFAULT: TCP (possible TCP, SCTP)\n");
    fprintf(stderr, "  -p <port>            Bind to the specified port.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_PORT);
    fprintf(stderr, "  -S <statistics_rate> Sample data and print statistics at specified rate.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_STATISTICS_LOG_RATE);
    fprintf(stderr, "  -u <user_log_level>  Use the specified user log level.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_USER_LOG_LEVEL);
    fprintf(stderr, "  -T <seconds>         Expected number of seconds to run without errors.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_EXPECTED_TIME);
}


static void
on_uv_walk(uv_handle_t *handle, void *arg)
{
    struct client_ctx *client_ctx = NULL;

    if (handle == NULL) {
	return;
    }

    if (handle->data) {
	client_ctx = (struct client_ctx *)handle->data;
    }

    if (!uv_is_closing(handle)) {
	if (client_ctx) {
	    if (client_ctx->tag == CLIENT_TAG) {
		uv_close(handle, on_client_close);
	    } else {
		user_log(LOG_LEVEL_ERROR, "%s: Unknown handle tag\n", __func__);
		set_error_time(1);
	    }
	} else {
	    uv_close(handle, NULL);
	}

	user_log(LOG_LEVEL_DEBUG, "%s: Handle closed\n", __func__);
    }
}

static void
on_connected(uv_poll_t *handle, int status, int events)
{
    int r;
    struct client_ctx *client_ctx;
    
    if (status < 0) {
	user_log(LOG_LEVEL_ERROR, "ERROR: %s: %s\n", __func__, uv_strerror(status));
	goto error;
    }

    client_ctx = (struct client_ctx*)handle->data;

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
	r = uv_poll_start(client_ctx->handle, UV_READABLE | UV_WRITABLE, uvpollable_cb);
    } else if (config_run_mode == RUN_MODE_HTTP_POST) {
	user_log(LOG_LEVEL_INFO, "Client %d (%d): Sending HTTP POST request...\n", client_ctx->id, client_ctx->sock);
	client_ctx->on_readable = on_readable_http;
	client_ctx->on_writable = on_writable_http;
	r = uv_poll_start(client_ctx->handle, UV_WRITABLE, uvpollable_cb);
    } else if (config_run_mode == RUN_MODE_CONNECTION) {
	client_ctx->on_readable = on_readable_connection;
	client_ctx->on_writable = NULL;
	r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb);
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode!\n", __func__);
	goto error;
    }

    if (r < 0) {
	user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
	goto error;
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
    return;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop\n");
    uv_stop(loop);
}

static void
uvpollable_cb(uv_poll_t *handle, int status, int events)
{
    struct client_ctx *client_ctx = (struct client_ctx *)handle->data;

    if (status < 0) {
	user_log(LOG_LEVEL_ERROR, "ERROR: %s: %s\n", __func__, uv_strerror(status));
	goto error;
    }

    user_log(LOG_LEVEL_DEBUG, "Socket %d is pollable\n", client_ctx->sock);

    if ((events) & UV_READABLE) {
	if (!uv_is_closing(handle)) {
	    if (client_ctx->on_readable) {
		if (client_ctx->on_readable(client_ctx) == -1) {
		    return;
		}
	    }
	}
    }

    if ((events) & UV_WRITABLE) {
	if (!uv_is_closing(handle)) {
	    if (client_ctx->on_writable) {
		if (client_ctx->on_writable(client_ctx) == -1) {
		    return;
		}
	    }
	}
    }
    return;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    uv_stop(loop);
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
	if (!uv_is_closing(client_ctx->handle)) {
	    uv_close(client_ctx->handle, on_client_close);
	}
	return -1;
    }

    return 1;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    uv_stop(loop);
    return -1;
}

static int
on_readable_http(struct client_ctx *client_ctx)
{
    int actual;
    int r;

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
	if (!uv_is_closing(client_ctx->handle)) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
	    uv_close(client_ctx->handle, on_client_close);
	}
	return -1;
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
	    client_ctx->on_writable = NULL;
	    client_ctx->receive_buffer_filled = 0;
	    if ((r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb)) < 0) {
		user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
		goto error;
	    }
	    return 1;
	} else {
	    client_ctx->http_file_size = request_length;
	    client_ctx->http_file_offset = actual - client_ctx->pret;
	    client_ctx->on_writable = NULL;
	    client_ctx->on_readable = on_readable_http_get;
	    if ((r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb)) < 0) {
		user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
		goto error;
	    }
	}

        return 1;
    } else if (client_ctx->pret == -1) {
	user_log(LOG_LEVEL_ERROR, "Client %d: Received invalid HTTP request\n", client_ctx->id);
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
    uv_stop(loop);
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
	if (!uv_is_closing(client_ctx->handle)) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
	    uv_close(client_ctx->handle, on_client_close);
	}
	return -1;
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
    uv_stop(loop);
    return -1;
}

static void
on_client_close(uv_handle_t *handle)
{
    struct client_ctx *client_ctx = (struct client_ctx *)handle->data;

    set_error_time(0);
    
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
    }

    if (clients_completed == config_number_of_requests) {
	user_log(LOG_LEVEL_INFO, "All clients are done!\n");

	if (!signal_received) {
	    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
	    uv_stop(loop);
	}
    }
    
    if (handle) {
	free(handle);
    }
}

static int
on_writable_http(struct client_ctx *client_ctx)
{
    struct msghdr msghdr;
    struct iovec iov;
    int amount;
    int send_size;
    int r;

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
	    if ((r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb)) < 0) {
		user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
		goto error;
	    }
	    if (config_run_mode == RUN_MODE_HTTP_POST) {;
		clients_completed++;
		user_log(LOG_LEVEL_INFO, "Client %d (%d): Closing...\n", client_ctx->id, client_ctx->sock);
		uv_close(client_ctx->handle, on_client_close);
	    }
	}
    }

    return 1;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    uv_stop(loop);
    return -1;
}

static void
print_statistics(uv_timer_t *handle)
{
    char val[10];
    
    snprintf(val, sizeof (val), "sample:%d", statistics_count);
    sample(val, 1);

    statistics_count++;
    uv_timer_start(handle, print_statistics,
		   1000 * config_statistics_log_rate, 0);
}

void
on_signal(uv_signal_t *handle, int signum)
{
    struct client_ctx *client_ctx;

    if (signum == SIGINT) {
	user_log(LOG_LEVEL_WARNING, "\n%s - Received SIGINT signal\n", __func__);
    } else if (signum == SIGTERM) {
	user_log(LOG_LEVEL_WARNING, "\n%s - Received SIGTERM signal\n", __func__);
    } else {
	user_log(LOG_LEVEL_WARNING, "\n%s - Received unknown signal\n", __func__);
    }

    if (statistics_timer_enabled) {
	user_log(LOG_LEVEL_DEBUG, "Stopping statistics timer...\n");
	uv_timer_stop(&statistics_timer);
    }

    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    signal_received = 1;
    uv_stop(loop);
}

int
main(int argc, char *argv[])
{
    int r;
    static char *remote_hostname = NULL;
    static char *remote_port = NULL;
    uv_signal_t signal_term;
    uv_signal_t signal_intr;
    FILE *connection_times_file;
    char connection_times_filename[500];

    if (gettimeofday(&start, NULL) == -1) {
	user_log(LOG_LEVEL_ERROR, "Failed to get starting time!\n");
	error = 1;
	goto cleanup;
    }

    if (handle_command_line_arguments(argc, argv, "Aa:b:sR:C:D:H:h:i:l:o:n:M:u:S:T:") == -1) {
	user_log(LOG_LEVEL_ERROR, "%s: Error when parsing command-line arguments!\n", __func__);
	error = 1;
	goto cleanup;
    }

    if ((loop = calloc(1, sizeof(uv_loop_t))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory\n", __func__);
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

    if (r <= 0) {
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
     * Here we initialize the event loop.
     */
    user_log(LOG_LEVEL_DEBUG, "Initializing event loop...\n");

    if (uv_loop_init(loop) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to initialize libuv loop!\n", __func__);
	free(loop);
	loop = NULL;
	error = 1;
	goto cleanup;
    }

    /*
     * Sample current time + more before for-loop
     */
    sample("beforeforloop", 1);
    
    for (int i = 0; i < config_number_of_requests; i++) {
	struct client_ctx *client_ctx = (client_contexts + i);
	client_ctx->id = -1;
	client_ctx->indicative_id = i + 1;

	user_log(LOG_LEVEL_INFO, "Creating new socket (%d)...\n", i + 1);
	client_ctx->sock = create_and_bind_socket(config_transport, config_local_ip, 0);

	if (client_ctx->sock == -1) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to create socket!\n", __func__);
	    error = 1;
	    goto cleanup;
	}

	if ((client_ctx->handle = calloc(1, sizeof (*client_ctx->handle))) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	    error = 1;
	    goto cleanup;
	}

	if ((r = uv_poll_init(loop, client_ctx->handle, client_ctx->sock)) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: uv_poll_init_socket: %s\n", uv_strerror(r));
	    error = 1;
	    goto cleanup;
	}

	user_log(LOG_LEVEL_DEBUG, "Socket descriptor %d made non-blocking by libuv\n", client_ctx->sock);
	client_ctx->tag = CLIENT_TAG;
	client_ctx->handle->data = client_ctx;

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
	
	if ((r = uv_poll_start(client_ctx->handle, UV_WRITABLE, on_connected)) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to start polling on socket %d\n", __func__, client_ctx->sock);
	    error = 1;
	    goto cleanup;
	}
    }

    /*
     * Sample current time + more after for-loop
     */
    sample("afterforloop", 1);

    memset(&signal_term, 0, sizeof (signal_term));
    if (uv_signal_init(loop, &signal_term) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to initialize signal handle\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (uv_signal_start(&signal_term, on_signal, SIGTERM) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to start signal handle for SIGTERM\n", __func__);
	error = 1;
	goto cleanup;
    }

    memset(&signal_intr, 0, sizeof (signal_intr));
    if (uv_signal_init(loop, &signal_intr) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to initialize signal handle\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (uv_signal_start(&signal_intr, on_signal, SIGINT) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to start signal handle for SIGINT\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (config_statistics_log_rate > 0) {
	statistics_timer_enabled = 1;
	uv_timer_init(loop, &statistics_timer);
	uv_timer_start(&statistics_timer, print_statistics,
		       1000 * config_statistics_log_rate, 0);
    }

    user_log(LOG_LEVEL_INFO, "Starting event loop...\n");
    sample("beforeloop", 1);
    uv_run(loop, UV_RUN_DEFAULT);
    sample("afterloop", 1);
    user_log(LOG_LEVEL_INFO, "Left the event loop\n");

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

    if (loop) {
	if (uv_loop_close(loop) == UV_EBUSY) {
	    user_log(LOG_LEVEL_DEBUG, "%s - Closing all open handles...\n", __func__);
	    uv_walk(loop, on_uv_walk, NULL);
	    uv_run(loop, UV_RUN_DEFAULT);

	    if (uv_loop_close(loop) == UV_EBUSY) {
		user_log(LOG_LEVEL_WARNING, "%s - Failed to close open handles!\n", __func__);
	    }
	}

	free(loop);
    }

    sample("end", 1);

    if (client_contexts) {
	free(client_contexts);
    }

    free_resources();
    free_resources_http();
    sanity_check(config_user_log_level);
    user_log(LOG_LEVEL_INFO, "Terminating...\n");
    return EXIT_SUCCESS;
}
