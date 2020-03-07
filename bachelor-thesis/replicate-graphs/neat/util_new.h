#ifndef UTIL_INCLUDE_H
#define UTIL_INCLUDE_H

#include <inttypes.h>
#include <sys/time.h>

int64_t read_file(const char *filename, const char **bufptr);
int sample_usage_data(char *prefix, char *where, char *results_dir);
void set_error_time(int fatal);
void sample(char *name, int condition);
void sample_time(char *where, int condition);
int get_time(struct timespec *ts);
void subtract_time(struct timespec *start, struct timespec *end, struct timespec *diff);
void sanity_check(int log_level);
void user_log(int log_level, const char *format, ...);

#endif /* ifndef UTIL_INCLUDE_H */
