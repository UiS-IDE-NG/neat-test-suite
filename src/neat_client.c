#include "common.h"
#include "client_common.h"
#include "http_common.h"
#include "util_new.h"

#include <neat.h>
#include <uv.h>
#include <picohttpparser.h>

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define QUOTE(...) #__VA_ARGS__

/* Structure associated with every flow */
struct client_ctx {
    int id;
    int indicative_id;
    int is_tcp;
    int is_sctp;
    char *http_request;
    int http_request_size;
    int http_request_offset;
    int http_request_offset_prev;
    int http_request_sent;
    int http_file_size;
    int http_file_offset;
    struct timespec before_connected;
    struct timespec after_connected;
    struct neat_flow *client_flow;
    char *receive_buffer;
    int receive_buffer_size;
    int receive_buffer_filled;
    int receive_buffer_filled_prev;
    int bytes_read;
    struct neat_flow_operations *ops;
    int minor_version;
    int status;
    char *msg;
    int msg_len;
    int pret;
    struct phr_header headers[100];
    int num_headers;
};

/*
 * General globals
 */


// Will be set to 0 after the first time on_readable is called
static int first_read = 1;

// Contains information about the CPU time of the process
static struct tms *process_time = NULL;

// The global NEAT context which all flows are associated with
static struct neat_ctx *ctx = NULL;

// Array of NEAT flows for all client requests
static struct neat_flow **flows = NULL;

// UV loop associated with the global NEAT event loop
static uv_loop_t *uv_loop = NULL;

// Array of client contexts
static struct client_ctx *client_contexts = NULL;

// UV timer for periodic statistics summary
static uv_timer_t statistics_timer;

static int statistics_count = 0;

// The amount of bytes that are read in a specified time interval (used to print throughput periodically)
static int bytes_read_in_interval = 0;

// The hostname of the server
static char *remote_hostname = NULL;

// The port number of the server
static unsigned long remote_port = 0;

// UV timer for timing exponentially distributed requests
static uv_timer_t timer;

// Signifies whether an interrupt signal of some kind has been received
static int signal_received = 0;

// Signifies whether the program has left the NEAT event loop
static int left_event_loop = 0;

// Contains a HTTP request
static char *request = NULL;
static int request_length = 0;

static uv_signal_t *signal_intr = NULL;
static int signal_intr_init = 0;
static uv_signal_t *signal_term = NULL;
static int signal_term_init = 0;

static void print_usage(void);
static void on_signal(uv_signal_t *handle, int signum);
static neat_error_code on_aborted(struct neat_flow_operations *opCB);
static neat_error_code on_error(struct neat_flow_operations *opCB);
static neat_error_code on_close(struct neat_flow_operations *opCB);
static neat_error_code on_writable_http(struct neat_flow_operations *opCB);
static neat_error_code on_all_written_http(struct neat_flow_operations *opCB);
static neat_error_code on_readable_http(struct neat_flow_operations *opCB);
static neat_error_code on_readable_http_get(struct neat_flow_operations *opCB);
static neat_error_code on_connected(struct neat_flow_operations *opCB);

static void
print_usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "$ ./neat_client [options] host port\n");
    fprintf(stderr, "  This client supports a HTTP mode and a non-HTTP mode.\n");
    fprintf(stderr, "  In HTTP mode it will first connect to the server and then send HTTP request(s)\n");
    fprintf(stderr, "  for certain objects located at the server.\n");
    fprintf(stderr, "  In non-HTTP mode it will await for data as soon as the client successfully connects.\n");
    fprintf(stderr, "  -A                   Expect a filename prefix for sampling results to be given.\n");
    fprintf(stderr, "                       DEFAULT: 0 (disabled)\n");
    fprintf(stderr, "  -s                   Enable sampling of CPU and memory data.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_SAMPLE_DATA);
    fprintf(stderr, "  -R <path>            The name of the sample results directory.\n");
    fprintf(stderr, "                       DEFAULT: \"%s\"\n", DEFAULT_RESULTS_DIR);
    fprintf(stderr, "  -C <flows>           Expect the specified number of connecting requests.\n");
    fprintf(stderr, "                       DEFAULT: %d%s\n", DEFAULT_EXPECTED_CONNECTING, DEFAULT_EXPECTED_CONNECTING ? "" : " (some CPU and memory samples disabled)");
    fprintf(stderr, "  -D <flows>           Expect the specified number of completing requests.\n");
    fprintf(stderr, "                       DEFAULT: %d%s\n", DEFAULT_EXPECTED_COMPLETING, DEFAULT_EXPECTED_COMPLETING ? "" : " (some CPU and memory samples disabled)");
    fprintf(stderr, "  -H size              Send HTTP request(s) for a data object of the specified size.\n");
    fprintf(stderr, "                       DEFAULT: 0 (disabled)\n");
    fprintf(stderr, "  -i IP                The local IP to use as an endpoint.\n");
    fprintf(stderr, "                       DEFAULT: Use all local endpoints\n");
    fprintf(stderr, "  -l buffer_size       The size of the receiving buffer.\n");
    fprintf(stderr, "                       DEFAULT: 10240\n");
    fprintf(stderr, "  -P filename:         Use the properties in the specified file.\n");
    fprintf(stderr, "                       DEFAULT: TCP\n");
    fprintf(stderr, "  -m                   Enable multihoming.\n");
    fprintf(stderr, "                       DEFAULT: 0 (disabled)\n");
    fprintf(stderr, "  -M protocol          Use the specified transport protocol.\n");
    fprintf(stderr, "                       DEFAULT: TCP (possible: TCP, SCTP, HE)\n");
    fprintf(stderr, "  -n requests          The number of requests to start.\n");
    fprintf(stderr, "                       DEFAULT: 1\n");
    fprintf(stderr, "  -r rate              Enable opening flows in an exponentially distributed manner with an average rate.\n");
    fprintf(stderr, "                       DEFAULT: 0\n");
    fprintf(stderr, "  -v neat_log_level:   Use the specified NEAT log level.\n");
    fprintf(stderr, "                       DEFAULT: 0\n");
    fprintf(stderr, "  -u user_log_level    Enable printing debug messages outside NEAT.\n");
    fprintf(stderr, "                       DEFAULT: 2\n");
    fprintf(stderr, "  -S statistics_rate   Statistics will be logged at this rate (in seconds).\n");
    fprintf(stderr, "                       DEFAULT: 0 (disabled))\n");
    fprintf(stderr, "  -T <seconds>         Expected number of seconds to run without errors.\n");
    fprintf(stderr, "                       DEFAULT: %d\n", DEFAULT_EXPECTED_TIME);
}

static void
on_signal(uv_signal_t *handle, int signum) {
    if (signum == SIGINT) {
	user_log(LOG_LEVEL_WARNING, "\nReceived signal: SIGINT\n");
    } else if (signum == SIGTERM) {
	user_log(LOG_LEVEL_WARNING, "\nReceived signal: SIGTERM\n");
    } else {
	user_log(LOG_LEVEL_ERROR, "\nReceived unknown signal!\n");
    }

    set_error_time(0);

    if (config_request_rate_set) {
	user_log(LOG_LEVEL_DEBUG, "Stopping exponential timer...\n");
	uv_timer_stop(&timer);
    }

    if (config_statistics_log_rate > 0) {
	user_log(LOG_LEVEL_DEBUG, "Stopping statistics timer...\n");
	uv_timer_stop(&statistics_timer);
    }

    signal_received = 1;
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
	user_log(LOG_LEVEL_ERROR, "Client %d: Aborted! Closing...\n", client_ctx->id);
	neat_close(opCB->ctx, opCB->flow);
    }

    opCB->on_error = NULL;
    neat_set_operations(opCB->ctx, opCB->flow, opCB);
    return NEAT_OK;
}

static neat_error_code
on_error(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;

    set_error_time(1);
    client_ctx = opCB->userData;

    if (opCB->userData) {
	client_ctx = (struct client_ctx *)opCB->userData;
	user_log(LOG_LEVEL_ERROR, "Client %d: error (%s)! Closing...\n", client_ctx->id, __func__);
	neat_close(opCB->ctx, opCB->flow);
    }

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
	//sample("afterallclosed", clients_closed == config_number_of_requests);
	
	if (client_ctx->receive_buffer) {
	    free(client_ctx->receive_buffer);
	    client_ctx->receive_buffer = NULL;
	}

	if (client_ctx->ops) {
	    free(client_ctx->ops);
	    client_ctx->ops = NULL;
	}

	if (clients_completed == config_number_of_requests) {
	    user_log(LOG_LEVEL_INFO, "All clients are done!\n");

	    if (!signal_received) {
		user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
		neat_stop_event_loop(ctx);
	    }
	}

	opCB->userData = NULL;
    }
    
    return NEAT_OK;
}

static neat_error_code
on_writable_http(struct neat_flow_operations *opCB)
{
    neat_error_code code;
    struct client_ctx *client_ctx = opCB->userData;
    int send_size;

    if (!has_written) {
	//sample("beforefirstwrite", 1);
	has_written = 1;
    }

    send_size = client_ctx->http_request_size - client_ctx->http_request_offset;

    if (send_size > config_send_chunk_size) {
	send_size = config_send_chunk_size;
    }

    code = neat_write(opCB->ctx, opCB->flow, client_ctx->http_request + client_ctx->http_request_offset, send_size, NULL, 0);
    user_log(LOG_LEVEL_DEBUG, "Client %d: Attempting to write %d bytes\n", client_ctx->id, send_size);

    if (code != NEAT_OK) {
	if (code == NEAT_ERROR_WOULD_BLOCK) {
	    user_log(LOG_LEVEL_WARNING, "Client %d: WARNING: WOULD BLOCK\n", client_ctx->id);
	    return NEAT_OK;
	} else {
	    user_log(LOG_LEVEL_ERROR, "Client %d: ERROR: Failed to send HTTP request\n", client_ctx->id);
	    user_log(LOG_LEVEL_ERROR, "  Attempted to send %d bytes\n", send_size);
	    user_log(LOG_LEVEL_ERROR, "  Sent %d / %d\n", client_ctx->http_request_offset, client_ctx->http_request_size);
	    set_error_time(1);
	    opCB->on_error = NULL;
	    opCB->on_writable = NULL;
	    opCB->on_all_written = NULL;
	    user_log(LOG_LEVEL_INFO, "Client %d: Closing...\n", client_ctx->id);
	    neat_close(opCB->ctx, opCB->flow);
	}
    } else {
	client_ctx->http_request_offset_prev = client_ctx->http_request_offset;
	client_ctx->http_request_offset += send_size;
	if (client_ctx->http_request_offset == client_ctx->http_request_size) {
	    client_ctx->http_request_sent = 1;
	}
	user_log(LOG_LEVEL_DEBUG, "Client %d: Buffered %d bytes to be sent\n", client_ctx->id, send_size);
	opCB->on_writable = NULL;
	opCB->on_all_written = on_all_written_http;
    }
    
    if (neat_set_operations(opCB->ctx, opCB->flow, opCB)) {
	user_log(LOG_LEVEL_ERROR, "%s: neat_set_operations failed\n", __func__);
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
on_all_written_http(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx = opCB->userData;

    total_bytes_sent += client_ctx->http_request_offset - client_ctx->http_request_offset_prev;
    user_log(LOG_LEVEL_DEBUG, "Client %d: Sent %d bytes\n", client_ctx->id, client_ctx->http_request_offset - client_ctx->http_request_offset_prev);
    
    if (client_ctx->http_request_sent) {
	user_log(LOG_LEVEL_INFO, "Client %d: Sent HTTP request\n", client_ctx->id);
	clients_completed_writing++;
	//sample("afterallwritten", clients_completed_writing == config_expected_completing);
	if (config_run_mode == RUN_MODE_HTTP_POST) {
	    opCB->on_writable = NULL;
	    opCB->on_all_written = NULL;
	    clients_completed++;
	    user_log(LOG_LEVEL_INFO, "Client %d: Closing...\n", client_ctx->id);
	    neat_close(opCB->ctx, opCB->flow);
	}
    } else {
	opCB->on_writable = on_writable_http;
	opCB->on_all_written = NULL;
    }

    neat_set_operations(opCB->ctx, opCB->flow, opCB);
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
	    user_log(LOG_LEVEL_ERROR, "Client %d: ERROR: Either a file of size 0 were requested, or an invalid file were requested\n", client_ctx->id);
	    goto error;
	}

	user_log(LOG_LEVEL_INFO, "Client %d: Started receiving HTTP response with %d bytes...\n", client_ctx->id, request_length);

	if ((client_ctx->pret + request_length) == actual) {
	    user_log(LOG_LEVEL_INFO, "Client %d: Received all data from HTTP response\n", client_ctx->id);
	    clients_completed++;
	    //sample("afterallread", clients_completed == config_expected_completing);
	    opCB->on_writable = NULL;
	} else {
	    client_ctx->http_file_size = request_length;
	    client_ctx->http_file_offset = actual - client_ctx->pret;
	    opCB->on_writable = NULL;
	    opCB->on_readable = on_readable_http_get;
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
on_readable_http_get(struct neat_flow_operations *opCB)
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
	    user_log(LOG_LEVEL_INFO, "Client %d: Received all data from HTTP response\n", client_ctx->id);
	    //sample("afterallread", clients_completed == config_expected_completing);
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
on_connected(struct neat_flow_operations *opCB)
{
    struct client_ctx *client_ctx;

    client_ctx = opCB->userData;

    /*
     * Sample the time when the flow has established a connection.
     */
    if (get_time(&(client_ctx->after_connected)) == -1) {
	user_log(LOG_LEVEL_ERROR, "Client %d: Failed to sample time data\n", clients_connected + 1);
	goto error;
    }

    clients_connected++;
    //sample("firstconnect", clients_connected == 1);
    sample("afterallconnected", clients_connected == config_number_of_requests);
    //sample("afterhalfconnected", clients_connected == (config_number_of_requests / 2));
    client_ctx->id = clients_connected;
    user_log(LOG_LEVEL_INFO, "Client %d: Connected\n", client_ctx->id);
    
    if ((client_ctx->receive_buffer = calloc(1, config_receive_buffer_size)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	goto error;
    }

    opCB->on_error = on_error;
    opCB->on_close = on_close;
    opCB->on_aborted = on_aborted;
    opCB->on_all_written = NULL;

    if (config_run_mode == RUN_MODE_HTTP_GET) {
	user_log(LOG_LEVEL_INFO, "Client %d: Sending HTTP GET request...\n", client_ctx->id);
	opCB->on_writable = on_writable_http;
	opCB->on_readable = on_readable_http;
    } else if (config_run_mode == RUN_MODE_HTTP_POST) {
	user_log(LOG_LEVEL_INFO, "Client %d: Sending HTTP POST request...\n", client_ctx->id);
	opCB->on_writable = on_writable_http;
	opCB->on_readable = NULL;
    } else if (config_run_mode == RUN_MODE_CONNECTION) {
	opCB->on_writable = NULL;
	opCB->on_readable = NULL;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode!\n", __func__);
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

    client_ctx->http_request = request;
    client_ctx->http_request_size = request_length;
    client_ctx->receive_buffer_size = config_receive_buffer_size;
    return NEAT_OK;
error:
    set_error_time(1);
    user_log(LOG_LEVEL_INFO, "Stopping event loop...\n");
    neat_stop_event_loop(opCB->ctx);
    return NEAT_OK;
}

static int
next_request_time(double rate)
{
    int result;

    result = (int)(- log(1.0 - ((double)random() / ((unsigned long)RAND_MAX + 1))) / rate);

    if (result < 1) {
	result = 1;
    }

    return result;
}

static void
send_new_request(uv_timer_t *handle)
{
    struct neat_flow *flow;
    struct neat_flow_operations *ops;
    struct client_ctx *client_ctx;

    if ((flow = neat_new_flow(ctx)) == NULL) {
	fprintf(stderr, "%s - error: could not create new NEAT flow\n", __func__);
	neat_stop_event_loop(ctx);
	return;
    }

    if (neat_set_property(ctx, flow, config_read_properties_set ? config_read_properties : config_properties)) {
	fprintf(stderr, "%s - error: neat_set_property\n", __func__);
	neat_stop_event_loop(ctx);
	return;
    }

    if ((ops = malloc(sizeof (*ops))) == NULL) {
	fprintf(stderr, "Failed to allocate memory!\n");
	neat_stop_event_loop(ctx);
	return;
    }

    memset(ops, 0, sizeof (*ops));

    if (neat_open(ctx, flow, remote_hostname, remote_port, NULL, 0) != NEAT_OK) {
	fprintf(stderr, "%s - error: neat_open\n", __func__);
	free(ops);
	neat_stop_event_loop(ctx);
	return;
    }

    if ((client_ctx = malloc(sizeof (*client_ctx))) == NULL) {
	fprintf(stderr, "Failed to allocate memory!\n");
	free(ops);
	neat_stop_event_loop(ctx);
	return;
    }

    client_ctx->id = -1;
    client_ctx->client_flow = flow;
    client_ctx->receive_buffer = NULL;
    client_ctx->ops = ops;
    
    ops->userData = client_ctx;
    ops->on_connected = on_connected;
    ops->on_error = on_error;
    ops->on_close = on_close;

    if (neat_set_operations(ctx, flow, ops)) {
	fprintf(stderr, "%s - error: neat_set_operations\n", __func__);
	free(ops);
	free(client_ctx);
	neat_stop_event_loop(ctx);
	return;
    }

    uv_timer_start(handle, send_new_request,
		   (uint64_t)next_request_time((double)1.0/config_request_rate), 0);
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
    %s%s%s%s%s%s%s
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

    // Skeleton of the __he_delay property
    static char *he_delay_skeleton = QUOTE(
    "__he_delay": {
        "value": %d
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
    char he_delay_buffer[150];
    char *comma_after_transport = "";
    char *comma_after_multihoming = "";
    char *comma_after_local_ips = "";

    /*
     * Other variables.
     */
    int result = EXIT_SUCCESS;
    static struct neat_flow_operations *ops = NULL;
    FILE *connection_times_file;
    char connection_times_filename[500];

    /*
     * Calculate the start time of the application.
     */
    if (gettimeofday(&start, NULL) == -1) {
	user_log(LOG_LEVEL_ERROR, "Failed to get start time!\n");
	error = 1;
	goto cleanup;
    }

    /*
     * Handle command-line arguments. The options are set in global
     * variables which are used throughout the rest of the code.
     */
    if (handle_command_line_arguments(argc, argv, "Aa:b:d:sR:C:D:h:H:i:l:o:P:n:mM:r:v:u:S:T:") == -1) {
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
    memset(he_delay_buffer, 0, sizeof (he_delay_buffer));
    
    if (config_neat_transport_set) {
	if (snprintf(transport_buffer, sizeof (transport_buffer), transport_skeleton, config_neat_transport) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    if (config_local_ip_set) {
	if (snprintf(local_ips_buffer, sizeof (local_ips_buffer), local_ips_skeleton, config_local_ip) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }
    
    if (config_he_delay_set) {
	if (snprintf(he_delay_buffer, sizeof (he_delay_buffer), he_delay_skeleton, config_he_delay) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    /*
     * Set remote hostname and port based on whether a prefix is
     * given as last argument.
     */
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
	if (prepare_http_request(remote_hostname, &request, &request_length) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to construct HTTP request\n", __func__);
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

    /*
     * Build the property string based on command-line options.
     */
    if (config_multihoming_enabled) {
	if (strlen(transport_buffer) > 0) {
	    comma_after_transport = ", ";
	}
	if (strlen(local_ips_buffer) > 0) {
	    comma_after_multihoming = ", ";
	}
	if (strlen(he_delay_buffer) > 0) {
	    comma_after_local_ips = ", ";
	}
	if (snprintf(property_buffer, sizeof (property_buffer), property_skeleton, transport_buffer, comma_after_transport, multihoming_skeleton, comma_after_multihoming, local_ips_buffer, comma_after_local_ips, he_delay_buffer) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    } else {
	if ((strlen(transport_buffer) > 0) && (strlen(local_ips_buffer) > 0)) {
	    comma_after_transport = ", ";
	    if (strlen(he_delay_buffer) > 0) {
		comma_after_local_ips = ", ";
	    }
	} else if ((strlen(transport_buffer) > 0) || (strlen(local_ips_buffer) > 0)) {
	    if (strlen(he_delay_buffer) > 0) {
		comma_after_local_ips = ", ";
	    }
	}

	if (snprintf(property_buffer, sizeof (property_buffer), property_skeleton, transport_buffer, comma_after_transport, "", "", local_ips_buffer, comma_after_local_ips, he_delay_buffer) < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	    error = 1;
	    goto cleanup;
	}
    }

    /*
     * Use different properties than the default if custom properties are
     * given.
     */
    if (strlen(property_buffer) > 4) {
	config_properties = property_buffer;
    }

    if ((flows = calloc(config_number_of_requests, sizeof (*flows))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }

    if ((ops = calloc(config_number_of_requests, sizeof (*ops))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }

    /* Create a context for each flow */
    if ((client_contexts = calloc(config_number_of_requests, sizeof (*client_contexts))) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	error = 1;
	goto cleanup;
    }

    /*
     * Print overview of the options that are set.
     */
    print_overview(1, remote_hostname, remote_port);

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
     * Here we initialize the event loop.
     */
    user_log(LOG_LEVEL_DEBUG, "Initializing NEAT context...\n");

    is_client = 1;
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
    
    is_client = 1;
    if (config_request_rate_set) {
	if ((uv_loop = neat_get_event_loop(ctx)) == NULL) {
	    fprintf(stderr, "Failed to get handle to UV loop\n");
	    result = EXIT_FAILURE;
	    error = 1;
	    goto cleanup;
	}

    is_client = 1;
	uv_timer_init(uv_loop, &timer);
	uv_timer_start(&timer, send_new_request,
		       (uint64_t)next_request_time((double)1.0/config_request_rate), 0);
	goto start_loop;
    }

    /*
     * Sample current time + more before for-loop
     */
    //sample("beforeforloop", 1);

    for (int i = 0; i < config_number_of_requests; i++) {
	struct client_ctx *client_ctx = (client_contexts + i);
	client_ctx->id = -1;
	client_ctx->indicative_id = i + 1;

	user_log(LOG_LEVEL_DEBUG, "Creating new flow (%d)...\n", i + 1);
	
    is_client = 1;
 	if ((flows[i] = neat_new_flow(ctx)) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: neat_new_flow error\n", __func__);
	    error = 1;
 	    goto cleanup;
 	}

	client_ctx->client_flow = flows[i];

    is_client = 1;
 	if (neat_set_property(ctx, flows[i], config_read_properties_set ? config_read_properties : config_properties)) {
	    user_log(LOG_LEVEL_ERROR, "%s: neat_set_property error\n", __func__);
	    error = 1;
 	    goto cleanup;
 	}

 	ops[i].userData = client_ctx;
 	ops[i].on_connected = on_connected;
	ops[i].on_aborted = on_aborted;
 	ops[i].on_error = on_error;
	ops[i].on_close = on_close;
 
 	if (neat_set_operations(ctx, flows[i], ops + i)) {
	    user_log(LOG_LEVEL_ERROR, "%s: neat_set_operations error\n", __func__);
	    error = 1;
 	    goto cleanup;
 	}

	user_log(LOG_LEVEL_INFO, "Opening connection to %s (%d)...\n", remote_hostname, i + 1);

	if (get_time(&(client_ctx->before_connected)) == -1) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to sample time data for flow\n", __func__);
	    error = 1;
	    goto cleanup;
	}

    is_client = 1;
 	if (neat_open(ctx, flows[i], remote_hostname, remote_port, NULL, 0) != NEAT_OK) {
	    user_log(LOG_LEVEL_ERROR, "%s: neat_open error\n", __func__);
	    error = 1;
 	    goto cleanup;
        }
    }

start_loop:

    if ((uv_loop = neat_get_event_loop(ctx)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "Failed to get handle to UV loop\n");
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
	uv_timer_init(uv_loop, &statistics_timer);
	uv_timer_start(&statistics_timer, print_statistics,
		       1000 * config_statistics_log_rate, 0);
	statistics_timer.data = &statistics_timer;
    }

    user_log(LOG_LEVEL_INFO, "Starting event loop...\n");
    //sample("beforeloop", 1);
    neat_start_event_loop(ctx, NEAT_RUN_DEFAULT);
    left_event_loop = 1;
    //sample("afterloop", 1);
    user_log(LOG_LEVEL_INFO, "Left the event loop.\n");

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
    
    for (int i = 0; i < config_number_of_requests; i++) {
	if (config_sample_data) {
	    struct timespec diff_time;
	    struct client_ctx *client_ctx = (client_contexts + i);
	    subtract_time(&(client_ctx->before_connected), &(client_ctx->after_connected), &diff_time);
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

    if (signal_intr) {
	if (signal_intr_init) {
	    if (!uv_is_closing((uv_handle_t *)signal_intr)) {
		if (config_user_log_level >= LOG_LEVEL_DEBUG) {
		    fprintf(stderr, "Closing SIGINT handle...\n");
		}
		uv_close((uv_handle_t *)signal_intr, NULL);
	    }
	}
    }

    if (signal_term) {
	if (signal_term_init) {
	    if (!uv_is_closing((uv_handle_t *)signal_term)) {
		if (config_user_log_level >= LOG_LEVEL_DEBUG) {
		    fprintf(stderr, "Closing SIGTERM handle...\n");
		}
		uv_close((uv_handle_t *)signal_term, NULL);
	    }
	}
    }

    if (request) {
	free(request);
    }

    if (flows) {
	free(flows);
    }

    if (ops) {
	free(ops);
    }
    
    if (client_contexts) {
	free(client_contexts);
    }

    if (ctx) {
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
