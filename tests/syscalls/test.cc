/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Generic functions for testing
 */
#include "native_client/tests/syscalls/test.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Must be implemented by the class that specifies the unit tests for a topic.
 */
bool TestSuite(void);

bool test::Failed(const char *testname, const char *msg) {
  if (msg != NULL) {
    printf("TEST FAILED: %s: %s\n", testname, msg);
  } else {
    printf("TEST FAILED: %s\n", testname);
  }
  return false;
}

bool test::Passed(const char *testname, const char *msg) {
  if (msg != NULL) {
    printf("TEST PASSED: %s: %s\n", testname, msg);
  } else {
    printf("TEST PASSED: %s\n", testname);
  }
  return true;
}

/*
 * Main entry point.
 *
 * Runs all tests and calls system exit with appropriate value
 *   0 - success, all tests passed.
 *  -1 - one or more tests failed.
 */
int main(const int argc, const char *argv[]) {
  bool passed;

  // run the full test suite
  passed = TestSuite();

  if (passed) {
    printf("All tests PASSED\n");
    exit(0);
  } else {
    printf("One or more tests FAILED\n");
    exit(-1);
  }
}
