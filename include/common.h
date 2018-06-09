#ifndef COMMON_INCLUDE_H
#define COMMON_INCLUDE_H

#include <stdlib.h>

/*
 * Log levels
 */

#define LOG_LEVEL_OFF     (0)
#define LOG_LEVEL_ERROR   (1)
#define LOG_LEVEL_WARNING (2)
#define LOG_LEVEL_INFO    (3)
#define LOG_LEVEL_DEBUG   (4)


/*
 * Run modes
 */

#define RUN_MODE_CONNECTION 0
#define RUN_MODE_HTTP 1
#define RUN_MODE_HTTP_GET 2
#define RUN_MODE_HTTP_POST 3

/*
 * Tags
 */

#define SERVER_TAG 10
#define CLIENT_TAG 20

/*
 * Default values
 */

#define DEFAULT_BENCHMARK 0
#define DEFAULT_EXPECTED_COMPLETING 0
#define DEFAULT_EXPECTED_CONNECTING 0
#define DEFAULT_EXPECTED_SCTP 0
#define DEFAULT_EXPECTED_TCP 0
#define DEFAULT_EXPECTED_TIME 0
#define DEFAULT_HE_DELAY 10
#define DEFAULT_HTTP_FILE_SIZE 1000
#define DEFAULT_HTTP_REQUEST_SIZE 123
#define DEFAULT_HTTP_REQUEST_MAX 512
#define DEFAULT_KEEP_ALIVE 0
#define DEFAULT_LOCAL_IP NULL
#define DEFAULT_MAX_HTTP_HEADER 1024
#define DEFAULT_MULTIHOMING_ENABLED 0
#define DEFAULT_NEAT_LOG_LEVEL LOG_LEVEL_WARNING
#define DEFAULT_NEAT_TRANSPORT "\"TCP\""
#define DEFAULT_NUMBER_OF_REQUESTS 1
#define DEFAULT_PORT 12327
#define DEFAULT_PREFIX "unspecified"
#define DEFAULT_PROPERTIES "{"\
    "\"transport\": {"\
        "\"value\": [\"TCP\"],"\
        "\"precedence\": 1"\
    "}"\
"}"
#define DEFAULT_RECEIVE_BUFFER_SIZE 10240
#define DEFAULT_REQUEST_RATE 0
#define DEFAULT_RESULTS_DIR "results"
#define DEFAULT_RUN_MODE RUN_MODE_CONNECTION
#define DEFAULT_SAMPLE_DATA 0
#define DEFAULT_SEND_CHUNK_SIZE 1000
#define DEFAULT_SEND_CONTINUOUS 0
#define DEFAULT_STATISTICS_LOG_RATE 0
#define DEFAULT_TRANSPORT "TCP"
#define DEFAULT_USER_LOG_LEVEL LOG_LEVEL_INFO

extern int config_benchmark;
extern int config_expected_completing;
extern int config_expected_connecting;
extern int config_expected_prefix;
extern int config_expected_sctp;
extern int config_expected_tcp;
extern int config_expected_time;
extern int config_http_file_size;
extern int config_http_request_size;
extern int config_keep_alive;
extern char *config_local_ip;
extern int config_multihoming_enabled;
extern int config_neat_log_level;
extern char *config_neat_transport;
extern char *config_prefix;
extern char *config_properties;
extern char *config_read_properties;
extern int config_receive_buffer_size;
extern char *config_results_dir;
extern int config_run_mode;
extern int config_sample_data;
extern int config_send_chunk_size;
extern int config_statistics_log_rate;
extern char *config_transport;
extern int config_user_log_level;

extern int config_benchmark_set;
extern int config_expected_completing_set;
extern int config_expected_connecting_set;
extern int config_expected_prefix_set;
extern int config_expected_sctp_set;
extern int config_expected_tcp_set;
extern int config_expected_time_set;
extern int config_http_file_size_set;
extern int config_http_request_size_set;
extern int config_keep_alive_set;
extern int config_local_ip_set;
extern int config_multihoming_enabled_set;
extern int config_neat_log_level_set;
extern int config_neat_transport_set;
extern int config_prefix_set;
extern int config_properties_set;
extern int config_read_properties_set;
extern int config_receive_buffer_size_set;
extern int config_results_dir_set;
extern int config_run_mode_http_get_set;
extern int config_run_mode_http_post_set;
extern int config_run_mode_set;
extern int config_sample_data_set;
extern int config_send_chunk_size_set;
extern int config_statistics_log_rate_set;
extern int config_transport_set;
extern int config_user_log_level_set;

extern int error;
extern int clients_connected;
extern int clients_completed;
extern int clients_completed_writing;
extern int clients_completed_reading;
extern int clients_closed;
extern int tcp_connected;
extern int sctp_connected;
extern int neat_custom_properties;
extern double elapsed_error_time;
extern double elapsed_time;
extern struct timeval *error_time;
extern struct timeval start;
extern struct timeval end;

extern long total_bytes_read;
extern long total_bytes_sent;
extern int has_written;
extern int has_read;

#endif
