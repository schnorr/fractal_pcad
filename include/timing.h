#include <sys/time.h>

double timespec_to_double(struct timespec t);
struct timespec timespec_diff(struct timespec start, struct timespec end);
struct timespec timespec_add(struct timespec t1, struct timespec t2);