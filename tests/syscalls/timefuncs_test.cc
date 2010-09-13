/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 *
 * These are the syscalls I am testing:
 *   #define NACL_sys_gettimeofday           40
 *   #define NACL_sys_clock                  41
 *   #define NACL_sys_nanosleep              42
*/


#include <time.h>       // For nanosleep.
#include <sys/time.h>   // For gettimeofday.
#include <sys/times.h>  // For clock which uses times

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "native_client/src/trusted/service_runtime/include/sys/nacl_syscalls.h"
#include "native_client/tests/syscalls/test.h"

/*
 * These four definitions are copied from nanosleep_test.c
 */
#define NANOS_PER_MICRO   (1000)
#define MICROS_PER_MILLI  (1000)
#define NANOS_PER_MILLI   (NANOS_PER_MICRO * MICROS_PER_MILLI)
#define MICROS_PER_UNIT   (1000 * 1000)



int TestClockFunction() {
  START_TEST("test clock function");

  clock_t clock_time = 1;
  /* NOTE: Re-enable call to clock after linking issue with clock is resolved
   * clock_time = clock();
   */
  EXPECT(clock_time > 0);

  END_TEST();
}

int TestTimeFuncs() {
  START_TEST("test gettimeofday,nanosleep function");
  struct timeval tv1;   // Used by gettimeofday

  /* Passing NULL as the first argument causes a warning with glibc. */
#ifndef __GLIBC__
  EXPECT(0 != gettimeofday(NULL, NULL));
#endif

  /*
   * gettimeofday takes two args: timeval and timezone pointers.
   * The use of the timezone structure is obsolete; the tz argument should
   * normally be specified as  NULL.
   */
  EXPECT(0 == gettimeofday(&tv1, NULL));

  struct timespec ts;  // Used by nanosleep.

  ts.tv_sec = 1;
  ts.tv_nsec = 5000000;
  EXPECT(0 == nanosleep(&ts, NULL));   // Sleep 1 second

  struct timeval tv2;
  EXPECT(0 == gettimeofday(&tv2, NULL));   // Get time of day again

  /*
   * Because of our nanosleep call, tv2 should have a later time than tv1
   */
  EXPECT(tv2.tv_sec > tv1.tv_sec);

  struct timeval tv3;
  struct timezone tz;
  tz.tz_minuteswest = 0;
  tz.tz_dsttime = 0;

  /*
   * Test gettimeofday using obselete timezone struct pointer
   */
  EXPECT(0 == gettimeofday(&tv3, &tz));  // Get time of day again

  /*
   * The time of day (tv3) should not be earlier than time of day (tv2)
   */
  EXPECT(tv3.tv_sec >= tv2.tv_sec);

  /*
   * Test nanosleep error conditions
   */
  EXPECT(0 != nanosleep(NULL, NULL));
  END_TEST();
}

/*
 * Returns failure count.  t_suspend should not be shorter than 1us,
 * since elapsed time measurement cannot possibly be any finer in
 * granularity.  In practice, 1ms is probably the best we can hope for
 * in timer resolution, so even if nanosleep suspends for 1us, the
 * gettimeofday resolution may cause a false failure report.
 */
int TestNanoSleep(struct timespec *t_suspend) {
  START_TEST("Test nanosleep");
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
  EXPECT(-1 != gettimeofday(&t_start, NULL));

  while (-1 == (rv = nanosleep(&t_remain, &t_remain)) &&
         EINTR == errno) {
  }
  EXPECT(-1 != rv);

  EXPECT(-1 != gettimeofday(&t_end, NULL));

  t_elapsed.tv_sec = t_end.tv_sec - t_start.tv_sec;
  t_elapsed.tv_usec = t_end.tv_usec - t_start.tv_usec;
  if (t_elapsed.tv_usec < 0) {
    t_elapsed.tv_usec += MICROS_PER_UNIT;
    t_elapsed.tv_sec -= 1;
  }
  if (t_elapsed.tv_usec >= MICROS_PER_UNIT) {
    printf("Microsecond field too large: %ld\n", t_elapsed.tv_usec);
  }

  printf("%40s: %ld.%06ld seconds\n",
         "Actual nanosleep duration",
         t_elapsed.tv_sec,
         t_elapsed.tv_usec);

  /*
   * This is the original check and error message (from nanosleep_test.c),
   * which provides feedback and is more readable.
   */
  if (t_elapsed.tv_sec < t_suspend->tv_sec ||
      (t_elapsed.tv_sec == t_suspend->tv_sec &&
       (NANOS_PER_MICRO * t_elapsed.tv_usec < t_suspend->tv_nsec))) {
    printf("Error: Elapsed time too short!\n");
  }

  /*
   * This check works with BEGIN_TEST/END_TEST and restates the check above
   */
  EXPECT(!(t_elapsed.tv_sec < t_suspend->tv_sec ||
          (t_elapsed.tv_sec == t_suspend->tv_sec &&
          (NANOS_PER_MICRO * t_elapsed.tv_usec < t_suspend->tv_nsec))));
  END_TEST();
}

/*
 * function testSuite()
 *
 *   Run through a complete sequence of file tests.
 *
 * returns true if all tests succeed.  false if one or more fail.
 */
bool TestSuite() {
  int fail_count = 0;

  fail_count += TestTimeFuncs();
  fail_count += TestClockFunction();

  /*
   * Copied from tests/nanosleep.c
   */
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

  for (unsigned int ix = 0; ix < sizeof t_suspend/sizeof t_suspend[0]; ++ix) {
    fail_count += TestNanoSleep(&t_suspend[ix]);
  }

  return fail_count;
}

/*
 * main entry point.
 *
 * run all tests and call system exit with appropriate value
 *   0 - success, all tests passed.
 *  -1 - one or more tests failed.
 */

int main(const int argc, const char *argv[]) {
  int fail_count = TestSuite();

  if (fail_count == 0)
    printf("All tests PASSED\n");
  else
    printf("There were %d failures\n", fail_count);

  exit(fail_count);
}

