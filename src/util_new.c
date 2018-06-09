#include "common.h"
#include "util_new.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

static clockid_t get_clock_id(pid_t pid);
static int sample_cpu_time(clockid_t clock_id, char *prefix, char *where, char *name, char *results_dir);

int64_t
read_file(const char *filename, const char **bufptr)
{
    int rc;
    struct stat stat_buffer;
    char *buffer = NULL;
    FILE *f = NULL;
    int64_t offset = 0;

    if ((rc = stat(filename, &stat_buffer)) < 0) {
        goto error;
    }

    assert(stat_buffer.st_size >= 0);
    buffer = (char*)malloc(stat_buffer.st_size);
    
    if (!buffer) {
        rc = -ENOMEM;
        goto error;
    }

    f = fopen(filename, "r");
    
    if (!f) {
        rc = -EIO;
        goto error;
    }

    do {
        int64_t bytes_read = fread(buffer + offset, 1, stat_buffer.st_size - offset, f);
        if (bytes_read < stat_buffer.st_size - offset && ferror(f))
            goto error;
        offset += bytes_read;
    } while (offset < (int64_t)stat_buffer.st_size);

    fclose(f);
    *bufptr = buffer;
    return (int64_t)stat_buffer.st_size;
error:
    if (buffer) {
        free(buffer);
    }
    
    if (f) {
        fclose(f);
    }
    
    *bufptr = NULL;
    return rc;
}


static clockid_t
get_clock_id(pid_t pid)
{
    clockid_t clock_id;

    if (clock_getcpuclockid(pid, &clock_id) != 0) {
        fprintf(stderr, "clock_getcpuclockid() failed!\n");
	return (clockid_t)-1;
    }

    return clock_id;
}

static int
sample_cpu_time(clockid_t clock_id, char *prefix, char *where, char *name, char *results_dir)
{
    struct timespec ts;
    int result = 0;
    char results_filename[500];
    FILE *results_file;

    snprintf(results_filename, sizeof (results_filename), "%s/%s_%s_%s.log", results_dir, prefix, where, name);
    if ((results_file = fopen(results_filename, "a")) == NULL) {
	perror("fopen");
	return -1;
    }

    if (clock_id == (clockid_t)-1) {
	fprintf(stderr, "Invalid clock id\n");
	result = -1;
    } else {
	if (clock_gettime(clock_id, &ts) == -1) {
	    perror("clock_gettime");
	    result = -1;
	} else {
	    fprintf(results_file, "%ld %ld\n", ts.tv_sec, ts.tv_nsec);
	}
    }

    if (results_file) {
	fclose(results_file);
    }
    
    return result;
}

int
sample_usage_data(char *prefix, char *where, char *results_dir)
{
#if defined(__linux__)
    fprintf(stderr, "Sampling not implemented for Linux yet...\n");
    fprintf(stderr, "prefix: %s\n", prefix);
    fprintf(stderr, "where: %s\n", where);
#endif
#if defined(__FreeBSD__)
    int error = 0;
    long int my_pid = (long int)getpid();
    char command_string[1000];

    /*
     * Record current time.
     */
    error = error | sample_cpu_time(CLOCK_MONOTONIC_PRECISE, prefix, where, "time_application", results_dir);
    
    /*
     * Record current idle CPU time.
     */
    error = error | sample_cpu_time(get_clock_id(11), prefix, where, "cpu_idle", results_dir);

    /*
     * Record current rand_harvestq CPU time.
     */
    error = error | sample_cpu_time(get_clock_id(6), prefix, where, "cpu_randharvestq", results_dir);

    /*
     * Record current application CPU time.
     */
    error = error | sample_cpu_time(CLOCK_PROCESS_CPUTIME_ID, prefix, where, "cpu_application", results_dir);

    /*
     * Record current application RSS memory usage.
     */
    snprintf(command_string, sizeof (command_string), "ps -o rss -p %ld >> %s/%s_%s_memory_application.log &", my_pid, results_dir, prefix, where);
    system(command_string);

    /* this command gives wrong results */
    // error = error | sample_cpu_time(12, prefix, where, "cpu_intr", results_dir);

    /*
     * Record current intr CPU time.
     */
    snprintf(command_string, sizeof (command_string), "ps -o vsz,rss,%%mem,%%cpu,time -p 12 >> %s/%s_%s_cpu_intr.log &", results_dir, prefix, where);
    system(command_string);

    /*
     * Record current global CPU ticks.
     */
    snprintf(command_string, sizeof (command_string), "sysctl kern.cp_time >> %s/%s_%s_cpu_globalticks.log &", results_dir, prefix, where);
    system(command_string);

    /* Get information about individual system threads for intr process in top */
    snprintf(command_string, sizeof (command_string), "top -b -HS | grep intr >> %s/%s_%s_overview_intrtop.log &", results_dir, prefix, where);
    system(command_string);

    /* Get information about individual sytem threads for kernel process in top */
    snprintf(command_string, sizeof (command_string), "top -b -HS | grep kernel >> %s/%s_%s_overview_kerneltop.log &", results_dir, prefix, where);
    system(command_string);

    /*
     * Record current information about interrupts, e.g. number of interrupts.
     */
    snprintf(command_string, sizeof (command_string), "vmstat -i >> %s/%s_%s_overview_intrcount.log &", results_dir, prefix, where);
    system(command_string);

    /*
     * Record current system process CPU time.
     */
    snprintf(command_string, sizeof (command_string), "procstat -r 0 >> %s/%s_%s_cpu_kernel.log &", results_dir, prefix, where);
    system(command_string);

    /*
     * Record current memory usage of zone memory allocator.
     */
    snprintf(command_string, sizeof (command_string), "vmstat -z >> %s/%s_%s_memory_vmstatz.log &", results_dir, prefix, where);
    system(command_string);

    /*
     * Record current memory usage of kernel malloced memory.
     */
    snprintf(command_string, sizeof (command_string), "vmstat -m >> %s/%s_%s_memory_vmstatm.log &", results_dir, prefix, where);
    system(command_string);

    /*
     * Record current memory usage of mbufs allocated in kernel.
     */
    snprintf(command_string, sizeof (command_string), "netstat -m >> %s/%s_%s_memory_netstatm.log &", results_dir, prefix, where);
    system(command_string);

    return error;
#endif
}

void
set_error_time(int fatal)
{
    if (error_time == NULL) {
	if ((error_time = calloc(1, sizeof (*error_time))) == NULL) {
	    error = 1;
	    return;
	}
	if (gettimeofday(error_time, NULL) == -1) {
	    fprintf(stderr, "Failed to get current time!\n");
	    error = 1;
	}
	elapsed_error_time = error_time->tv_sec - start.tv_sec;
	elapsed_error_time += (double)(error_time->tv_usec - start.tv_usec) / 1000000;
    }
    if (fatal) {
	error = 1;
    }
}

void
sample(char *name, int condition)
{
    if (config_sample_data) {
	if (condition) {
	    if (sample_usage_data(config_prefix, name, config_results_dir) == -1) {
		set_error_time(1);
	    }
	}
    }
    return;
}

void
sample_time(char *where, int condition)
{
#if defined(__linux__)
    fprintf(stderr, "Attempted to sample data, but sampling is not supported in Linux yet.\n");
#endif
#if defined(__FreeBSD__)
    if (config_sample_data) {
	if (condition) {
	    if (sample_cpu_time(CLOCK_MONOTONIC_PRECISE, config_prefix, where, "time_application", config_results_dir) == -1) {
		set_error_time(1);
	    }
	}
    }
#endif
}

int
get_time(struct timespec *ts)
{
#if defined(__linux__)
    fprintf(stderr, "Attempted to sample data, but sampling is not supported in Linux yet.\n");
    return 1;
#endif
#if defined(__FreeBSD__)
    if (ts == NULL) {
	fprintf(stderr, "get_time - Got NULL pointer!\n");
	set_error_time(1);
	return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC_PRECISE, ts) == -1) {
	perror("clock_gettime");
	set_error_time(1);
	return -1;
    }

    return 1;
#endif
}

void
subtract_time(struct timespec *start, struct timespec *end, struct timespec *diff)
{
    if ((end->tv_nsec - start->tv_nsec) < 0) {
        diff->tv_sec = end->tv_sec - start->tv_sec - 1;
        diff->tv_nsec = end->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        diff->tv_sec = end->tv_sec - start->tv_sec;
        diff->tv_nsec = end->tv_nsec - start->tv_nsec;
    }
}

void
sanity_check(int log_level)
{
    if (error_time == NULL) {
	if (log_level >= 1) {
	    fprintf(stderr, "ERROR: The error time was never set!\n");
	}
	error = 1;
    } else {
	if (log_level >= 1) {
	    if (gettimeofday(&end, NULL) == -1) {
		fprintf(stderr, "Failed to get current time!\n");
		error = 1;
	    }
	    elapsed_time = end.tv_sec - start.tv_sec;
	    elapsed_time += (double)(end.tv_usec - start.tv_usec) / 1000000;
	    fprintf(stderr, "Elapsed time: %f s\n", elapsed_time);
	    fprintf(stderr, "Normal runtime: %f s\n", elapsed_error_time);
	    fprintf(stderr, "Average send throughput: %f Mbit/s\n", (((double)total_bytes_sent * 8) / 1000000) / elapsed_time);
	    fprintf(stderr, "Average read throughput: %f Mbit/s\n", (((double)total_bytes_read * 8) / 1000000) / elapsed_time);
	}
	if ((config_expected_time > 0) && (config_expected_time > elapsed_error_time)) {
	    if (log_level >= 1) {
		fprintf(stderr, "Expected to run normally for %d s. Stopped at %f s.\n", config_expected_time, elapsed_error_time);
	    }
	    error = 1;
	}
	free(error_time);
    }

    if (log_level >= 1) {
	fprintf(stderr, "Connected: %d\n", clients_connected);
    }
    if ((config_expected_connecting > 0) && (config_expected_connecting != clients_connected)) {
	if (log_level >= 1) {
	    fprintf(stderr, "Expected %d connections to succeed - %d succeeded\n", config_expected_connecting, clients_connected);
	}
	error = 1;
    }

    if ((config_expected_completing > 0) && (config_expected_completing != clients_completed)) {
	if (log_level >= 1) {
	    fprintf(stderr, "Expected %d clients to complete - %d completed\n", config_expected_completing, clients_completed);
	}
	error = 1;
    }

    if ((config_expected_tcp > 0) && (config_expected_tcp != tcp_connected)) {
	if (log_level >= 1) {
	    fprintf(stderr, "Expected %d TCP connections - Got %d\n", config_expected_tcp, tcp_connected);
	}
	error = 1;
    }

    if ((config_expected_sctp > 0) && (config_expected_sctp != sctp_connected)) {
	if (log_level >= 1) {
	    fprintf(stderr, "Expected %d SCTP connections - Got %d\n", config_expected_sctp, sctp_connected);
	}
	error = 1;
    }

    if (error) {
	if (log_level >= 1) {
	    fprintf(stderr, "An error occured during execution!\n");
	}
	if (config_sample_data) {
	    char command_string[700];

	    snprintf(command_string, sizeof (command_string), "mkdir -p %s", config_results_dir);
	    system(command_string);
	    snprintf(command_string, sizeof (command_string), "touch %s/error.log", config_results_dir);
	    system(command_string);
	    snprintf(command_string, sizeof (command_string), "echo \"%s, ExpConn: %d, Conn: %d, ExpComp: %d, Comp: %d, ExpTCP: %d, TCP: %d, ExpSCTP: %d, SCTP: %d, Elapsed: %f, Expected: %d\" >> %s/error.log", config_prefix, config_expected_connecting, clients_connected, config_expected_completing, clients_completed, config_expected_tcp, tcp_connected, config_expected_sctp, sctp_connected, elapsed_error_time, config_expected_time, config_results_dir);
	    system(command_string);
	}
    }
}

void
user_log(int log_level, const char *format, ...)
{
    va_list argptr;

    if (config_user_log_level < log_level) {
	return;
    }

    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}
