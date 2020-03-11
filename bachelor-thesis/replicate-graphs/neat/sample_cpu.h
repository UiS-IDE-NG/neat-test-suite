#include <stdlib.h>

#include "neat.h"
#include "neat_log.h"

void log_cpu_time(char * filename, struct timespec *start, struct timespec *end, struct timespec *diff);
//void sample_memory_usage(long int my_pid, char* name, struct neat_ctx *nc);
