#include <stdlib.h>
#include <uv.h>
#include <jansson.h>

#include <unistd.h>
#include <sample_cpu.h>
#include <common.h>
#include "json_overhead.h"

#include "neat.h"
#include "neat_internal.h"
#include "neat_unix_json_socket.h"
#include "neat_pm_socket.h"

//static struct timespec start_t, end_t, diff_t;

static void
on_pm_written(struct neat_ctx *ctx, struct neat_flow *flow, struct neat_ipc_context *context)
{
    struct neat_pm_context *pm_context = context->data;

    nt_log(ctx, NEAT_LOG_DEBUG, "%s", __func__);

    if (nt_unix_json_start_read(context) ||
        nt_unix_json_shutdown(context)) {

        nt_log(ctx, NEAT_LOG_DEBUG, "Failed to initiate read/shutdown for PM socket");

        pm_context->on_pm_error(ctx, flow, PM_ERROR_SOCKET);
    }
}

static void
on_pm_written_no_reply(struct neat_ctx *ctx, struct neat_flow *flow, struct neat_ipc_context *context)
{
    struct neat_pm_context *pm_context = context->data;

    nt_log(ctx, NEAT_LOG_DEBUG, "%s", __func__);

    if (nt_unix_json_shutdown(context)) {

        nt_log(ctx, NEAT_LOG_DEBUG, "Failed to initiate shutdown for PM socket");

        pm_context->on_pm_error(ctx, flow, PM_ERROR_SOCKET);
    }
}

static void
on_timer_close(uv_handle_t* handle)
{
    free(handle);
}

static void
on_pm_close(void* data)
{
    struct neat_pm_context *pm_context = data;

    //nt_log(NEAT_LOG_DEBUG, "%s", __func__);

    free(pm_context->output_buffer);
    free(pm_context->ipc_context);

    if (!uv_is_closing((uv_handle_t*)pm_context->timer)) {
        uv_close((uv_handle_t*)pm_context->timer, on_timer_close);
    }

    free(pm_context);
}

static void
on_pm_timeout(uv_timer_t* timer)
{
    struct neat_pm_context *pm_context = timer->data;

    //nt_log(NEAT_LOG_DEBUG, "%s", __func__);

    pm_context->on_pm_error(pm_context->ipc_context->ctx, pm_context->ipc_context->flow, PM_ERROR_SOCKET);

    nt_unix_json_close(pm_context->ipc_context, on_pm_close, pm_context);
}

static void
on_pm_read(struct neat_ctx *ctx, struct neat_flow *flow, json_t *json, void *data)
{
    struct neat_pm_context *pm_context = data;

    nt_log(ctx, NEAT_LOG_DEBUG, "%s", __func__);

    if (pm_context->on_pm_reply != NULL) {
        pm_context->on_pm_reply(ctx, flow, json);
    }

    nt_unix_json_close(pm_context->ipc_context, on_pm_close, data);
}

static void
on_pm_error(struct neat_ctx *ctx, struct neat_flow *flow, int error, void *data)
{
    struct neat_pm_context *pm_context = data;

    nt_log(ctx, NEAT_LOG_DEBUG, "%s", __func__);

    pm_context->on_pm_error(ctx, flow, error);

    nt_unix_json_close(pm_context->ipc_context, on_pm_close, data);
}

static void
on_pm_connected(struct neat_ipc_context *context, void *data)
{
    struct neat_pm_context *pm_context = data;

    //nt_log(NEAT_LOG_DEBUG, "%s", __func__);

    if ((nt_unix_json_send(context, pm_context->output_buffer, on_pm_written, context->on_error)) != NEAT_ERROR_OK) {
        pm_context->on_pm_error(pm_context->ipc_context->ctx, pm_context->ipc_context->flow, PM_ERROR_SOCKET);
    }
}

static void
on_pm_connected_no_reply(struct neat_ipc_context *context, void *data)
{
    struct neat_pm_context *pm_context = data;

    //nt_log(NEAT_LOG_DEBUG, "%s", __func__);

    if ((nt_unix_json_send(context, pm_context->output_buffer, on_pm_written_no_reply, context->on_error)) != NEAT_ERROR_OK) {
        pm_context->on_pm_error(pm_context->ipc_context->ctx, pm_context->ipc_context->flow, PM_ERROR_SOCKET);
    }
}

neat_error_code
nt_json_send_once(struct neat_ctx *ctx, struct neat_flow *flow, const char *path, json_t *json, pm_reply_callback cb, pm_error_callback err_cb)
{
    int rc;
    struct neat_ipc_context *context;
    struct neat_pm_context *pm_context;

    nt_log(ctx, NEAT_LOG_DEBUG, "%s", __func__);

    if ((context = calloc(1, sizeof(*context))) == NULL)
        return NEAT_ERROR_OUT_OF_MEMORY;

    if ((pm_context = calloc(1, sizeof(*pm_context))) == NULL) {
        rc = NEAT_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    pm_context->timer = NULL;

    //long int my_pid = (long int)getpid();
    //sample_memory_usage(my_pid, "jsondumps_before", ctx);
    //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_t);
    /*if (first5) {
        if ((pm_context->output_buffer = json_dumps(json, JSON_INDENT(2))) == NULL) {
            rc = NEAT_ERROR_OUT_OF_MEMORY;
            goto error;
        }
        strcpy(send_prop_array2[index_to_replace3].output_buffer_before, pm_context->output_buffer);
        free(pm_context->output_buffer);
        pm_context->output_buffer = send_prop_array2[index_to_replace3].output_buffer_before;
        first5 = false;
    } else {
        if (same_value_jsondumps2) {
            pm_context->output_buffer = send_prop_array2[index_to_replace3].output_buffer_before;   // should be index_of_same_value!
        } else {
            if ((pm_context->output_buffer = json_dumps(json, JSON_INDENT(2))) == NULL) {
                rc = NEAT_ERROR_OUT_OF_MEMORY;
                goto error;
            }
            strcpy(send_prop_array2[index_to_replace3].output_buffer_before, pm_context->output_buffer);
            free(pm_context->output_buffer);
            pm_context->output_buffer = send_prop_array2[index_to_replace3].output_buffer_before;    
        }
    }*/
    if ((pm_context->output_buffer = json_dumps(json, JSON_INDENT(2))) == NULL) {
        rc = NEAT_ERROR_OUT_OF_MEMORY;
        goto error;
    }
    //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_t);
    //long int my_pid2 = (long int)getpid();
    //sample_memory_usage(my_pid2, "jsondumps_after", ctx);
    //log_cpu_time("/home/helena/results-neat-test-suite/client/tcp/json_cpu_difference_jsondumps.log", &start_t, &end_t, &diff_t);                
    /*strcpy(output_buffer_before2, pm_context->output_buffer);   // temperarly added so that free(pm_context->output_buffer); can be removed in pm_error so 
    free(pm_context->output_buffer);                            // that the other json_dumps doesn't cause an error
    pm_context->output_buffer = output_buffer_before2;*/

    if ((pm_context->timer = calloc(1, sizeof(*pm_context->timer))) == NULL) {
        rc = NEAT_ERROR_OUT_OF_MEMORY;
        goto error;
    }
   
    if ((rc = uv_timer_init(ctx->loop, pm_context->timer))) {
        nt_log(ctx, NEAT_LOG_DEBUG, "uv_timer_init error: %s", uv_strerror(rc));
        rc = NEAT_ERROR_INTERNAL;
        goto error;
    }

    if ((rc = uv_timer_start(pm_context->timer, on_pm_timeout, 3000, 0))) {
        nt_log(ctx, NEAT_LOG_DEBUG, "uv_timer_start error: %s", uv_strerror(rc));
        rc = NEAT_ERROR_INTERNAL;
        goto error;
    }

    pm_context->timer->data = pm_context;
    pm_context->on_pm_reply = cb;
    pm_context->on_pm_error = err_cb;
    pm_context->ipc_context = context;

    if ((rc = nt_unix_json_socket_open(ctx, flow, context, path, on_pm_connected, on_pm_read, on_pm_error, pm_context)) == NEAT_OK)
        return NEAT_OK;
error:
    if (pm_context) {
        if (pm_context->output_buffer)
            free(pm_context->output_buffer);
        if (pm_context->timer)
            free(pm_context->timer);
        free(pm_context);
    }
    if (context)
        free(context);
    return rc;
}

neat_error_code
nt_json_send_once_no_reply(struct neat_ctx *ctx, struct neat_flow *flow, const char *path, json_t *json, pm_reply_callback cb, pm_error_callback err_cb)
{
    int rc;
    struct neat_ipc_context *context;
    struct neat_pm_context *pm_context;

    nt_log(ctx, NEAT_LOG_DEBUG, "%s", __func__);

    if ((context = calloc(1, sizeof(*context))) == NULL)
        return NEAT_ERROR_OUT_OF_MEMORY;

    if ((pm_context = calloc(1, sizeof(*pm_context))) == NULL) {
        rc = NEAT_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    pm_context->timer = NULL;

    //long int my_pid = (long int)getpid();
    //sample_memory_usage(my_pid, "jsondumps_before", ctx);
    //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_t);
    /*if (first2) {
        if ((pm_context->output_buffer = json_dumps(json, JSON_INDENT(2))) == NULL) {
            rc = NEAT_ERROR_OUT_OF_MEMORY;
            goto error;
        }
        strcpy(output_buffer_before, pm_context->output_buffer);
        free(pm_context->output_buffer);
        pm_context->output_buffer = output_buffer_before;
        first2 = false;
    } else {
        if (same_value_jsondumps) {
            pm_context->output_buffer = output_buffer_before;
            same_value_jsondumps = false;
        } else {
            if ((pm_context->output_buffer = json_dumps(json, JSON_INDENT(2))) == NULL) {
                rc = NEAT_ERROR_OUT_OF_MEMORY;
                goto error;
            }
            strcpy(output_buffer_before, pm_context->output_buffer);
            free(pm_context->output_buffer);
            pm_context->output_buffer = output_buffer_before;
        }
    }*/
    if ((pm_context->output_buffer = json_dumps(json, JSON_INDENT(2))) == NULL) {
        rc = NEAT_ERROR_OUT_OF_MEMORY;
        goto error;
    }
    //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_t);
    //long int my_pid2 = (long int)getpid();
    //sample_memory_usage(my_pid2, "jsondumps_after", ctx);
    //log_cpu_time("/home/helena/results-neat-test-suite/client/tcp/json_cpu_difference_jsondumps.log", &start_t, &end_t, &diff_t);                

    if ((pm_context->timer = calloc(1, sizeof(*pm_context->timer))) == NULL) {
        rc = NEAT_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    if ((rc = uv_timer_init(ctx->loop, pm_context->timer))) {
        nt_log(ctx, NEAT_LOG_DEBUG, "uv_timer_init error: %s", uv_strerror(rc));
        rc = NEAT_ERROR_INTERNAL;
        goto error;
    }

    if ((rc = uv_timer_start(pm_context->timer, on_pm_timeout, 3000, 0))) {
        nt_log(ctx, NEAT_LOG_DEBUG, "uv_timer_start error: %s", uv_strerror(rc));
        rc = NEAT_ERROR_INTERNAL;
        goto error;
    }

    pm_context->timer->data = pm_context;
    pm_context->on_pm_reply = cb;
    pm_context->on_pm_error = err_cb;
    pm_context->ipc_context = context;

    if ((rc = nt_unix_json_socket_open(ctx, flow, context, path, on_pm_connected_no_reply, on_pm_read, on_pm_error, pm_context)) == NEAT_OK)
        return NEAT_OK;
error:
    if (pm_context) {
        if (pm_context->output_buffer)
            free(pm_context->output_buffer);
        if (pm_context->timer)
            free(pm_context->timer);
        free(pm_context);
    }
    if (context)
        free(context);
    return rc;
}
