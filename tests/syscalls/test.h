/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef TESTS_SYSCALLS_TEST_H_
#define TESTS_SYSCALLS_TEST_H_

/*
 * Generic functions for testing
 */


namespace test {
  /*
   * Call this to print a failure and return false.
   */
  bool Failed(const char *testname, const char *msg);

  /*
   * Call this to print a message and return true.  Note that this message
   * should not contain data specific to an instance of a test run, so that
   * golden output can be used.
   */
  bool Passed(const char *testname, const char *msg);
}

#endif  // TESTS_SYSCALLS_TEST_H_
