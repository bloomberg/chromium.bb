/*
 * Copyright 2009  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

/*
 * Newlib's time.h not working right: getting the nanosleep
 * declaration from the newlib time.h file requires defining
 * _POSIX_TIMERS, but we don't implement any of the clock_settime,
 * clock_gettime, clock_getres, timer_create, timer_delete,
 * timer_settime, timer_gettime, and timer_getoverrun functions.  In
 * glibc's time.h, these function declarations are obtained by
 * ensuring _POSIX_C_SOURCE >= 199309L.
 *
 * Sigh.
 */
#include <sys/nacl_syscalls.h>

/*
 * Returns failure count.  t_suspend should not be shorter than 1us,
 * since elapsed time measurement cannot possibly be any finer in
 * granularity.  In practice, 1ms is probably the best we can hope for
 * in timer resolution, so even if nanosleep suspends for 1us, the
 * gettimeofday resolution may cause a false failure report.
 */
int TestNanoSleep(struct timespec *t_suspend) {
  struct timespec t_remain;
  struct timeval  t_start;
  int             rv;
  struct timeval  t_end;
  struct timeval  t_elapsed;

  printf("%40s: %ld.%09ld seconds\n",
         "Requesting nanosleep duration",
         t_suspend->tv_sec,
         t_suspend->tv_nsec);
  t_remain = *t_suspend;
  /*
   * BUG: ntp or other time adjustments can mess up timing.
   * BUG: time-of-day clock resolution may be not be fine enough to
   * measure nanosleep duration.
   */
  if (-1 == gettimeofday(&t_start, NULL)) {
    printf("gettimeofday for start time failed\n");
    return 1;
  }
  while (-1 == (rv = nanosleep(&t_remain, &t_remain)) &&
         EINTR == errno) {
  }
  if (-1 == rv) {
    printf("nanosleep failed, errno = %d\n", errno);
    return 1;
  }
  if (-1 == gettimeofday(&t_end, NULL)) {
    printf("gettimeofday for end time failed\n");
    return 1;
  }

  t_elapsed.tv_sec = t_end.tv_sec - t_start.tv_sec;
  t_elapsed.tv_usec = t_end.tv_usec - t_start.tv_usec;
  if (t_elapsed.tv_usec < 0) {
    t_elapsed.tv_usec += 1000000;
    t_elapsed.tv_sec -= 1;
  }

  printf("%40s: %ld.%06ld seconds\n",
         "Actual nanosleep duration",
         t_elapsed.tv_sec,
         t_elapsed.tv_usec);
  if (t_elapsed.tv_sec < t_suspend->tv_sec ||
      (t_elapsed.tv_sec == t_suspend->tv_sec &&
       (1000 * t_elapsed.tv_usec < t_suspend->tv_nsec))) {
    printf("Elapsed time too short!\n");
    printf("Error\n");
    return 1;
  } else {
    printf("OK\n");
    return 0;
  }
}

#define NANOS_PER_MICRO   (1000)
#define MICROS_PER_MILLI  (1000)
#define NANOS_PER_MILLI   (NANOS_PER_MICRO * MICROS_PER_MILLI)

int main(void) {
  int                     num_errors = 0;
  int                     ix;

  static struct timespec  t_suspend[] = {
    { 0,   1 * NANOS_PER_MILLI, },
    { 0,   2 * NANOS_PER_MILLI, },
    { 0,   5 * NANOS_PER_MILLI, },
    { 0,  10 * NANOS_PER_MILLI, },
    { 0,  25 * NANOS_PER_MILLI, },
    { 0,  50 * NANOS_PER_MILLI, },
    { 0, 100 * NANOS_PER_MILLI, },
    { 0, 250 * NANOS_PER_MILLI, },
    { 0, 500 * NANOS_PER_MILLI, },
    { 1,   0 * NANOS_PER_MILLI, },
    { 1, 500 * NANOS_PER_MILLI, },
  };

  for (ix = 0; ix < sizeof t_suspend/sizeof t_suspend[0]; ++ix) {
    num_errors += TestNanoSleep(&t_suspend[ix]);
  }

  if (0 != num_errors) {
    printf("FAILED\n");
    return 1;
  } else {
    printf("PASSED\n");
    return 0;
  }
}
