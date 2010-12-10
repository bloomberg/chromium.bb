/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Expiration time test.
 */

#include <errno.h>

#include "native_client/src/trusted/expiration/expiration.h"
#include "native_client/src/shared/platform/nacl_check.h"

time_t gTestTimeOutValue;
int gTestTimeErrno;

time_t TestTimeFn(time_t *out_time) {
  if (((time_t) -1) == gTestTimeOutValue) {
    errno = gTestTimeErrno;
    return -1;
  }
  if (NULL != out_time) {
    *out_time = gTestTimeOutValue;
  }
  return gTestTimeOutValue;
}

time_t gTestMktimeOutValue;

time_t TestMktimeFn(struct tm *in_broken_out) {
  UNREFERENCED_PARAMETER(in_broken_out);
  /* ignore the broken out time input */
  return gTestMktimeOutValue;
}

int main() {
  int errors = 0;

  static struct tm exp_tm;

  printf("One second after expiration is expired\n");
  gTestTimeOutValue = 1001;  /* now */
  gTestMktimeOutValue = 1000;  /* expiration */
  if (NaClHasExpiredMockable(TestTimeFn, TestMktimeFn)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("now is expired\n");
  gTestTimeOutValue = 1000;  /* expiration */
  if (NaClHasExpiredMockable(TestTimeFn, TestMktimeFn)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("one second before expiration is okay\n");
  gTestTimeOutValue = 999;  /* expiration */
  if (!NaClHasExpiredMockable(TestTimeFn, TestMktimeFn)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("NOW should not be expired (SDK should be up-to-date!)\n");
  gTestTimeOutValue = time((time_t *) NULL);
  if (!NaClHasExpiredMockable(TestTimeFn, mktime)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("Actual day of expiration is not expired\n");
  exp_tm.tm_isdst = -1;
  exp_tm.tm_yday = 0;
  exp_tm.tm_wday = 0;
  exp_tm.tm_year = EXPIRATION_YEAR - 1900;
  exp_tm.tm_mon = EXPIRATION_MONTH - 1;
  exp_tm.tm_mday = EXPIRATION_DAY;
  exp_tm.tm_hour = 0;
  exp_tm.tm_min = 0;
  exp_tm.tm_sec = 0;
  gTestTimeOutValue = mktime(&exp_tm);
  if (!NaClHasExpiredMockable(TestTimeFn, mktime)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("Last second of day of expiration should not be expired\n");
  exp_tm.tm_isdst = -1;
  exp_tm.tm_yday = 0;
  exp_tm.tm_wday = 0;
  exp_tm.tm_year = EXPIRATION_YEAR - 1900;
  exp_tm.tm_mon = EXPIRATION_MONTH - 1;
  exp_tm.tm_mday = EXPIRATION_DAY;
  exp_tm.tm_hour = 23;
  exp_tm.tm_min = 59;
  exp_tm.tm_sec = 59;
  gTestTimeOutValue = mktime(&exp_tm);
  if (!NaClHasExpiredMockable(TestTimeFn, mktime)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("Day afte expiration should be expired\n");
  exp_tm.tm_isdst = -1;
  exp_tm.tm_yday = 0;
  exp_tm.tm_wday = 0;
  exp_tm.tm_year = EXPIRATION_YEAR - 1900;
  exp_tm.tm_mon = EXPIRATION_MONTH - 1;
  exp_tm.tm_mday = EXPIRATION_DAY + 1;
  exp_tm.tm_hour = 0;
  exp_tm.tm_min = 0;
  exp_tm.tm_sec = 0;
  gTestTimeOutValue = mktime(&exp_tm);
  if (NaClHasExpiredMockable(TestTimeFn, mktime)) {
    printf("OK\n");
  } else {
    ++errors;
    printf("ERROR\n");
  }

  printf("%s\n", (0 == errors) ? "PASSED" : "FAILED");
  return 0 != errors;
}
