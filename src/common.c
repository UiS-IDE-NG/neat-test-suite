#include "common.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Global config values
 */

int config_benchmark = DEFAULT_BENCHMARK;
int config_expected_completing = DEFAULT_EXPECTED_COMPLETING;
int config_expected_connecting = DEFAULT_EXPECTED_CONNECTING;
int config_expected_sctp = DEFAULT_EXPECTED_SCTP;
int config_expected_tcp = DEFAULT_EXPECTED_TCP;
int config_expected_time = DEFAULT_EXPECTED_TIME;
int config_http_file_size = DEFAULT_HTTP_FILE_SIZE;
int config_http_request_size = DEFAULT_HTTP_REQUEST_SIZE;
int config_keep_alive = DEFAULT_KEEP_ALIVE;
char *config_local_ip = DEFAULT_LOCAL_IP;
int config_multihoming_enabled = DEFAULT_MULTIHOMING_ENABLED;
int config_neat_log_level = DEFAULT_NEAT_LOG_LEVEL;
char *config_neat_transport = DEFAULT_NEAT_TRANSPORT;
char *config_prefix = DEFAULT_PREFIX;
char *config_properties = DEFAULT_PROPERTIES;
char *config_read_properties = NULL;
int config_receive_buffer_size = DEFAULT_RECEIVE_BUFFER_SIZE;
char *config_results_dir = DEFAULT_RESULTS_DIR;
int config_run_mode = DEFAULT_RUN_MODE;
int config_sample_data = DEFAULT_SAMPLE_DATA;
int config_send_chunk_size = DEFAULT_SEND_CHUNK_SIZE;
int config_statistics_log_rate = DEFAULT_STATISTICS_LOG_RATE;
char *config_transport = DEFAULT_TRANSPORT;
int config_user_log_level = DEFAULT_USER_LOG_LEVEL;

/*
 * Global indicators of whether options have been set
 */

int config_benchmark_set = 0;
int config_expected_completing_set = 0;
int config_expected_connecting_set = 0;
int config_expected_sctp_set = 0;
int config_expected_tcp_set = 0;
int config_expected_time_set = 0;
int config_http_file_size_set = 0;
int config_http_request_size_set = 0;
int config_keep_alive_set = 0;
int config_local_ip_set = 0;
int config_multihoming_enabled_set = 0;
int config_neat_log_level_set = 0;
int config_neat_transport_set = 0;
int config_prefix_set = 0;
int config_properties_set = 0;
int config_read_properties_set = 0;
int config_receive_buffer_size_set = 0;
int config_results_dir_set = 0;
int config_run_mode_http_get_set = 0;
int config_run_mode_http_post_set = 0;
int config_run_mode_set = 0;
int config_sample_data_set = 0;
int config_send_chunk_size_set = 0;
int config_statistics_log_rate_set = 0;
int config_transport_set = 0;
int config_user_log_level_set = 0;

int error = 0;
int clients_connected = 0;
int clients_completed = 0;
int clients_completed_writing = 0;
int clients_completed_reading = 0;
int clients_closed = 0;
int tcp_connected = 0;
int sctp_connected = 0;
int neat_custom_properties = 0;
double elapsed_error_time = 0;
double elapsed_time = 0;
struct timeval *error_time = NULL;
struct timeval start;
struct timeval end;

long total_bytes_read = 0;
long total_bytes_sent = 0;
int has_written = 0;
int has_read = 0;
