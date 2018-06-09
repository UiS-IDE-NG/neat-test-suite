#include "common.h"
#include "server_common.h"
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

struct server_ctx {
    int tag;
    uv_poll_t *handle;
    int sock;
};

struct client_ctx;
typedef int (*io_function)(struct client_ctx *);

struct client_ctx {
    int tag;
    int id;
    uv_poll_t *handle;
    int sock;
    char *http_response_header;
    int http_response_header_size;
    int http_response_header_offset;
    int http_response_header_sent;
    int http_file_size;
    int http_file_offset;
    int http_file_offset_prev;
    io_function on_readable;
    io_function on_writable;
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

/*
 * General variables
 */

static uv_loop_t *loop = NULL;
static int listen_socket_tcp = -1;
static int listen_socket_sctp = -1;
static struct server_ctx *server_ctx_tcp = NULL;
static struct server_ctx *server_ctx_sctp = NULL;
static int statistics_timer_enabled = 0;
static uv_timer_t statistics_timer;
static int statistics_count = 0;

static void print_usage(void);
static struct server_ctx *create_server_ctx(int sock);
static void on_uv_walk(uv_handle_t *handle, void *arg);
static struct client_ctx *create_client_ctx(int sock);
static void on_connected(uv_poll_t *handle, int status, int events);
static void uvpollable_cb(uv_poll_t *handle, int status, int events);
static int on_readable_connection(struct client_ctx *client_ctx);
static int on_readable_http(struct client_ctx *client_ctx);
static int on_readable_http_post(struct client_ctx *client_ctx);
static int on_writable_http(struct client_ctx *client_ctx);
static void on_client_close(uv_handle_t *handle);
static void on_server_close(uv_handle_t *handle);
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
    fprintf(stderr, "  -H <file size>       Run in HTTP mode. Allocate the specified\n");
    fprintf(stderr, "                       amount of bytes as the HTTP response file.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_HTTP_FILE_SIZE);
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

static struct server_ctx *
create_server_ctx(int sock)
{
    int r;
    struct server_ctx *ctx = NULL;

    if ((ctx = calloc(1, sizeof (*ctx))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((ctx->handle = calloc(1, sizeof (*ctx->handle))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((r = uv_poll_init(loop, ctx->handle, sock)) < 0) {
	user_log(LOG_LEVEL_ERROR, "%s - uv_poll_init_socket: %s\n", uv_strerror(r));
	goto error;
    }

    user_log(LOG_LEVEL_DEBUG, "Socket descriptor %d made non-blocking by libuv\n", sock);
    ctx->tag = SERVER_TAG;
    ctx->handle->data = ctx;
    ctx->sock = sock;
    return ctx;
error:
    if (ctx) {
	if (ctx->handle) {
	    free(ctx->handle);
	}
	free(ctx);
    }

    set_error_time(1);
    return NULL;
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
	    } else if (client_ctx->tag == SERVER_TAG) {
		uv_close(handle, on_server_close);
	    } else {
		user_log(LOG_LEVEL_ERROR, "%s - Unknown handle tag\n", __func__);
		set_error_time(1);
	    }
	} else {
	    uv_close(handle, NULL);
	}

	user_log(LOG_LEVEL_DEBUG, "%s - Handle closed\n", __func__);
    }
}

static struct client_ctx*
create_client_ctx(int sock)
{
    int r;
    struct client_ctx *ctx;

    if ((ctx = calloc(1, sizeof (*ctx))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((ctx->handle = calloc(1, sizeof (*ctx->handle))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((ctx->receive_buffer = calloc(1, config_receive_buffer_size)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((r = uv_poll_init(loop, ctx->handle, sock)) < 0) {
	user_log(LOG_LEVEL_ERROR, "uv_poll_init_socket: %s\n", uv_strerror(r));
	goto error;
    }

    user_log(LOG_LEVEL_DEBUG, "Socket descriptor %d made non-blocking by libuv\n", sock);
    ctx->tag = CLIENT_TAG;
    ctx->handle->data = ctx;
    ctx->sock = sock;

    return ctx;
error:
    if (ctx) {
	if (ctx->handle) {
	    free(ctx->handle);
	}
	if (ctx->receive_buffer) {
	    free(ctx->receive_buffer);
	}
	free(ctx);
    }

    set_error_time(1);
    return NULL;
}

static void
on_connected(uv_poll_t *handle, int status, int events)
{
    int r;
    struct server_ctx *server_ctx;
    struct client_ctx *client_ctx;
    int sock = -1;
    struct sockaddr_in in_addr;
    socklen_t in_len;
    
    if (status < 0) {
	user_log(LOG_LEVEL_ERROR, "ERROR: %s: %s\n", __func__, uv_strerror(status));
	goto error;
    }

    clients_connected++;
    //sample("firstconnect", clients_connected == 1);
    sample("afterallconnected", clients_connected == config_expected_connecting);
    //sample("afterhalfconnected", clients_connected == (config_expected_connecting / 2));

    server_ctx = (struct server_ctx*)handle->data;
    in_len = sizeof (in_addr);
    sock = accept(server_ctx->sock, (struct sockaddr*)&in_addr, &in_len);
    user_log(LOG_LEVEL_DEBUG, "accept(%d, ..., %d) -> %d\n", server_ctx->sock, in_len, sock);

    if (sock == -1) {
	if (config_user_log_level >= LOG_LEVEL_ERROR) {
	    perror("accept");
	}
        goto error;
    }

    if ((client_ctx = create_client_ctx(sock)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to create client context!\n", __func__);
	goto error;
    }

    if (config_run_mode == RUN_MODE_CONNECTION) {
	client_ctx->on_readable = on_readable_connection;
	client_ctx->on_writable = NULL;
	r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb);
    } else if (config_run_mode == RUN_MODE_HTTP) {
	client_ctx->on_readable = on_readable_http;
	client_ctx->on_writable = on_writable_http;
	r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb);
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode\n", __func__);
	goto error;
    }

    if (r < 0) {
	user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", sock, uv_strerror(r));
	clients_connected--;
	goto error;
    }

    if (server_ctx->sock == listen_socket_tcp) {
	tcp_connected++;
    } else {
	sctp_connected++;
    }

    client_ctx->id = clients_connected;
    client_ctx->receive_buffer_size = config_receive_buffer_size;
    user_log(LOG_LEVEL_INFO, "Client %d (%d): Connected\n", client_ctx->id, client_ctx->sock);  
    return;
error:
    uv_stop(loop);
    set_error_time(1);
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
    uv_stop(loop);
    set_error_time(1);
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
    uv_stop(loop);
    set_error_time(1);
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
	set_error_time(0);
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
	    if ((r = uv_poll_start(client_ctx->handle, UV_WRITABLE | UV_READABLE, uvpollable_cb)) < 0) {
		user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
		goto error;
	    }
	} else if (strncmp(client_ctx->method, "POST", client_ctx->method_len) == 0) {
	    user_log(LOG_LEVEL_INFO, "Client %d (%d): Started receiving HTTP POST request with %d bytes...\n", client_ctx->id, client_ctx->sock,  request_length);
	    if ((client_ctx->pret + request_length) == actual) {
		user_log(LOG_LEVEL_INFO, "Client %d (%d): Received all data from POST request\n", client_ctx->id, client_ctx->sock);
		clients_completed++;
		sample("afterallread", clients_completed == config_expected_completing);
		client_ctx->on_writable = NULL;
		client_ctx->receive_buffer_filled = 0;
		if ((r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb)) < 0) {
		    user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
		    goto error;
		}
	    } else {
		client_ctx->http_file_size = request_length;
		client_ctx->http_file_offset = actual - client_ctx->pret;
		client_ctx->on_writable = NULL;
		client_ctx->on_readable = on_readable_http_post;
		if ((r = uv_poll_start(client_ctx->handle, UV_READABLE, uvpollable_cb)) < 0) {
		    user_log(LOG_LEVEL_ERROR, "uv_poll_start socket %d: %s\n", client_ctx->sock, uv_strerror(r));
		    goto error;
		}
	    }
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d (%d): Invalid HTTP request method \"%.*s\" reeived\n", client_ctx->id, client_ctx->sock, client_ctx->method_len, client_ctx->method);
	    client_ctx->on_writable = NULL;
	    client_ctx->on_readable = NULL;
	    uv_poll_start(client_ctx->handle, 0, uvpollable_cb);
	    goto error;
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
    uv_stop(loop);
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
    uv_stop(loop);
    return -1;
}

static void
on_client_close(uv_handle_t *handle)
{
    struct client_ctx *client_ctx = (struct client_ctx *)handle->data;

    if (client_ctx) {
	if (client_ctx->sock > 0) {
	    close(client_ctx->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", client_ctx->sock);
	}

	if (client_ctx->receive_buffer) {
	    free(client_ctx->receive_buffer);
	}

	user_log(LOG_LEVEL_INFO, "Client %d (%d): Disconnected\n", client_ctx->id, client_ctx->sock);
	free(client_ctx);
    }

    if (handle) {
	free(handle);
    }
}

static void
on_server_close(uv_handle_t *handle)
{
    struct server_ctx *server_ctx = (struct server_ctx *)handle->data;

    if (server_ctx) {
	if (server_ctx->sock > 0) {
	    close(server_ctx->sock);
	    user_log(LOG_LEVEL_DEBUG, "close(%d)\n", server_ctx->sock);
	}
	free(server_ctx);
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
		uv_close(client_ctx->handle, on_client_close);
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
    uv_stop(loop);
}

int
main(int argc, char *argv[])
{
    int result = EXIT_FAILURE;
    int s;
    uv_signal_t signal_term;
    uv_signal_t signal_intr;
    int listen_to_tcp = 0;
    int listen_to_sctp = 0;

    if (gettimeofday(&start, NULL) == -1) {
	user_log(LOG_LEVEL_ERROR, "Failed to get current time!\n");
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

    if ((loop = calloc(1, sizeof(uv_loop_t))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s - error: Failed to allocate memory\n", __func__);
	error = 1;
	goto cleanup;
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
    
    if (uv_loop_init(loop) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s - error: Failed to initialize libuv loop!\n", __func__);
	free(loop);
	loop = NULL;
	error = 1;
	goto cleanup;
    }

    if ((config_run_mode == RUN_MODE_HTTP) || (config_run_mode == RUN_MODE_CONNECTION)) {
	if (listen_to_tcp) {
	    if ((listen_socket_tcp = create_and_bind_socket_server("TCP", config_local_ip, config_port)) == -1) {
		user_log(LOG_LEVEL_ERROR, "%s: Failed to create a listening TCP socket\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    s = listen(listen_socket_tcp, SOMAXCONN);
	    user_log(LOG_LEVEL_DEBUG, "listen(%d, %d) -> %d\n", listen_socket_tcp, SOMAXCONN, s);

	    if (s == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("listen");
		}
		error = 1;
		goto cleanup;
	    }

	    if ((server_ctx_tcp = create_server_ctx(listen_socket_tcp)) == NULL) {
		user_log(LOG_LEVEL_ERROR, "%s - Failed to create server context!\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    if ((s = uv_poll_start(server_ctx_tcp->handle, UV_READABLE, on_connected)) < 0) {
		user_log(LOG_LEVEL_ERROR, "%s - Failed to start polling on socket %d\n", __func__, listen_socket_tcp);
		error = 1;
		goto cleanup;
	    }
	}

	if (listen_to_sctp) {
	    if ((listen_socket_sctp = create_and_bind_socket_server("SCTP", config_local_ip, config_port)) == -1) {
		user_log(LOG_LEVEL_ERROR, "%s - error: Failed to create a listening SCTP socket\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    s = listen(listen_socket_sctp, SOMAXCONN);
	    user_log(LOG_LEVEL_DEBUG, "listen(%d, %d) -> %d\n", listen_socket_sctp, SOMAXCONN, s);

	    if (s == -1) {
		if (config_user_log_level >= LOG_LEVEL_ERROR) {
		    perror("listen");
		}
		error = 1;
		goto cleanup;
	    }

	    if ((server_ctx_sctp = create_server_ctx(listen_socket_sctp)) == NULL) {
		user_log(LOG_LEVEL_ERROR, "%s - Failed to create server context!\n", __func__);
		error = 1;
		goto cleanup;
	    }

	    if ((s = uv_poll_start(server_ctx_sctp->handle, UV_READABLE, on_connected)) < 0) {
		user_log(LOG_LEVEL_ERROR, "%s - Failed to start polling on socket %d\n", __func__, listen_socket_sctp);
		error = 1;
		goto cleanup;
	    }
	}
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Unknown run mode specified!\n", __func__);
	error = 1;
	goto cleanup;
    }

    memset(&signal_term, 0, sizeof (signal_term));
    if (uv_signal_init(loop, &signal_term) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to initialize signal handle\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (uv_signal_start(&signal_term, on_signal, SIGTERM) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to start signal handle for SIGTERM\n", __func__);
	error = 1;
	goto cleanup;
    }

    memset(&signal_intr, 0, sizeof (signal_intr));
    if (uv_signal_init(loop, &signal_intr) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to initialize signal handle\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (uv_signal_start(&signal_intr, on_signal, SIGINT) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s - Failed to start signal handle for SIGINT\n", __func__);
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

    free_resources();
    free_resources_http();
    sanity_check(config_user_log_level);
    user_log(LOG_LEVEL_INFO, "Terminating...\n");
    
    return EXIT_SUCCESS;
}
