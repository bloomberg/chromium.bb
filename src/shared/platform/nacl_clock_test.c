/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "native_client/src/shared/platform/nacl_clock.h"

#include "native_client/src/shared/platform/platform_init.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

/*
 * Very basic sanity check.  With clock functionality, tests are a
 * pain without a set of globally consistent dependency injection for
 * syscalls, since faking out time-related syscalls in the test
 * without faking out the same syscalls used by other modules is
 * difficult.  Furthermore, this test is trying to verify basic
 * functionality -- and testing against a mock interface that works
 * according to our expectations of what the syscalls will do isn't
 * the same: our assumptions might be wrong, and we ought to have a
 * test that verifies end-to-end functionality.  Here, we just compare
 * clock_gettime of the realtime clock against gettimeofday, and do
 * two monotonic clock samples between a nanosleep to verify that the
 * monotonic clock *approximately* measured the sleep duration -- with
 * great fuzziness.
 *
 * On an unloaded i7, 1ms with a 1.25 fuzziness factor and a 100,000
 * ns constant syscall overhead works fine.  On bots, we have to be
 * much more generous.  (This is especially true for qemu-based
 * testing.)
 */
#define NANOS_PER_MICRO   (1000)
#define MICROS_PER_MILLI  (1000)
#define NANOS_PER_MILLI   (NANOS_PER_MICRO * MICROS_PER_MILLI)
#define MICROS_PER_UNIT   (1000 * 1000)
#define NANOS_PER_UNIT    (NANOS_PER_MICRO * MICROS_PER_UNIT)

#define DEFAULT_NANOSLEEP_EXTRA_OVERHEAD  (10 * NANOS_PER_MILLI)
#define DEFAULT_NANOSLEEP_EXTRA_FACTOR    (100.0)
#define DEFAULT_NANOSLEEP_TIME            (10 * NANOS_PER_MILLI)

/*
 * Global testing parameters -- fuzziness coefficients in determining
 * what is considered accurate.
 */
double   g_fuzzy_factor = DEFAULT_NANOSLEEP_EXTRA_FACTOR;
uint64_t g_syscall_overhead = DEFAULT_NANOSLEEP_EXTRA_OVERHEAD;
uint64_t g_slop_ms = 0;

/*
 * ClockMonotonicAccuracyTest samples the NACL_ABI_CLOCK_MONOTONIC
 * clock before and after invoking NaClNanosleep and computes the time
 * delta.  The test is considered to pass if the time delta is close
 * to the requested value.  "Close" is a per-host-OS attribute, thus
 * the above testing parameters.
 */
int ClockMonotonicAccuracyTest(uint64_t sleep_nanos) {
  int                       num_failures = 0;

  int                       err;
  struct nacl_abi_timespec  t_start;
  struct nacl_abi_timespec  t_sleep;
  struct nacl_abi_timespec  t_end;

  uint64_t                  elapsed_nanos;
  uint64_t                  elapsed_lower_bound;
  uint64_t                  elapsed_upper_bound;

  t_sleep.tv_sec  = sleep_nanos / NANOS_PER_UNIT;
  t_sleep.tv_nsec = sleep_nanos % NANOS_PER_UNIT;

  printf("\nCLOCK_MONOTONIC accuracy test:\n");

  if (0 != (err = NaClClockGetTime(NACL_ABI_CLOCK_MONOTONIC, &t_start))) {
    fprintf(stderr,
            "nacl_clock_test: NaClClockGetTime (start) failed, error %d\n",
            err);
    ++num_failures;
    goto done;
  }
  for (;;) {
    err = NaClNanosleep(&t_sleep, &t_sleep);
    if (0 == err) {
      break;
    }
    if (-NACL_ABI_EINTR == err) {
      /* interrupted syscall: sleep some more */
      continue;
    }
    fprintf(stderr,
            "nacl_clock_test: NaClNanoSleep failed, error %d\n", err);
    num_failures++;
    goto done;
  }
  if (0 != (err = NaClClockGetTime(NACL_ABI_CLOCK_MONOTONIC, &t_end))) {
    fprintf(stderr,
            "nacl_clock_test: NaClClockGetTime (end) failed, error %d\n",
            err);
    return 1;
  }

  elapsed_nanos = (t_end.tv_sec - t_start.tv_sec) * NANOS_PER_UNIT
      + (t_end.tv_nsec - t_start.tv_nsec) + g_slop_ms * NANOS_PER_MILLI;

  elapsed_lower_bound = sleep_nanos;
  elapsed_upper_bound = (uint64_t) (sleep_nanos * g_fuzzy_factor
                                    + g_syscall_overhead);

  printf("requested sleep:      %20"NACL_PRIu64" nS\n", sleep_nanos);
  printf("actual elapsed sleep: %20"NACL_PRIu64" nS\n", elapsed_nanos);
  printf("sleep lower bound:    %20"NACL_PRIu64" nS\n", elapsed_lower_bound);
  printf("sleep upper bound:    %20"NACL_PRIu64" nS\n", elapsed_upper_bound);

  if (elapsed_nanos < elapsed_lower_bound
      || elapsed_upper_bound < elapsed_nanos) {
    printf("discrepancy too large\n");
    num_failures++;
  }
 done:
  printf((0 == num_failures) ? "PASSED\n" : "FAILED\n");
  return num_failures;
}

/*
 * ClockRealtimeAccuracyTest compares the time returned by
 * NACL_ABI_CLOCK_REALTIME against that returned by NaClGetTimeOfDay.
 */
int ClockRealtimeAccuracyTest(void) {
  int                       num_failures = 0;

  int                       err;
  struct nacl_abi_timespec  t_now_ts;
  struct nacl_abi_timeval   t_now_tv;

  uint64_t                  t_now_ts_nanos;
  uint64_t                  t_now_tv_nanos;
  int64_t                   t_now_diff_nanos;

  printf("\nCLOCK_REALTIME accuracy test:\n");

  if (0 != (err = NaClClockGetTime(NACL_ABI_CLOCK_REALTIME, &t_now_ts))) {
    fprintf(stderr,
            "nacl_clock_test: NaClClockGetTime (now) failed, error %d\n",
            err);
    num_failures++;
    goto done;
  }
  if (0 != (err = NaClGetTimeOfDay(&t_now_tv))) {
    fprintf(stderr,
            "nacl_clock_test: NaClGetTimeOfDay (now) failed, error %d\n",
            err);
    num_failures++;
    goto done;
  }

  t_now_ts_nanos = t_now_ts.tv_sec * NANOS_PER_UNIT + t_now_ts.tv_nsec;
  t_now_tv_nanos = t_now_tv.nacl_abi_tv_sec * NANOS_PER_UNIT
      + t_now_tv.nacl_abi_tv_usec * NANOS_PER_MICRO;

  printf("clock_gettime:   %20"NACL_PRIu64" nS\n", t_now_ts_nanos);
  printf("gettimeofday:    %20"NACL_PRIu64" nS\n", t_now_tv_nanos);

  t_now_diff_nanos = t_now_ts_nanos - t_now_tv_nanos;
  if (t_now_diff_nanos < 0) {
    t_now_diff_nanos = -t_now_diff_nanos;
  }
  printf("time difference: %20"NACL_PRId64" nS\n", t_now_diff_nanos);

  if (t_now_ts_nanos < g_syscall_overhead) {
    printf("discrepancy too large\n");
    num_failures++;
  }
 done:
  printf((0 == num_failures) ? "PASSED\n" : "FAILED\n");
  return num_failures;
}

int main(int ac, char **av) {
  uint64_t                  sleep_nanos = DEFAULT_NANOSLEEP_TIME;

  int                       opt;
  uint32_t                  num_failures = 0;

  puts("This is a basic functionality test being repurposed as an");
  puts(" unit/regression test.  The test parameters default to values that");
  puts("are more appropriate for heavily loaded continuous-testing robots.");
  printf("\nThe default values are:\n -S %"NACL_PRIu64
         " -f %f -o %"NACL_PRIu64" -s %"NACL_PRIu64"\n\n",
         sleep_nanos, g_fuzzy_factor, g_syscall_overhead, g_slop_ms);
  puts("For testing functionality, this test should be run with a different");
  puts("set of parameters.  On an unloaded i7, a sleep duration (-S) of");
  puts("1000000 ns (one millisecond), with a fuzziness factor (-f) of 1.25,");
  puts("a constant test overhead of 100000 ns (100 us), and a");
  puts("sleep duration \"slop\" (-s) of 0 is fine.");

  while (-1 != (opt = getopt(ac, av, "f:o:s:S:"))) {
    switch (opt) {
      case 'f':
        g_fuzzy_factor = strtod(optarg, (char **) NULL);
        break;
      case 'o':
        g_syscall_overhead = strtoul(optarg, (char **) NULL, 0);
        break;
      case 's':
        g_slop_ms = strtoul(optarg, (char **) NULL, 0);
        break;
      case 'S':
        sleep_nanos = strtoul(optarg, (char **) NULL, 0);
        break;
      default:
        fprintf(stderr, "nacl_clock_test: unrecognized option `%c'.\n",
                opt);
        fprintf(stderr,
                "Usage: nacl_clock_test [-f fuzz_factor] [-s sleep_nanos]\n"
                "       [-o syscall_overhead_nanos]\n");
        return -1;
    }
  }

  NaClPlatformInit();

  num_failures += ClockMonotonicAccuracyTest(sleep_nanos);
  num_failures += ClockRealtimeAccuracyTest();

  NaClPlatformFini();

  return num_failures;
}
