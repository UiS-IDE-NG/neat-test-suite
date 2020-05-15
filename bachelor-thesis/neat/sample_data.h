#include <stdlib.h>

#include "neat.h"
#include "neat_log.h"

extern int CPU_SAMPLING;
extern int MEMORY_SAMPLING;

void log_cpu_time(char * filename, struct timespec *start, struct timespec *end, struct timespec *diff);
void sample_memory_usage(char* name, struct neat_ctx *nc);