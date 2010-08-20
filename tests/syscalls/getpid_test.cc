/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Test for getpid syscall.
 */

#include "native_client/tests/syscalls/test.h"  //NOLINT

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bool TestGetPid() {
  bool test_status;
  const char *testname = "getpid_test";
  pid_t pid_one = getpid();
  pid_t pid_two = getpid();

  // check if it's greater than 0.
  if ((pid_one > 0) && (pid_one == pid_two)) {
    test_status =
        test::Passed(testname,
                     "getpid returned what appears to be a valid pid.");
  } else if (pid_one == pid_two) {
    // a buffer size that should be sufficient
    char buffer[256];
    snprintf(buffer,
             255,
             "getpid returned an invalid pid for this test: %d",
             pid_one);
    buffer[255] = 0;
    test_status = test::Failed(testname, buffer);
  } else {
    char buffer[256];
    snprintf(buffer,
             255,
             "getpid returned different pids for the same process.  First "
             "time it was called: %d Second time: %d",
             pid_one, pid_two);
    buffer[255] = 0;
    test_status = test::Failed(testname, buffer);
  }
  return test_status;
}

/*
 * function testSuite()
 *
 *   Run through a complete sequence of file tests.
 *
 * returns true if all tests succeed.  false if one or more fail.
 */
bool TestSuite() {
  bool ret = true;
  ret &= TestGetPid();
  return ret;
}
