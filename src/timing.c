#include <sys/time.h>
#include "timing.h"

#define BILLION 1000000000L

double timespec_to_double(struct timespec t) {
  return (double)t.tv_sec + (double)t.tv_nsec / (double)BILLION;
}

struct timespec timespec_diff(struct timespec start, struct timespec end) {
  struct timespec diff;
  if ((end.tv_nsec - start.tv_nsec) < 0) {
    diff.tv_sec = end.tv_sec - start.tv_sec - 1;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec + BILLION;
  } else {
    diff.tv_sec = end.tv_sec - start.tv_sec;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return diff;
}

struct timespec timespec_add(struct timespec t1, struct timespec t2) {
  struct timespec result;
  result.tv_sec = t1.tv_sec + t2.tv_sec;
  result.tv_nsec = t1.tv_nsec + t2.tv_nsec;
  if (result.tv_nsec >= BILLION) {
    result.tv_sec++;
    result.tv_nsec -= BILLION;
  }
  return result;
}
