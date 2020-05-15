#include <stdlib.h>
#include <uv.h>
#include <string.h>
#include <unistd.h>

#include "neat.h"
#include "neat_log.h"

#include <common.h> // for is_client 
#include "sample_data.h"

int CPU_SAMPLING = 0;
int MEMORY_SAMPLING = 0;

void log_cpu_time(char * filename, struct timespec *start_s, struct timespec *end_s, struct timespec *diff) 
{    
    if (is_client) {
        FILE * fp;

        fp = fopen(filename, "a");
        diff->tv_nsec = end_s->tv_nsec - start_s->tv_nsec;
        fprintf(fp, "%ld\n", (long)diff->tv_nsec);

        fclose(fp);
    }
}

void sample_memory_usage(char* name, struct neat_ctx *nc) 
{
    char command_string[1000]; 
    long int my_pid = (long int)getpid();

    if (is_client) {
        sprintf(command_string, "ps -o rss -p %ld >> /home/helena/results-neat-test-suite/client/tcp/json_memory_%s.log &", my_pid, name);
        int system_return = system(command_string);

        if (system_return == -1) {
            nt_log(nc, NEAT_LOG_ERROR,  "%s - failed to sample memory", __func__);
        }
        return;
    }
}