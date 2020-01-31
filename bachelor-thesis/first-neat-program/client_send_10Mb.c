#include <neat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <time.h>

/*
Write a program with NEAT API that sends 10Mb of dummy data from a client to a server on
Linux in one scenario with "SCTP only property required” and another with “ TCP only
property required” and produce a plot that shows the CPU/memory overhead and compare the 
TCP vs SCTP overhead on the client-side using a graph.
*/

static uint32_t snd_chunk = 10000;       
static uint32_t snd_bytes = 10000000;    // 10 Mb of data
static uint16_t last_stream = 0;
static int first_time = 0;              // to sample memory overhead before write
//static int counter = 0;

// to calculate CPU time
static struct timespec time1, time2, temp_time, before_connected, after_connected,
connected_cpu_time, before_write, after_write, write_cpu_time;

static char *properties = "{\"transport\": {\"value\": \"SCTP\", \"precedence\": 1} }";
//static char *properties = "{\"transport\": {\"value\": \"TCP\", \"precedence\": 1} }"; 

void sample(long int my_pid, char* name);

static neat_error_code
on_readable(struct neat_flow_operations *opCB)
{
    uint32_t bytes_read = 0;
    unsigned char buffer[32];

    if (neat_read(opCB->ctx, opCB->flow, buffer, 31, &bytes_read, NULL, 0) == NEAT_OK) {
        buffer[bytes_read] = 0;
        fprintf(stdout, "Read %u bytes:\n%s", bytes_read, buffer);
    }

    return NEAT_OK;
}

static neat_error_code
on_close(struct neat_flow_operations *opCB) 
{
    neat_stop_event_loop(opCB->ctx);
    return NEAT_OK;
}

static neat_error_code
on_writable(struct neat_flow_operations *opCB)
{
    int32_t chunk_size;                    
    neat_error_code code;
    struct neat_tlv options[1];

    options[0].tag           = NEAT_TAG_STREAM_ID;
    options[0].type          = NEAT_TYPE_INTEGER;
    options[0].value.integer = last_stream;

    while(snd_bytes > 0)
    {
        if (snd_bytes<snd_chunk) {
          chunk_size = snd_bytes;    
        } else {
          chunk_size = snd_chunk;
        }

        unsigned char msg[chunk_size];
        memset(msg, 0x4e, chunk_size);

        /*Sampling in the middle of the write process doesn't work well as it slows it down
        and, for TCP, write more to the log file (is inconsistent as well).
        Sampling of RSS is therefore only done before and after write process*/
        if (first_time == 0)
        {
            long int my_pid = (long int)getpid();
            sample(my_pid, "before_write");
            first_time = 1;
        }

        /* 
        This can be used for SCTP, even though it will take more time, sampling will work
        fine and you will get more data.

        if (first_time == 0)
        {
            long int my_pid = (long int)getpid();
            sample(my_pid, "before_write");
            first_time = 1;
        } else {
            sample(my_pid, "while_writing");
        }
        counter ++;
        */

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &before_write);

        //code = neat_write(opCB->ctx, opCB->flow, msg, sizeof(msg), NULL, 0);     // TCP
        code = neat_write(opCB->ctx, opCB->flow, msg, sizeof(msg), options, 1);   // SCTP
        if (code != NEAT_OK) {
          fprintf(stderr, "%s - neat_write - error: %d\n", __func__, (int)code);
        }

        snd_bytes -= chunk_size;
    }
   
    return NEAT_OK;
}

static neat_error_code
on_all_written(struct neat_flow_operations *opCB)
{
    opCB->on_readable = on_readable;
    opCB->on_writable = NULL;
    neat_set_operations(opCB->ctx, opCB->flow, opCB);

    long int my_pid = (long int)getpid();
    sample(my_pid, "after_write");

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &after_write);

    return NEAT_OK;
}

static neat_error_code
on_connected(struct neat_flow_operations *opCB)
{
    long int my_pid = (long int)getpid();
    sample(my_pid, "after_connected");

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &after_connected);
    
    opCB->on_writable    = on_writable;
    opCB->on_all_written = on_all_written;
    neat_set_operations(opCB->ctx, opCB->flow, opCB);

    return NEAT_OK;
}

static void log_cpu_time()
{
    time_t t = time(NULL); 
    struct tm tm = *localtime(&t); 
    FILE * fp;

    fp = fopen("log_cpu_time.txt", "w");

    fprintf(fp, "Date: %d-%02d-%02d %02d:%02d:%02d\n\n", tm.tm_year + 1900, tm.tm_mon + 1,
     tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    fprintf(fp, "The whole process\n");
    fprintf(fp, "Start: %9ld ns\n", (long)time1.tv_nsec);
    fprintf(fp, "End: %9ld ns\n", (long)time2.tv_nsec);
    temp_time.tv_nsec = time2.tv_nsec - time1.tv_nsec;
    fprintf(fp, "CPU time exactly: %ld ns\n", (long)temp_time.tv_nsec);
    temp_time.tv_nsec *= 0.000001;
    fprintf(fp, "CPU time (rounded down): %ld ms\n\n", (long)temp_time.tv_nsec);

    fprintf(fp, "Writing process:\n");
    fprintf(fp, "Before write: %9ld ns\n", (long)before_write.tv_nsec);
    fprintf(fp, "After write:  %9ld ns\n", (long)after_write.tv_nsec);
    write_cpu_time.tv_nsec = after_write.tv_nsec - before_write.tv_nsec;
    fprintf(fp, "CPU time exactly: %ld n\n", (long)write_cpu_time.tv_nsec);
    write_cpu_time.tv_nsec *= 0.000001;
    fprintf(fp, "CPU time (rounded down): %ld ms\n\n", (long)write_cpu_time.tv_nsec);

    fprintf(fp, "on_connected():\n");
    fprintf(fp, "Before connected: %9ld ns\n", (long)before_connected.tv_nsec);
    fprintf(fp, "After connected:  %9ld ns\n", (long)after_connected.tv_nsec);
    connected_cpu_time.tv_nsec = after_connected.tv_nsec - before_connected.tv_nsec;
    fprintf(fp, "CPU time exactly: %ld ns\n", (long)connected_cpu_time.tv_nsec);
    connected_cpu_time.tv_nsec *= 0.000001;
    fprintf(fp, "CPU time (rounded down): %ld ms\n", (long)connected_cpu_time.tv_nsec);

    fclose(fp);
}

// for sampling of memory overhead 
void sample(long int my_pid, char* name)
{
    char command_string[1000];

    sprintf(command_string, "echo '%s' >> rss.log &", name);
    system(command_string);

    sprintf(command_string, "ps -o rss -p %ld >> rss.log &", my_pid);
    system(command_string);

    return;
}

int main(int argc, char const *argv[])
{
    struct neat_ctx *ctx;
    struct neat_flow *flow;
    struct neat_flow_operations opCB;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);

    ctx  = neat_init_ctx();
    if (!ctx) {
        fprintf(stderr, "neat_init_ctx failed\n");
        return EXIT_FAILURE;
    }

    flow = neat_new_flow(ctx);
    if (!flow) {
        fprintf(stderr, "neat_new_flow failed\n");
        return EXIT_FAILURE;
    }

    memset(&opCB, 0, sizeof(opCB));

    opCB.on_connected = on_connected;
    opCB.on_close = on_close;
    neat_set_operations(ctx, flow, &opCB);

    if (neat_set_property(ctx, flow, properties) != NEAT_OK) {
        fprintf(stderr, "neat_set_property failed\n");
        return EXIT_FAILURE;
    }

    long int my_pid = (long int)getpid();
    sample(my_pid, "before_connected");

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &before_connected);

    if (neat_open(ctx, flow, "127.0.0.1", 5000, NULL, 0)) {
        fprintf(stderr, "neat_open failed\n");
        return EXIT_FAILURE;
    }

    neat_start_event_loop(ctx, NEAT_RUN_DEFAULT);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);

    log_cpu_time();
    
    neat_free_ctx(ctx);

    return EXIT_SUCCESS;
}