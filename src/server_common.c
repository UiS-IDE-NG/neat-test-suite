#include "server_common.h"
#include "common.h"
#include "util_new.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int config_port = DEFAULT_PORT;
int config_send_continuous = DEFAULT_SEND_CONTINUOUS;

int config_port_set = 0;
int config_send_continuous_set = 0;

int
handle_command_line_arguments(int argc, char *argv[], char *options)
{
    int arg;

    while ((arg = getopt(argc, argv, options)) != -1) {
        switch(arg) {
	case 'D':
	    config_expected_completing = atoi(optarg);
	    config_expected_completing_set = 1;
	    break;
	case 'C':
	    config_expected_connecting = atoi(optarg);
	    config_expected_connecting_set = 1;
	    break;
	case 'b':
	    config_expected_sctp = atoi(optarg);
	    config_expected_sctp_set = 1;
	    break;
	case 'a':
	    config_expected_tcp = atoi(optarg);
	    config_expected_tcp_set = 1;
	    break;
	case 'T':
	    config_expected_time = atoi(optarg);
	    config_expected_time_set = 1;
	    break;
	case 'H':
	    config_http_file_size = atoi(optarg);
	    config_http_file_size_set = 1;
	    config_run_mode = RUN_MODE_HTTP;
	    config_run_mode_set = 1;
	    break;
	case 'k':
	    config_keep_alive = 1;
	    config_keep_alive_set = 1;
	case 'I':
	    config_local_ip = strdup(optarg);
	    config_local_ip_set = 1;
	    neat_custom_properties = 1;
	    break;
	case 'm':
	    config_multihoming_enabled = 1;
	    config_multihoming_enabled_set = 1;
	    neat_custom_properties = 1;
	    break;
	case 'v':
	    config_neat_log_level = atoi(optarg);
	    config_neat_log_level_set = 1;
	    break;
	case 'p':
	    config_port = atoi(optarg);
	    config_port_set = 1;
	    break;
	case 'A':
	    config_prefix = argv[argc - 1];
	    config_prefix_set = 1;
	    break;
	case 'P':
	    if (read_file(optarg, &config_read_properties) < 0) {
		user_log(LOG_LEVEL_ERROR, "Unable to read properties from %s: %s", optarg, strerror(errno));
		goto error;
            }
	    config_read_properties_set = 1;
            break;
	case 'l':
	    config_receive_buffer_size = atoi(optarg);
	    config_receive_buffer_size_set = 1;
	    break;
	case 'R':
	    config_results_dir = strdup(optarg);
	    config_results_dir_set = 1;
	    break;
	case 's':
	    config_sample_data = 1;
	    config_sample_data_set = 1;
	    break;
	case 'o':
	    config_send_chunk_size = atoi(optarg);
	    config_send_chunk_size_set = 1;
	    break;
	case 'c':
	    config_send_continuous = 1;
	    config_send_continuous_set = 1;
	    break;
	case 'S':
	    config_statistics_log_rate = atoi(optarg);
	    config_statistics_log_rate_set = 1;
	    break;
	case 'M':
	    if (strcmp(optarg, "TCP") == 0) {
		config_transport = "TCP";
		config_neat_transport = "\"TCP\"";
	    } else if (strcmp(optarg, "SCTP") == 0) {
		config_transport = "SCTP";
		config_neat_transport = "\"SCTP\"";
	    } else if (strcmp(optarg, "BOTH") == 0) {
		config_transport = "BOTH";
		config_neat_transport = "\"SCTP\", \"TCP\"";
	    } else if (strcmp(optarg, "HE") == 0) {
		config_transport = "BOTH";
		config_neat_transport = "\"SCTP\", \"TCP\"";
	    } else if (strcmp(optarg, "TCP-SCTP") == 0) {
		config_transport = "BOTH";
		config_neat_transport = "\"TCP\", \"SCTP\"";
	    } else if (strcmp(optarg, "SCTP-TCP") == 0) {
		config_transport = "BOTH";
		config_neat_transport = "\"SCTP\", \"TCP\"";
	    } else if (strcmp(optarg, "SCTP/UDP") == 0) {
		config_transport = "?";
		config_neat_transport = "\"SCTP/UDP\"";
	    } else if (strcmp(optarg, "SCTP/UDP-TCP") == 0) {
		config_transport = "?";
		config_neat_transport = "\"SCTP/UDP\", \"TCP\"";
	    } else {
		user_log(LOG_LEVEL_ERROR, "Invalid transport: %s\n", optarg);
		goto error;
	    }
	    config_transport_set = 1;
	    config_neat_transport_set = 1;
	    neat_custom_properties = 1;
	    break;
	case 'u':
	    config_user_log_level = atoi(optarg);
	    config_user_log_level_set = 1;
	    break;
	default:
	    user_log(LOG_LEVEL_ERROR, "Invalid option!\n");
            goto error;
            break;
        }
    }

    if (optind + config_prefix_set != argc) {
	user_log(LOG_LEVEL_ERROR, "%s: Wrong number of arguments\n", __func__);
        goto error;
    }

    if (config_keep_alive && (config_run_mode != RUN_MODE_HTTP)) {
	user_log(LOG_LEVEL_ERROR, "%s: Cannot use Keep-Alive semantics when not running in HTTP mode.\n", __func__);
	goto error;
    }

    if (neat_custom_properties && config_read_properties_set) {
	user_log(LOG_LEVEL_ERROR, "%s: Cannot both read properties from file and use custom properties.\n", __func__);
	goto error;
    }

    return 0;
error:
    set_error_time(1);

    if (config_results_dir_set) {
	free(config_results_dir);
	config_results_dir_set = 0;
    }

    if (config_read_properties_set) {
	free(config_read_properties);
	config_read_properties_set = 0;
    }

    if (config_local_ip_set) {
	free(config_local_ip);
	config_local_ip_set = 0;
    }

    return -1;
}

void
free_resources(void)
{
    if (config_results_dir_set) {
	free(config_results_dir);
	config_results_dir_set = 0;
    }

    if (config_read_properties_set) {
	free(config_read_properties);
	config_read_properties_set = 0;
    }

    if (config_local_ip_set) {
	free(config_local_ip);
	config_local_ip_set = 0;
    }
}

void
print_overview(int is_neat)
{
    if (config_user_log_level >= LOG_LEVEL_INFO) {
	fprintf(stderr, "\n");
	fprintf(stderr, "Listening on %s:%d\n", config_local_ip_set ? config_local_ip : "ALL", config_port);
	
	if (config_run_mode == RUN_MODE_HTTP) {
	    fprintf(stderr, "Run mode: HTTP\n");
	    fprintf(stderr, "  HTTP file size: %d\n", config_http_file_size);
	    if (config_keep_alive) {
		fprintf(stderr, "  Keep-Alive: enabled\n");
	    } else {
		fprintf(stderr, "  Keep-Alive: disabled\n");
	    }
	} else if (config_run_mode == RUN_MODE_CONNECTION) {
	    fprintf(stderr, "Run mode: CONNECTION\n");
	} else {
	    fprintf(stderr, "Run mode: UNKNOWN\n");
	}

	if (config_sample_data) {
	    fprintf(stderr, "Sample data: enabled\n");
	    fprintf(stderr, "  Prefix used for results filenames: %s\n", config_prefix);
	    fprintf(stderr, "  Results path: %s\n", config_results_dir);
	} else {
	    fprintf(stderr, "Sample data: disabled\n");
	}

	if (config_expected_connecting_set) {
	    fprintf(stderr, "Expected connecting flows: %d\n", config_expected_connecting);
	}

	if (config_expected_completing_set) {
	    fprintf(stderr, "Expected completing flows: %d\n", config_expected_completing);
	}

	if (config_expected_tcp_set) {
	    fprintf(stderr, "Expected connecting TCP flows: %d\n", config_expected_tcp);
	}

	if (config_expected_sctp_set) {
	    fprintf(stderr, "Expected connecting SCTP flows: %d\n", config_expected_sctp);
	}

	if (config_expected_time_set) {
	    fprintf(stderr, "Expected number of seconds to run normally: %d\n", config_expected_time);
	}

	fprintf(stderr, "Receive buffer size: %d\n", config_receive_buffer_size);
	fprintf(stderr, "Send chunk size: %d\n", config_send_chunk_size);

	if (config_send_continuous_set) {
	    fprintf(stderr, "Send continuous: enabled\n");
	} else {
	    fprintf(stderr, "Send continuous: disabled\n");
	}

	if (config_multihoming_enabled) {
	    fprintf(stderr, "Multihoming: enabled\n");
	} else {
	    fprintf(stderr, "Multihoming: disabled\n");
	}

	fprintf(stderr, "Statistics log rate: %d\n", config_statistics_log_rate);
	fprintf(stderr, "User log level: %d\n", config_user_log_level);

	if (is_neat) {
	    fprintf(stderr, "NEAT log level: %d\n", config_neat_log_level);

	    if (config_read_properties_set) {
		fprintf(stderr, "Use properties read from file:\n%s\n\n", config_read_properties);
	    } else {
		fprintf(stderr, "Use the following properties:\n%s\n\n", config_properties);
	    }
	} else {
	    fprintf(stderr, "Listening for: %s\n", config_transport);
	}
    }
}
