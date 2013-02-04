/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/tests/performance/perf_test_runner.h"


double TimeIterations(PerfTest *test, int iterations) {
  struct timespec start_time;
  struct timespec end_time;
  ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &start_time), 0);
  for (int i = 0; i < iterations; i++) {
    test->run();
  }
  ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &end_time), 0);
  double total_time =
      (end_time.tv_sec - start_time.tv_sec
       + (double) (end_time.tv_nsec - start_time.tv_nsec) / 1e9);
  // Output the raw data.
  printf("%.3f usec (%g sec) per iteration: %g sec for %i iterations\n",
         total_time / iterations * 1e6,
         total_time / iterations,
         total_time, iterations);
  return total_time;
}

int CalibrateIterationCount(PerfTest *test, double target_time,
                            int sample_count) {
  int calibration_iterations = 100;
  double calibration_time;
  for (;;) {
    calibration_time = TimeIterations(test, calibration_iterations);
    // If the test completed too quickly to get an accurate
    // measurement, try a larger number of iterations.
    if (calibration_time >= 1e-5)
      break;
    ASSERT_LE(calibration_iterations, INT_MAX / 10);
    calibration_iterations *= 10;
  }

  double iterations_d =
      (target_time / (calibration_time / calibration_iterations)
       / sample_count);
  // Sanity checks for very fast or very slow tests.
  ASSERT_LE(iterations_d, INT_MAX);
  int iterations = iterations_d;
  if (iterations < 1)
    iterations = 1;
  return iterations;
}

void TimePerfTest(PerfTest *test, double *mean, double *stddev) {
  // 'target_time' is the amount of time we aim to run this perf test
  // for in total.
  double target_time = 0.5;  // seconds
  // 'sample_count' is the number of separate timings we take in order
  // to measure the variability of the results.
  int sample_count = 5;
  int iterations = CalibrateIterationCount(test, target_time, sample_count);

  double sum = 0;
  double sum_of_squares = 0;
  for (int i = 0; i < sample_count; i++) {
    double time = TimeIterations(test, iterations) / iterations;
    sum += time;
    sum_of_squares += time * time;
  }
  *mean = sum / sample_count;
  *stddev = sqrt(sum_of_squares / sample_count - *mean * *mean);
}

void RunPerfTest(const char *description_string, const char *test_name,
                 PerfTest *test) {
  double mean;
  double stddev;
  printf("\n%s:\n", test_name);
  TimePerfTest(test, &mean, &stddev);
  printf("mean:   %.6f usec\n", mean * 1e6);
  printf("stddev: %.6f usec\n", stddev * 1e6);
  printf("relative stddev: %.2f%%\n", stddev / mean * 100);
  // Output the result in a format that Buildbot will recognise in the
  // logs and record, using the Chromium perf testing infrastructure.
  printf("RESULT %s: %s= {%.6f, %.6f} microseconds\n",
         test_name, description_string, mean * 1e6, stddev * 1e6);

  delete test;
}

int main(int argc, char **argv) {
  const char *description_string = argc >= 2 ? argv[1] : "time";

  // Turn off stdout buffering to aid debugging.
  setvbuf(stdout, NULL, _IONBF, 0);

#define RUN_TEST(class_name) \
    extern PerfTest *Make##class_name(); \
    RunPerfTest(description_string, #class_name, Make##class_name());

  RUN_TEST(TestNull);
#if defined(__native_client__)
  RUN_TEST(TestNaClSyscall);
#endif
#if NACL_LINUX
  RUN_TEST(TestHostSyscall);
#endif
  RUN_TEST(TestClockGetTime);
  RUN_TEST(TestTlsVariable);
  RUN_TEST(TestMmapAnonymous);
  RUN_TEST(TestAtomicIncrement);
  RUN_TEST(TestUncontendedMutexLock);
  RUN_TEST(TestCondvarSignalNoOp);
  RUN_TEST(TestThreadCreateAndJoin);
  RUN_TEST(TestThreadWakeup);

#if defined(__native_client__)
  // Test untrusted fault handling.  This should come last because, on
  // Windows, registering a fault handler has a performance impact on
  // thread creation and exit.  This is because when the Windows debug
  // exception handler is attached to sel_ldr as a debugger, Windows
  // suspends the whole sel_ldr process every time a thread is created
  // or exits.
  RUN_TEST(TestCatchingFault);
  // Measure that overhead by running MakeTestThreadCreateAndJoin again.
  RunPerfTest(description_string,
              "TestThreadCreateAndJoinAfterSettingFaultHandler",
              MakeTestThreadCreateAndJoin());
#endif

#undef RUN_TEST

  return 0;
}
