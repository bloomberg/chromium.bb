/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */





#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/perf_counter/nacl_perf_counter.h"

/* A simple test of the performance counter basics. */

struct perf_counter_test {
  struct nacl_abi_timeval _[2];
  int64_t res;
};

struct perf_counter_test arr[] = {
  { { { 0, 0 }, { 0, 0 } }, 0 },
  { { { 0, 100 }, { 0, 0 } }, -100 },
  { { { 1, 100, }, { 2, 5 } }, (999*1000 + 905) },
  { { { 0, 0 }, { 0, 0 } }, 0 },
};


#define ALEN(A) ((int)(sizeof(A)/sizeof(A[0])))

int main(int argc, char*argv[]) {
  int i = 0;
  int64_t res = 0;
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);
  NaClLogModuleInit();
  NaClTimeInit();

  for (; i < ALEN(arr)-1; ++i) {
    struct NaClPerfCounter tm;
    memset(&tm, 0, sizeof(struct NaClPerfCounter));

    tm.sample_list[0] = arr[i]._[0];
    tm.sample_list[1] = arr[i]._[1];
    tm.samples = 2;
    res = NaClPerfCounterInterval(&tm, 0, 1);
    ASSERT_EQ(res, arr[i].res);
  }

  do {
    /* live run */
    unsigned i;
    struct NaClPerfCounter tm;
    NaClPerfCounterCtor(&tm, "test_perf_counter");

    ASSERT_EQ(1, tm.samples);

    for (i = 1; i < NACL_MAX_PERF_COUNTER_SAMPLES-2; ++i) {
      NaClPerfCounterMark(&tm, "foo");
      ASSERT_EQ(i+1, tm.samples);
    }
  } while (0);

  do {
    /* live run */
    unsigned i;
    struct NaClPerfCounter tm;
    int r = 0;
    const char *names[] = {
      "--",
      "first",
      "second",
      "third",
      "fourth",
      "fifth",
      "sixth",
      "a-really-long-name-which-will-be-truncated",
      "eighth",
      "ninth",
      "tenth",
      "eleventh",
      "twelveth",
      "thirteenth",
    };

    NaClPerfCounterCtor(&tm, "test_perf_counter");

    ASSERT_EQ(1, tm.samples);

    for (i = 1; i < NACL_MAX_PERF_COUNTER_SAMPLES; ++i) {
      r = NaClPerfCounterMark(&tm, names[i]);
      ASSERT_EQ(i+1, tm.samples);
      ASSERT_EQ(i, (unsigned)r);
      ASSERT_EQ(0, strncmp(tm.sample_names[i], names[i], \
                           (NACL_MAX_PERF_COUNTER_NAME-1)));
    }
    ASSERT_EQ(NACL_MAX_PERF_COUNTER_SAMPLES, tm.samples);
    ASSERT_EQ(strlen(tm.sample_names[7]), (NACL_MAX_PERF_COUNTER_NAME-1));

    r = NaClPerfCounterMark(&tm, "foo");
    ASSERT_EQ(NACL_MAX_PERF_COUNTER_SAMPLES, tm.samples);
    ASSERT_EQ(-1, r);
    ASSERT_NE(-1, NaClPerfCounterInterval(&tm, 0, 1));
  } while (0);

  NaClTimeFini();
  NaClLogModuleFini();
  return 0;
}
