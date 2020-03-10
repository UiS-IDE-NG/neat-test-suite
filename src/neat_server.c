#include "common.h"
#include "server_common.h"
#include "http_common.h"
#include "util_new.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <neat.h>
#include <uv.h>
#include <picohttpparser.h>

#define QUOTE(...) #__VA_ARGS__

struct client_ctx {
    int id;
    int is_tcp;
    int is_sctp;
    char *http_response_header;
    int http_response_header_size;
    int http_response_header_offset;
    int http_response_header_offset_prev;
    int http_response_header_sent;
    int http_file_size;
    int http_file_offset;
    int http_file_offset_prev;
    int http_file_sent;
    char *receive_buffer;
    int receive_buffer_size;
    int receive_buffer_filled;
    int receive_buffer_filled_prev;
    int bytes_sent;
    int bytes_read;
    char *method;
    char *path;
    int minor_version;
    int pret;
    struct phr_header headers[100];
    size_t method_len;
    int path_len;
    int num_headers;
    int keep_alive;
};

/*
 * General globals
 */

static struct neat_ctx *ctx = NULL;
static uv_loop_t *uv_loop = NULL;
static int statistics_timer_enabled = 0;
static uv_timer_t statistics_timer;
static int statistics_count = 0;
static int bytes_sent_in_interval = 0;
static uv_signal_t *signal_intr = NULL;
static int signal_intr_init = 0;
static uv_signal_t *signal_term = NULL;
static int signal_term_init = 0;
static int program_ended = 0;

static void print_usage(void);
static void on_signal(uv_signal_t *handle, int signum);
static neat_error_code on_aborted(struct neat_flow_operations *opCB);
static neat_error_code on_close(struct neat_flow_operations *opCB);
static neat_error_code on_error(struct neat_flow_operations *opCB);
static neat_error_code on_readable_http(struct neat_flow_operations *opCB);
static neat_error_code on_readable_http_post(struct neat_flow_operations *opCB);
static neat_error_code on_writable_http(struct neat_flow_operations *opCB);
static neat_error_code on_all_written_http(struct neat_flow_operations *opCB);
static neat_error_code on_connected(struct neat_flow_operations *opCB);
static void print_statistics(uv_timer_t *handle);

static void
print_usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "$ ./neat_server [options]\n");
    fprintf(stderr, "  This server supports two modes: HTTP and NORMAL.\n");
    fprintf(stderr, "  HTTP: Respond with HTTP response(s) to client HTTP requests.\n");
    fprintf(stderr, "        The client requests files with format \"<bytes>.txt\" in HTTP\n");
    fprintf(stderr, "        request.\n");
    fprintf(stderr, "  NORMAL: Print the data received from client.\n\n");
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
    fprintf(stderr, "  -P <filename>        Use the properties in the specified file.\n");
    fprintf(stderr, "                       DEFAULT: TCP\n");
    fprintf(stderr, "  -m                   Enable multihoming.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_MULTIHOMING_ENABLED);
    fprintf(stderr, "  -M <protocol>        Use the specified transport protocol.\n");
    fprintf(stderr, "                       DEFAULT: TCP (possible TCP, SCTP, HE, TCP-SCTP, SCTP-TCP)\n");
    fprintf(stderr, "  -p <port>            Bind to the specified port.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_PORT);
    fprintf(stderr, "  -S <statistics_rate> Sample data and print statistics at specified rate.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_STATISTICS_LOG_RATE);
    fprintf(stderr, "  -v <log_level>       Use the specified NEAT log level.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_NEAT_LOG_LEVEL);
    fprintf(stderr, "  -u <user_log_level>  Use the specified user log level.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_USER_LOG_LEVEL);
    fprintf(stderr, "  -T <seconds>         Expected number of seconds to run without errors.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_EXPECTED_TIME);
}

static void
on_signal(uv_signal_t *handle, int signum) {
    set_error_time(0);
    
    if (signum == SIGINT) {
	user_log(LOG_LEVEL_WARNING, "\nReceived signal: SIGINT\n");
    } else if (signum == SIGTERM) {
	user_log(LOG_LEVEL_WARNING, "\nReceived signal: SIGTERM\n");
    } else {
	user_log(LOG_LEVEL_WARNING, "\nReceived unknown signal!\n");
    }

    if (statistics_timer_enabled) {
	user_log(LOG_LEVEL_DEBUG, "Stopping statistics timer...\n");
	uv_timer_stop(&statistics_timer);
    }

    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    neat_stop_event_loop(ctx);
}

static neat_error_code
on_aborted(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;

    set_error_time(0);

    if (opCB->userData) {
	client_ctx = (struct client_ctx *)opCB->userData;

	user_log(LOG_LEVEL_WARNING, "Client %d: Aborted! Closing...\n", client_ctx->id);
	neat_close(opCB->ctx, opCB->flow);
    }

    opCB->on_error = NULL;
    neat_set_operations(opCB->ctx, opCB->flow, opCB);
    return NEAT_OK;
}

static neat_error_code
on_close(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;

    set_error_time(0);

    if (opCB->userData) {
	client_ctx = opCB->userData;
	user_log(LOG_LEVEL_INFO, "Client %d: Disconnected\n", client_ctx->id);
	clients_closed++;
	//sample("afterallclosed", clients_closed == config_expected_connecting);

	if (client_ctx->receive_buffer) {
	    free(client_ctx->receive_buffer);
	    client_ctx->receive_buffer = NULL;
	}

	free(client_ctx);
	opCB->userData = NULL;
    }

    return NEAT_OK;
}

static neat_error_code
on_error(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;

    set_error_time(1);

    if (opCB->userData) {
	client_ctx = (struct client_ctx *)opCB->userData;

	user_log(LOG_LEVEL_ERROR, "Client %d: error (%s)! Closing...\n", client_ctx->id, __func__);
	neat_close(opCB->ctx, opCB->flow);
    }

    return NEAT_OK;
}

static neat_error_code
on_readable_http(struct neat_flow_operations *opCB)
{
    neat_error_code code;
    struct client_ctx *client_ctx = opCB->userData;
    uint32_t actual;

    if (!has_read) {
	//sample("beforefirstread", 1);
	has_read = 1;
    }

    code = neat_read(opCB->ctx, opCB->flow, (unsigned char *)(client_ctx->receive_buffer + client_ctx->receive_buffer_filled), client_ctx->receive_buffer_size - client_ctx->receive_buffer_filled, &actual, NULL, 0);
    user_log(LOG_LEVEL_DEBUG, "Client %d: %s: neat_read returns %d bytes\n", client_ctx->id, __func__, actual);

    if (code != NEAT_OK) {
	if (code == NEAT_ERROR_WOULD_BLOCK) {
	    user_log(LOG_LEVEL_WARNING, "Client %d: WARNING: WOULD BLOCK!\n", client_ctx->id);
	    return NEAT_OK;
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d: error (%s): Failed to read data\n", client_ctx->id, __func__);
	    goto error;
	}
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

    if (client_ctx->pret > 0) {
	char path_buffer[client_ctx->path_len+1];
	int request_length = 0;

	memcpy(path_buffer, client_ctx->path, client_ctx->path_len);
	path_buffer[client_ctx->path_len] = '\0';
	request_length = atoi(path_buffer);

	if (request_length <= 0) {
	    user_log(LOG_LEVEL_ERROR, "Client %d: ERROR: Either a file of size 0 were requested, or an invalid file were requested\n", client_ctx->id);
	    goto error;
	}

	/*
	 * Perform different actions based on the HTTP request method received.
	 */
	if (strncmp(client_ctx->method, "GET", client_ctx->method_len) == 0) {
	    user_log(LOG_LEVEL_INFO, "Client %d: Received HTTP GET request for %d bytes\n", client_ctx->id, request_length);
	    clients_completed_reading++;
	    //sample("afterallread", clients_completed_reading == config_expected_completing);
	    if (prepare_http_response(client_ctx->headers, client_ctx->num_headers, client_ctx->path, client_ctx->path_len, &client_ctx->http_response_header, &client_ctx->http_response_header_size, &client_ctx->http_file_size, &client_ctx->keep_alive) < 0) {
		user_log(LOG_LEVEL_ERROR, "Client %d: Failed to create HTTP response to GET request\n", client_ctx->id);
		goto error;
	    }
	    user_log(LOG_LEVEL_INFO, "Client %d: Started sending HTTP response with %d bytes...\n", client_ctx->id, request_length);
	    opCB->on_writable = on_writable_http;
	    opCB->on_readable = NULL;
	} else if (strncmp(client_ctx->method, "POST", client_ctx->method_len) == 0) {
	    user_log(LOG_LEVEL_INFO, "Client %d: Started receiving HTTP POST request with %d bytes...\n", client_ctx->id, request_length);
	    if ((client_ctx->pret + request_length) == actual) {
		user_log(LOG_LEVEL_INFO, "Client %d: Received all data from POST request\n", client_ctx->id);
		clients_completed++;
		//sample("afterallread", clients_completed == config_expected_completing);
		opCB->on_writable = NULL;
		opCB->on_readable = NULL;
	    } else {
		client_ctx->http_file_size = request_length;
		client_ctx->http_file_offset = actual - client_ctx->pret;
		opCB->on_writable = NULL;
		opCB->on_readable = on_readable_http_post;
	    }
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d: Invalid HTTP request method \"%.*s\" reeived\n", client_ctx->id, client_ctx->method_len, client_ctx->method);
	    opCB->on_writable = NULL;
	    opCB->on_readable = NULL;
	    goto error;
	}
        neat_set_operations(opCB->ctx, opCB->flow, opCB);
        return NEAT_OK;
    } else if (client_ctx->pret == -1) {
	user_log(LOG_LEVEL_ERROR, "Client %d: Received invalid HTTP request\n", client_ctx->id);
	goto error;
    }

    if (client_ctx->pret != -2) {
	user_log(LOG_LEVEL_ERROR, "%s: Unexpected return code from HTTP parser!\n", __func__);
	goto error;
    }

    if (client_ctx->receive_buffer_filled == client_ctx->receive_buffer_size) {
	user_log(LOG_LEVEL_ERROR, "Client %d: HTTP request is longer than the receive buffer size!\n", client_ctx->id);
	goto error;
    }

    return NEAT_OK;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    neat_stop_event_loop(opCB->ctx);
    return NEAT_OK;
}

static neat_error_code
on_readable_http_post(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;
    uint32_t actual;
    neat_error_code code;

    client_ctx = opCB->userData;
    
    code = neat_read(opCB->ctx,
		     opCB->flow,
		     client_ctx->receive_buffer,
		     client_ctx->receive_buffer_size,
		     &actual,
		     NULL,
		     0);
    user_log(LOG_LEVEL_DEBUG, "Client %d: %s: neat_read returns %d bytes\n", client_ctx->id, __func__, actual);

    if (code != NEAT_OK) {
	if (code == NEAT_ERROR_WOULD_BLOCK) {
	    user_log(LOG_LEVEL_WARNING, "Client %d: WARNING: WOULD BLOCK!\n", client_ctx->id);
	    return NEAT_OK;
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d: ERROR: Failed to read data\n", client_ctx->id);
	    goto error;
	}
    }

    if (actual > 0) {
	client_ctx->bytes_read += actual;
	total_bytes_read += actual;
	client_ctx->http_file_offset += actual;
	if (client_ctx->http_file_offset == client_ctx->http_file_size) {
	    clients_completed++;
	    //sample("afterallread", clients_completed == config_expected_completing);
	    user_log(LOG_LEVEL_INFO, "Client %d: Received all data from POST request\n", client_ctx->id);
	}
    } else {
	user_log(LOG_LEVEL_DEBUG, "Client %d: Read 0 bytes - end of file\n", client_ctx->id);
	set_error_time(0);
    }

    return NEAT_OK;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    neat_stop_event_loop(opCB->ctx);
    return NEAT_OK;
}

static neat_error_code
on_writable_http(struct neat_flow_operations *opCB)
{
    neat_error_code code;
    struct client_ctx *client_ctx = opCB->userData;
    char *buff_ptr;
    int buff_offset;
    int send_size;

    if (!has_written) {
	//sample("beforefirstwrite", 1);
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

    code = neat_write(opCB->ctx, opCB->flow, buff_ptr + buff_offset, send_size, NULL, 0);
    user_log(LOG_LEVEL_DEBUG, "Client %d: Attempting to write %d bytes\n", client_ctx->id, send_size);

    if (code != NEAT_OK) {
	if (code == NEAT_ERROR_WOULD_BLOCK) {
	    user_log(LOG_LEVEL_WARNING, "Client %d: WARNING: WOULD BLOCK\n", client_ctx->id);
	    return NEAT_OK;
	} else if (code == NEAT_ERROR_OUT_OF_MEMORY) {
	    user_log(LOG_LEVEL_WARNING, "Client %d: WARNING: OUT OF MEMORY\n", client_ctx->id);
	    return NEAT_OK;
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d: ERROR: Failed to send HTTP response\n", client_ctx->id);
	    set_error_time(0);
            opCB->on_error = NULL;
            opCB->on_writable = NULL;
            opCB->on_all_written = NULL;
            neat_set_operations(opCB->ctx, opCB->flow, opCB);
	    user_log(LOG_LEVEL_INFO, "Client %d: Closing...\n", client_ctx->id);
	    neat_close(opCB->ctx, opCB->flow);
	}
    } else {
	if (client_ctx->http_response_header_sent) {
	    client_ctx->http_file_offset_prev = client_ctx->http_file_offset;
	    client_ctx->http_file_offset += send_size;
	    if (client_ctx->http_file_offset == client_ctx->http_file_size) {
		client_ctx->http_file_sent = 1;
	    }
	} else {
	    client_ctx->http_response_header_offset += send_size;
	    total_bytes_sent += send_size;
	    if (client_ctx->http_response_header_offset == client_ctx->http_response_header_size) {
		client_ctx->http_response_header_sent = 1;
	    }
	}

	user_log(LOG_LEVEL_DEBUG, "Client %d: Buffered %d bytes to be sent\n", client_ctx->id, send_size);
	opCB->on_writable = NULL;
	opCB->on_all_written = on_all_written_http;
    }

    neat_set_operations(opCB->ctx, opCB->flow, opCB);
    return NEAT_OK;
}

static neat_error_code
on_all_written_http(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx = opCB->userData;

    total_bytes_sent += client_ctx->http_file_offset - client_ctx->http_file_offset_prev;
    user_log(LOG_LEVEL_DEBUG, "Client %d: Sent %d bytes\n", client_ctx->id, client_ctx->http_file_offset - client_ctx->http_file_offset_prev);
    
    if (client_ctx->http_file_sent) {
	user_log(LOG_LEVEL_INFO, "Client %d: Sent HTTP response\n", client_ctx->id);
	clients_completed++;
	//sample("afterallwritten", clients_completed == config_expected_completing);
	opCB->on_writable = NULL;
	opCB->on_all_written = NULL;
	user_log(LOG_LEVEL_INFO, "Client %d: Closing...\n", client_ctx->id);
	neat_close(opCB->ctx, opCB->flow);
    } else {
	opCB->on_writable = on_writable_http;
	opCB->on_all_written = NULL;
    }
    
    neat_set_operations(opCB->ctx, opCB->flow, opCB);
    return NEAT_OK;
}

static neat_error_code
on_connected(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;

    clients_connected++;
    //sample("firstconnect", clients_connected == 1);
    sample("afterallconnected", clients_connected == config_expected_connecting);
    //sample("afterhalfconnected", clients_connected == (config_expected_connecting / 2));
    
    if ((client_ctx = calloc(1, sizeof (struct client_ctx))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((client_ctx->receive_buffer = calloc(1, config_receive_buffer_size)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	goto error;
    }

    if (config_run_mode == RUN_MODE_HTTP) {
	opCB->userData = client_ctx;
	opCB->on_all_written = NULL;
	opCB->on_readable = on_readable_http;
	opCB->on_writable = NULL;
	opCB->on_error = on_error;
	opCB->on_close = on_close;
	opCB->on_aborted = on_aborted;
    } else if (config_run_mode == RUN_MODE_CONNECTION) {
	opCB->userData = client_ctx;
	opCB->on_all_written = NULL;
	opCB->on_readable = NULL;
	opCB->on_writable = NULL;
	opCB->on_error = on_error;
	opCB->on_close = on_close;
	opCB->on_aborted = on_aborted;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode\n", __func__);
	goto error;
    }

    if (neat_set_operations(opCB->ctx, opCB->flow, opCB)) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_set_operations failed\n", __func__);
        goto error;
    }

    if (opCB->transport_protocol == NEAT_STACK_TCP) {
	tcp_connected++;
	client_ctx->is_tcp = 1;
    } else if (opCB->transport_protocol == NEAT_STACK_SCTP) {
	sctp_connected++;
	client_ctx->is_sctp = 1;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Unknown protocol type connected!\n", __func__);
	goto error;
    }

    client_ctx->id = clients_connected;
    client_ctx->receive_buffer_size = config_receive_buffer_size;
    user_log(LOG_LEVEL_INFO, "Client %d: Connected\n", client_ctx->id);  
    return NEAT_OK;
error:
    set_error_time(1);
    if (client_ctx) {
	if (client_ctx->receive_buffer) {
	    free(client_ctx->receive_buffer);
	}
	free(client_ctx);
	opCB->userData = NULL;
    }
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    neat_stop_event_loop(opCB->ctx);
    return NEAT_OK;
}

static void
print_statistics(uv_timer_t *handle)
{
    char val[10];
    
    snprintf(val, sizeof (val), "sample:%d", statistics_count);
    //sample(val, 1);

    statistics_count++;
    uv_timer_start(handle, print_statistics,
		   1000 * config_statistics_log_rate, 0);
}

int
main(int argc, char *argv[])
{
    /*
     * Here we define skeleton strings which are used as a base
     * for inserting property values later when they are extracted
     * from the command-line options.
     */

    // Skeleton of the entire property string
    static char *property_skeleton = QUOTE(
{
    %s%s%s%s%s
}
);
    
    // Skeleton of the transport property
    static char *transport_skeleton = QUOTE(
    "transport": {
        "value": [%s],
	"precedence": 1 }
);
    
    // Skeleton of the multihoming property
    static char *multihoming_skeleton = QUOTE(
    "multihoming": {
        "value": true,
        "precedence": 1
    }
);
    
    // Skeleton of the local_ips property
    static char *local_ips_skeleton = QUOTE(
    "local_ips": [
        {
            "value": "%s",
            "precedence": 1
        }
    ]
);

    /*
     * Buffers used to construct a custom JSON property string
     * based on the options given on the command-line.
     *
     * The comma_* variables is an unelegant way of binding
     * the different properties together in the final property
     * string.
     */
    char property_buffer[500];
    char transport_buffer[150];
    char local_ips_buffer[150];
    char *comma_after_transport = "";
    char *comma_after_multihoming = "";

    /*
     * Various variables.
     */
    int result = EXIT_FAILURE;
    static struct neat_flow *flow;
    static struct neat_flow_operations ops;

    /*
     * Get the start time.
     */
    if (gettimeofday(&start, NULL) == -1) {
	user_log(LOG_LEVEL_ERROR, "Failed to get current time!\n");
	error = 1;
	goto cleanup;
    }

    memset(&ops, 0, sizeof (ops));

    /*
     * Handle command-line arguments. The options are set in global
     * variables which are used throughout the rest of the code.
     */
    if (handle_command_line_arguments(argc, argv, "a:b:AH:sR:C:D:I:l:o:ck:P:mM:p:S:v:u:T:") == -1) {
	user_log(LOG_LEVEL_ERROR, "%s: Error when parsing command-line arguments!\n", __func__);
	error = 1;
	goto cleanup;
    }

    /*
     * Clear the property buffers and then add property strings
     * to the buffers if the associated options are given
     * on the command-line.
     */
    memset(transport_buffer, 0, sizeof (transport_buffer));
    memset(local_ips_buffer, 0, sizeof (local_ips_buffer));
    
    if (config_local_ip_set) {
	if (snprintf(local_ips_buffer, sizeof (local_ips_buffer), local_ips_skeleton, config_local_ip) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    if (config_neat_transport_set) {
	if (snprintf(transport_buffer, sizeof (transport_buffer), transport_skeleton, config_neat_transport) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    /*
     * Allocate signal handlers which will be used to leave the NEAT
     * event loop gracefully.
     */
    if ((signal_intr = calloc(1, sizeof (*signal_intr))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }
    signal_intr->data = signal_intr;

    if ((signal_term = calloc(1, sizeof (*signal_term))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }
    signal_term->data = signal_term;

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
    
    /*
     * Listen on a specific local address instead of all addresses if "I" option is given
     */
    NEAT_OPTARGS_DECLARE(NEAT_OPTARGS_MAX);
    NEAT_OPTARGS_INIT();

    if (config_local_ip_set) {
	NEAT_OPTARG_STRING(NEAT_TAG_LOCAL_NAME, config_local_ip);
    }

    /*
     * Build property string based on command-line options
     */
    if (config_multihoming_enabled) {
	if (strlen(transport_buffer) > 0) {
	    comma_after_transport = ", ";
	}
	if (strlen(local_ips_buffer) > 0) {
	    comma_after_multihoming = ", ";
	}
	if (snprintf(property_buffer, sizeof (property_buffer), property_skeleton, transport_buffer, comma_after_transport, multihoming_skeleton, comma_after_multihoming, local_ips_buffer) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    } else {
	if ((strlen(transport_buffer) > 0) && (strlen(local_ips_buffer) > 0)) {
	    comma_after_transport = ", ";
	}
	if (snprintf(property_buffer, sizeof (property_buffer), property_skeleton, transport_buffer, comma_after_transport, "", "", local_ips_buffer) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    if (strlen(property_buffer) > 4) {
	config_properties = property_buffer;
    }

    /*
     * Print overview of the options that are set.
     */
    print_overview(1);

    /*
     * Create the results directory if it does
     * not exist already.
     */
    if (config_sample_data) {
	char command_string[500];

	// Create results directory if not already created
	snprintf(command_string, sizeof (command_string), "mkdir -p %s", config_results_dir);
	system(command_string);
    }

    //sample("beforestart", 1);
    sample("start", 1);

    /*
     * From this point we initialize the NEAT context, create a
     * listening NEAT flow and start listening for incoming
     * connections. The callbacks will be fired once the
     * NEAT event loop has started.
     */
    user_log(LOG_LEVEL_DEBUG, "Initializing NEAT context...\n");
    
    is_client = 0;
    if ((ctx = neat_init_ctx()) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_init_ctx failed\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (config_neat_log_level == 0) {
        neat_log_level(ctx, NEAT_LOG_OFF);
    } else if (config_neat_log_level == 1) {
        neat_log_level(ctx, NEAT_LOG_ERROR);
    } else if (config_neat_log_level == 2) {
	neat_log_level(ctx, NEAT_LOG_WARNING);
    } else if (config_neat_log_level == 3) {
	neat_log_level(ctx, NEAT_LOG_INFO);
    } else {
        neat_log_level(ctx, NEAT_LOG_DEBUG);
    }

    user_log(LOG_LEVEL_DEBUG, "Creating NEAT flow for listening...\n");
    
    is_client = 0;
    if ((flow = neat_new_flow(ctx)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_new_flow failed\n", __func__);
	error = 1;
        goto cleanup;
    }

    user_log(LOG_LEVEL_DEBUG, "Setting NEAT properties...\n");
    
    is_client = 0;
    if (neat_set_property(ctx, flow, config_read_properties_set ? config_read_properties : config_properties)) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_set_property failed\n", __func__);
        error = 1;
        goto cleanup;
    }

    if (config_run_mode == RUN_MODE_HTTP) {
	ops.on_connected = on_connected;
	ops.on_error = on_error;
	ops.on_aborted = on_aborted;
	ops.on_close = on_close;
    } else if (config_run_mode == RUN_MODE_CONNECTION) {
	ops.on_connected = on_connected;
	ops.on_error = on_error;
	ops.on_aborted = on_aborted;
	ops.on_close = on_close;
    } else {
	user_log(LOG_LEVEL_ERROR, "Unknown run mode specified!\n");
	error = 1;
	goto cleanup;
    }

    user_log(LOG_LEVEL_DEBUG, "Setting NEAT callbacks...\n");
    
    if (neat_set_operations(ctx, flow, &ops)) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_set_operations failed\n", __func__);
        error = 1;
        goto cleanup;
    }

    user_log(LOG_LEVEL_DEBUG, "Calling neat_accept...\n");
    
    if (neat_accept(ctx, flow, config_port, NEAT_OPTARGS, NEAT_OPTARGS_COUNT)) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_accept failed\n", __func__);
        error = 1;
        goto cleanup;
    }

    /*
     * Get the libuv loop associated with the NEAT context and
     * attach additional handles for handling signals and
     * sampling timer.
     */
    if ((uv_loop = neat_get_event_loop(ctx)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to get handle to UV loop\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (uv_signal_init(uv_loop, signal_term) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to initialize signal handle\n", __func__);
	error = 1;
	goto cleanup;
    }
    signal_term_init = 1;

    if (uv_signal_start(signal_term, on_signal, SIGTERM) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to start signal handle for SIGTERM\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (uv_signal_init(uv_loop, signal_intr) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to initialize signal handle\n", __func__);
	error = 1;
	goto cleanup;
    }
    signal_intr_init = 1;

    if (uv_signal_start(signal_intr, on_signal, SIGINT) != 0) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to start signal handle for SIGINT\n", __func__);
	error = 1;
	goto cleanup;
    }

    if (config_statistics_log_rate > 0) {
	statistics_timer_enabled = 1;
	uv_timer_init(uv_loop, &statistics_timer);
	uv_timer_start(&statistics_timer, print_statistics,
		       1000 * config_statistics_log_rate, 0);
    }

    user_log(LOG_LEVEL_INFO, "Starting event loop...\n");
    //sample("beforeloop", 1);
    neat_start_event_loop(ctx, NEAT_RUN_DEFAULT);
    //sample("afterloop", 1);
    program_ended = 1;
    user_log(LOG_LEVEL_INFO, "Left the event loop.\n");

    result = EXIT_SUCCESS;
cleanup:
    set_error_time(0);

    if (signal_intr) {
	if (signal_intr_init) {
	    if (!uv_is_closing((uv_handle_t *)signal_intr)) {
		user_log(LOG_LEVEL_DEBUG, "Closing SIGINT handle...\n");
		uv_close((uv_handle_t *)signal_intr, NULL);
	    }
	}
    }

    if (signal_term) {
	if (signal_term_init) {
	    if (!uv_is_closing((uv_handle_t *)signal_term)) {
		user_log(LOG_LEVEL_DEBUG, "Closing SIGTERM handle...\n");
		uv_close((uv_handle_t *)signal_term, NULL);
	    }
	}
    }
    
    if (ctx) {
	user_log(LOG_LEVEL_DEBUG, "Freeing NEAT context...\n");
	neat_free_ctx(ctx);
    }

    //sample("end", 1);

    if (signal_intr) {
	free(signal_intr);
    }

    if (signal_term) {
	free(signal_term);
    }

    free_resources();
    free_resources_http();
    sanity_check(config_user_log_level);
    user_log(LOG_LEVEL_INFO, "Terminating...\n");
    
    return result;
}
