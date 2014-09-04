/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/irt_ext/error_report.h"
#include "native_client/tests/irt_ext/basic_calls.h"
#include "native_client/tests/irt_ext/libc/libc_test.h"

typedef int (*TYPE_basic_test)(struct basic_calls_environment *basic_call_env);

#define TEST_TIME_VALUE 20
#define TEST_NANOTIME_VALUE 123

static int do_time_test(struct basic_calls_environment *basic_call_env) {
  time_t retrieved_time;

  basic_call_env->current_time = TEST_TIME_VALUE;
  retrieved_time = time(NULL);

  if (retrieved_time != TEST_TIME_VALUE) {
    irt_ext_test_print("do_time_test: did not retrieve expected time.\n"
                       "  Retrieved time: %d. Expected time: %d.\n",
                       (int) retrieved_time, TEST_TIME_VALUE);
    return 1;
  }

  if (basic_call_env->current_time <= retrieved_time) {
    irt_ext_test_print("do_time_test: current time less than sampled time.\n"
                       "  Current time: %d. Sampled time: %d.\n",
                       (int) basic_call_env->current_time,
                       (int) retrieved_time);
    return 1;
  }

  return 0;
}

static int do_clock_test(struct basic_calls_environment *basic_call_env) {
  clock_t retrieved_clock;

  basic_call_env->current_time = TEST_TIME_VALUE;
  retrieved_clock = clock();

  if (retrieved_clock != TEST_TIME_VALUE) {
    irt_ext_test_print("do_clock_test: did not retrieve expected clock.\n"
                       "  Retrieved clock: %d. Expected clock: %d.\n",
                       (int) retrieved_clock, TEST_TIME_VALUE);
    return 1;
  }

  if (basic_call_env->current_time <= retrieved_clock) {
    irt_ext_test_print("do_clock_test: current time less than sampled clock.\n"
                       "  Current clock: %d. Sampled clock: %d.\n",
                       (int) basic_call_env->current_time,
                       (int) retrieved_clock);
    return 1;
  }

  return 0;
}

static int do_nanosleep_test(struct basic_calls_environment *basic_call_env) {
  struct timespec sleep_time = {
    TEST_TIME_VALUE,
    TEST_NANOTIME_VALUE
  };

  struct timespec remaining_time = {
    0,
    0
  };

  basic_call_env->current_time = TEST_TIME_VALUE;
  if (0 != nanosleep(&sleep_time, &remaining_time)) {
    irt_ext_test_print("do_nanosleep_test: nanosleep failed - %s.\n",
                       strerror(errno));
    return 1;
  }

  if (remaining_time.tv_sec != 0 ||
      remaining_time.tv_nsec != sleep_time.tv_nsec) {
    irt_ext_test_print("do_nanosleep_test: unexpected sleep remainder.\n"
                       "  Expected: 0:%d. Retrieved: %d:%d.\n",
                       (int) sleep_time.tv_nsec,
                       (int) remaining_time.tv_sec,
                       (int) remaining_time.tv_nsec);
    return 1;
  }

  if (basic_call_env->current_time != TEST_TIME_VALUE + sleep_time.tv_sec) {
    irt_ext_test_print("do_nanosleep_test: env did not sleep correctly.\n"
                       "  Expected: %d. Retrieved: %d.\n",
                       (int) (TEST_TIME_VALUE + sleep_time.tv_sec),
                       (int) basic_call_env->current_time);
    return 1;
  }

  return 0;
}

static int do_sched_yield_test(struct basic_calls_environment *basic_call_env) {
  if (basic_call_env->thread_yielded) {
    irt_ext_test_print("do_sched_yield_test: env was not initialized.\n");
    return 1;
  }

  if (0 != sched_yield()) {
    irt_ext_test_print("do_sched_yield_test: sched_yield failed - %s.\n",
                       strerror(errno));
    return 1;
  }

  if (!basic_call_env->thread_yielded) {
    irt_ext_test_print("do_sched_yield_test: env did not yield thread.\n");
    return 1;
  }

  return 0;
}

static int do_sysconf_test(struct basic_calls_environment *basic_call_env) {
  long ret = sysconf(SYSCONF_TEST_QUERY);
  if (ret == -1) {
    irt_ext_test_print("do_sysconf_test: sysconf failed - %s.\n",
                       strerror(errno));
    return 1;
  }

  if (SYSCONF_TEST_VALUE != ret) {
    irt_ext_test_print("do_sysconf_test: sysconf returned unexpected value.\n"
                       "  Expected value: %d. Retrieved value: %d.\n",
                       SYSCONF_TEST_VALUE, (int) ret);
    return 1;
  }

  return 0;
}

static const TYPE_basic_test g_basic_tests[] = {
  do_time_test,
  do_clock_test,
  do_nanosleep_test,
  do_sched_yield_test,
  do_sysconf_test,
};

int run_basic_tests(void) {
  struct basic_calls_environment basic_calls_env;

  int num_failures = 0;
  irt_ext_test_print("Running %d basic tests...\n",
                     NACL_ARRAY_SIZE(g_basic_tests));

  for (int i = 0; i < NACL_ARRAY_SIZE(g_basic_tests); i++) {
    init_basic_calls_environment(&basic_calls_env);
    activate_basic_calls_env(&basic_calls_env);

    if (0 != g_basic_tests[i](&basic_calls_env)) {
      num_failures++;
    }

    deactivate_basic_calls_env();
  }

  if (num_failures > 0) {
    irt_ext_test_print("Basic Tests Failed [%d/%d]\n", num_failures,
                       NACL_ARRAY_SIZE(g_basic_tests));
  } else {
    irt_ext_test_print("Basic Tests Passed [%d/%d]\n",
                       NACL_ARRAY_SIZE(g_basic_tests),
                       NACL_ARRAY_SIZE(g_basic_tests));
  }

  return num_failures;
}
