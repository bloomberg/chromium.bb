// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_UNIT_TESTS_H__
#define SANDBOX_LINUX_TESTS_UNIT_TESTS_H__

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Define a new test case that runs inside of a death test. This is necessary,
// as most of our tests by definition make global and irreversible changes to
// the system (i.e. they install a sandbox). GTest provides death tests as a
// tool to isolate global changes from the rest of the tests.
#define SANDBOX_TEST(test_case_name, test_name)                               \
  void TEST_##test_name(void *);                                              \
  TEST(test_case_name, test_name) {                                           \
    sandbox::UnitTests::RunTestInProcess(TEST_##test_name, NULL);             \
  }                                                                           \
  void TEST_##test_name(void *)

// Simple assertion macro that is compatible with running inside of a death
// test. We unfortunately cannot use any of the GTest macros.
#define SANDBOX_STR(x) #x
#define SANDBOX_ASSERT(expr)                                                  \
  ((expr)                                                                     \
   ? static_cast<void>(0)                                                     \
   : sandbox::UnitTests::AssertionFailure(SANDBOX_STR(expr),                  \
                                          __FILE__, __LINE__))

class UnitTests {
 public:
  typedef void (*Test)(void *);

  // Runs a test inside a short-lived process. Do not call this function
  // directly. It is automatically invoked by SANDBOX_TEST(). Most sandboxing
  // functions make global irreversible changes to the execution environment
  // and must therefore execute in their own isolated process.
  static void RunTestInProcess(Test test, void *arg);

  // Report a useful error message and terminate the current SANDBOX_TEST().
  // Calling this function from outside a SANDBOX_TEST() is unlikely to do
  // anything useful.
  static void AssertionFailure(const char *expr, const char *file, int line);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UnitTests);
};

}  // namespace

#endif  // SANDBOX_LINUX_TESTS_UNIT_TESTS_H__
