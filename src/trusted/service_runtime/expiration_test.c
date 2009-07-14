/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Expiration time test.
 */

#include <errno.h>

#if defined(HAVE_SDL)
# include <SDL.h>
#endif

#include "native_client/src/trusted/service_runtime/expiration.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"

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
  /* ignore the broken out time input */
  return gTestMktimeOutValue;
}

int main(int argc, char **argv) {
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
