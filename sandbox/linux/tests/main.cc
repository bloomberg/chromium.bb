// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/test/test_suite.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

// Check for leaks in our tests.
void RunPostTestsChecks() {
  if (TestUtils::CurrentProcessHasChildren()) {
    LOG(ERROR) << "One of the tests created a child that was not waited for. "
               << "Please, clean-up after your tests!";
  }
}

}  // namespace
}  // namespace sandbox

#if defined(OS_ANDROID)
void UnitTestAssertHandler(const std::string& str) {
  _exit(1);
}
#endif

int main(int argc, char* argv[]) {
#if defined(OS_ANDROID)
  // The use of Callbacks requires an AtExitManager.
  base::AtExitManager exit_manager;
  testing::InitGoogleTest(&argc, argv);
  // Death tests rely on LOG(FATAL) triggering an exit (the default behavior is
  // SIGABRT).  The normal test launcher does this at initialization, but since
  // we still do not use this on Android, we must install the handler ourselves.
  logging::SetLogAssertHandler(UnitTestAssertHandler);
#endif
  // Always go through re-execution for death tests.
  // This makes gtest only marginally slower for us and has the
  // additional side effect of getting rid of gtest warnings about fork()
  // safety.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
#if defined(OS_ANDROID)
  int tests_result = RUN_ALL_TESTS();
#else
  int tests_result = base::RunUnitTestsUsingBaseTestSuite(argc, argv);
#endif

  sandbox::RunPostTestsChecks();
  return tests_result;
}
