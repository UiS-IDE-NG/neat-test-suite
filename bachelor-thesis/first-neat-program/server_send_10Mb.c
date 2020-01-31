#include <neat.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

static uint32_t buffer_size = 100000;
static uint16_t log_level = 2;

static unsigned char *buffer = NULL;
static uint32_t buffer_filled = 0;
static uint32_t buffer_received = 0;

static neat_error_code on_writable(struct neat_flow_operations *ops);
static neat_error_code on_all_written(struct neat_flow_operations *ops);

static neat_error_code
on_readable(struct neat_flow_operations *ops)
{
    neat_error_code code;

    if (log_level >= 2) {
        fprintf(stderr, "%s()\n", __func__);
    }

    code = neat_read(ops->ctx, ops->flow, buffer, buffer_size, &buffer_filled, NULL, 0);
    if (code != NEAT_OK) {
        if (code == NEAT_ERROR_WOULD_BLOCK) {
            if (log_level >= 1) {
                printf("on_readable - NEAT_ERROR_WOULD_BLOCK\n");
            }
            return NEAT_OK;
        } else {
            fprintf(stderr, "%s - neat_read error: %d\n", __func__, (int)code);
        }
    }

    // add the data received to a file
    time_t t = time(NULL); 
    struct tm tm = *localtime(&t); 
    FILE * fp;

    fp = fopen("log_data_received.txt", "w");

    fprintf(fp, "Date: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1,
     tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    for (int i = buffer_filled; i < 10000000; i+=buffer_filled)
    {
        fprintf(fp, "%s\n", buffer); 
        fflush(fp);
    }

    if (log_level >= 1) {
        printf("received data - %d byte\n", buffer_filled);
    }
    
    buffer_received += buffer_filled;
    printf("Total received data: %d\n", buffer_received);

    if (buffer_received >= 10000000)
    {
        fprintf(fp, "received data - %d byte\n", buffer_received);
        fclose(fp);
    }

    return NEAT_OK;
}

static neat_error_code
on_connected(struct neat_flow_operations *ops)
{
    if (log_level >= 2) {
        fprintf(stderr, "%s()\n", __func__);
    }

    if (log_level >= 1) {
        printf("peer connected\n");
    }

    ops->on_readable = on_readable;
    neat_set_operations(ops->ctx, ops->flow, ops);

    return NEAT_OK;
}

int
main(int argc, char *argv[])
{
    struct neat_ctx *ctx;
    struct neat_flow *flow;
    struct neat_flow_operations ops;

    if ((buffer = malloc(buffer_size)) == NULL) {
        fprintf(stderr, "%s - malloc failed\n", __func__);
    }

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
    
    memset(&ops, 0, sizeof(ops));

    ops.on_connected = on_connected;
    neat_set_operations(ctx, flow, &ops);

    if (neat_accept(ctx, flow, 5000, NULL, 0)) {
        fprintf(stderr, "neat_accept failed\n");
        return EXIT_FAILURE;
    }

    neat_start_event_loop(ctx, NEAT_RUN_DEFAULT);

    return EXIT_SUCCESS;
}
