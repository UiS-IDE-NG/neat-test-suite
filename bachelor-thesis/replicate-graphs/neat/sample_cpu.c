#include <stdlib.h>
#include <uv.h>
#include <string.h>

#include "neat.h"
#include "neat_log.h"

#include <common.h>
#include "sample_cpu.h"

void log_cpu_time(char * filename, struct timespec *start_s, struct timespec *end_s, struct timespec *diff)
{ 
    if (is_client)  // only sample for client at the moment --> can change this when one wants to sample for client (later; when sampling for client or server is determined)
    {
        FILE * fp;
        /*char dir[100];

        strcpy(dir, config_results_dir);

        strcat(dir, "/json_cpu_difference_");
        strcat(dir, filename);
        //strcat(dir, ".log");*/

        fp = fopen(filename, "a");     // dir
        
        diff->tv_nsec = end_s->tv_nsec - start_s->tv_nsec;
        fprintf(fp, "%ld\n", (long)diff->tv_nsec);

        fclose(fp);
    } 
}

// can also add a parameter for what directory
/*void sample_memory_usage(long int my_pid, char* name, struct neat_ctx *nc)
{
    char command_string[1000]; 
    sprintf(command_string, "ps -o rss -p %ld >> /home/helena/results-neat-test-suite/client/tcp/json_memory_%s.log &", my_pid, name);
    //sprintf(command_string, "ps -o rss -p %ld >> %s/json_memory_%s.log &", my_pid, config_results_dir, name);
    int system_return = system(command_string);

    if (system_return == -1)
    {
        nt_log(nc, NEAT_LOG_ERROR,  "%s - failed to sample memory", __func__);
    }

    return;
}*/