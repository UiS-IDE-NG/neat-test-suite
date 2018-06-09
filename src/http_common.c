#include "common.h"
#include "util_new.h"
#include "picohttpparser.h"

#if defined(__linux__)
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <linux/random.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *http_file = NULL;

int
prepare_http_response(struct phr_header *headers, int num_headers, char *path, int path_len, char **header_buffer_arg, int *header_buffer_len_arg, int *payload_len_arg, int *keep_alive_enabled_arg)
{
    /*
     * Buffer for containing the HTTP response header
     */
    unsigned char *header_buffer = NULL;
    int header_length = 0;
    int payload_length = 0;

    /*
     * Buffer for containing the path received
     * by the HTTP client. The path will have the
     * form <size>.txt to represent that a HTTP
     * object of size <size> is requested.
     */
    char path_buffer[path_len+1];

    /*
     * Various variables.
     */
    int i;
    char misc_buffer[DEFAULT_MAX_HTTP_HEADER];
    char *http_header_connection_keep_alive  = "Connection: Keep-Alive";

    /*
     * For each header field, check if it specifies that
     * Keep-Alive is required.
     */
    *keep_alive_enabled_arg = 0;

    for (i = 0; i < num_headers; i++) {
        // Build string from name/value pair
        snprintf(misc_buffer, DEFAULT_MAX_HTTP_HEADER, "%.*s: %.*s",
	    headers[i].name_len,
            headers[i].name,
            headers[i].value_len,
            headers[i].value);

        // Header contains Keep-Alive information
        if (strncasecmp(misc_buffer, http_header_connection_keep_alive, strlen(http_header_connection_keep_alive)) == 0 &&
            config_keep_alive == 1) {
	    *keep_alive_enabled_arg = 1;
        }
    }

    /*
     * Determine the size of the payload.
     */
    memcpy(path_buffer, path, path_len);
    path_buffer[path_len] = '\0';
    payload_length = atoi(path_buffer);

    /*
     * HTTP file data pool is smaller than the requested HTTP object. Return with error.
     */
    if (config_http_file_size < payload_length) {
	user_log(LOG_LEVEL_ERROR, "%s: HTTP file too small to satisfy HTTP request\n", __func__);
	goto error;
    }

    /*
     * Construct the HTTP response header.
     */
    if ((header_buffer = calloc(1, DEFAULT_MAX_HTTP_HEADER)) == NULL) {
	user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	goto error;
    }

    if ((header_length = snprintf((char *) header_buffer, DEFAULT_MAX_HTTP_HEADER,
				  "HTTP/1.1 200 OK\r\n"
				  "Server: HTTP server\r\n"
				  "Content-Length: %u\r\n"
				  "Connection: Close\r\n"
				  "\r\n",
				  payload_length)) < 0) {
	user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
	goto error;
    }

    /*
     * Assign new HTTP response information to arguments.
     */
    *header_buffer_arg = header_buffer;
    *header_buffer_len_arg = header_length;
    *payload_len_arg = payload_length;

    return 0;
error:
    if (header_buffer) {
	free(header_buffer);
    }
    set_error_time(1);
    return -1;
}

int
create_http_file(void)
{
    /*
     * Allocate the HTTP file which will be used as a data pool
     * when HTTP requests arrive later, and a subset of the data
     * will be sent as response.
     */
    if ((config_run_mode == RUN_MODE_HTTP_GET) || (config_run_mode == RUN_MODE_HTTP_POST) || (config_run_mode == RUN_MODE_HTTP)) {
	if ((http_file = calloc(1, config_http_file_size)) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory for HTTP file!\n", __func__);
	    goto error;
	}

#if defined(__FreeBSD__)
	arc4random_buf(http_file, config_http_file_size);
#endif
#if defined(__linux__)
	if (syscall(SYS_getrandom, http_file, config_http_file_size, 0) == -1) {
	    if (config_user_log_level >= LOG_LEVEL_ERROR) {
		perror("getrandom");
	    }
	    goto error;
	}
#endif
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode %d\n", __func__, config_run_mode);
	goto error;
    }

    return 0;
error:
    if (http_file) {
	free(http_file);
	http_file = NULL;
    }
    set_error_time(1);
    return -1;
}

int
prepare_http_request(char *remote_hostname, char **request_arg, int *request_length_arg)
{
    char *buffer = NULL;
    int buffer_length;
    char *new_buffer = NULL;
    int header_length = 0;

    if (config_run_mode == RUN_MODE_HTTP_GET) {
	/*
	 * Allocate buffer for containing the HTTP GET request.
	 */
	if ((buffer = calloc(1, DEFAULT_HTTP_REQUEST_MAX)) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	    goto error;
	}

	/*
	 * Build the HTTP GET request message.
	 */
	if (config_http_request_size > 0) {
	    if ((buffer_length = snprintf(buffer, DEFAULT_HTTP_REQUEST_MAX, "GET %d.txt HTTP/1.1\r\nHost: %s\r\nUser-agent: libneat\r\nConnection: close\r\n\r\n", config_http_request_size, remote_hostname)) < 0) {
		user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
		goto error;
	    }
	} else {
	    user_log(LOG_LEVEL_ERROR, "HTTP GET request must request data object larger than 0 bytes\n");
	    goto error;
	}
    } else if (config_run_mode == RUN_MODE_HTTP_POST) {
	/*
	 * Allocate buffer for containing the header of the HTTP POST request.
	 */
	if ((buffer = calloc(1, DEFAULT_HTTP_REQUEST_MAX)) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	    goto error;
	}

	/*
	 * Build the HTTP POST request header.
	 */
	if (config_http_request_size > 0) {
	    if ((header_length = snprintf(buffer, DEFAULT_HTTP_REQUEST_MAX, "POST %d.txt HTTP/1.1\r\nHost: %s\r\nUser-agent: libneat\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", config_http_request_size, remote_hostname, config_http_request_size)) < 0) {
		user_log(LOG_LEVEL_ERROR, "%s: snprintf error\n", __func__);
		goto error;
	    }
	} else {
	    user_log(LOG_LEVEL_ERROR, "%s: HTTP POST request must send data object larger than 0 bytes\n", __func__);
	    goto error;
	}

	if ((new_buffer = realloc(buffer, header_length + config_http_request_size)) == NULL) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to allocate memory!\n", __func__);
	    goto error;
	}

	buffer = new_buffer;

	if (create_http_file() < 0) {
	    user_log(LOG_LEVEL_ERROR, "%s: Failed to create HTTP file to send in HTTP POST request\n", __func__);
	    goto error;
	}

	memcpy(buffer + header_length, http_file, config_http_request_size);
	buffer_length = header_length + config_http_request_size;
    } else {
	user_log(LOG_LEVEL_ERROR, "%s: Invalid run mode %d\n", __func__, config_run_mode);
	return -1;
    }
    *request_arg = buffer;
    *request_length_arg = buffer_length;
    return 0;
error:
    set_error_time(1);
    if (buffer) {
	free(buffer);
    }
    return -1;
}

void
free_resources_http(void)
{
    if (http_file) {
	free(http_file);
	http_file = NULL;
    }
}
