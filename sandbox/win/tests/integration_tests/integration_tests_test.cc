// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some tests for the framework itself.

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/tests/common/controller.h"

namespace sandbox {

// Returns the current process state.
SBOX_TESTS_COMMAND int IntegrationTestsTest_state(int argc, wchar_t **argv) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return BEFORE_INIT;

  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    return BEFORE_REVERT;

  return AFTER_REVERT;
}

// Returns the current process state, keeping track of it.
SBOX_TESTS_COMMAND int IntegrationTestsTest_state2(int argc, wchar_t **argv) {
  static SboxTestsState state = MIN_STATE;
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled()) {
    if (MIN_STATE == state)
      state = BEFORE_INIT;
    return state;
  }

  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf()) {
    if (BEFORE_INIT == state)
      state = BEFORE_REVERT;
    return state;
  }

  if (BEFORE_REVERT == state)
    state =  AFTER_REVERT;
  return state;
}

// Blocks the process for argv[0] milliseconds simulating stuck child.
SBOX_TESTS_COMMAND int IntegrationTestsTest_stuck(int argc, wchar_t **argv) {
  int timeout = 500;
  if (argc > 0) {
    timeout = _wtoi(argv[0]);
  }

  ::Sleep(timeout);
  return 1;
}

// Returns the number of arguments
SBOX_TESTS_COMMAND int IntegrationTestsTest_args(int argc, wchar_t **argv) {
  for (int i = 0; i < argc; i++) {
    wchar_t argument[20];
    size_t argument_bytes = wcslen(argv[i]) * sizeof(wchar_t);
    memcpy(argument, argv[i], __min(sizeof(argument), argument_bytes));
  }

  return argc;
}

TEST(IntegrationTestsTest, CallsBeforeInit) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_INIT);
  ASSERT_EQ(BEFORE_INIT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsBeforeRevert) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_REVERT);
  ASSERT_EQ(BEFORE_REVERT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsAfterRevert) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(AFTER_REVERT);
  ASSERT_EQ(AFTER_REVERT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsEveryState) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  ASSERT_EQ(AFTER_REVERT, runner.RunTest(L"IntegrationTestsTest_state2"));
}

TEST(IntegrationTestsTest, ForwardsArguments) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_INIT);
  ASSERT_EQ(1, runner.RunTest(L"IntegrationTestsTest_args first"));
  ASSERT_EQ(4, runner.RunTest(L"IntegrationTestsTest_args first second third "
                              L"fourth"));
}

TEST(IntegrationTestsTest, StuckChild) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the process must have finished.
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(1, exit_code);
}

TEST(IntegrationTestsTest, StuckChildNoJob) {
  TestRunner runner(JOB_NONE, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner.SetTimeout(2000);
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 2000"));
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the processes are not tracked by the broker and should be
  // still active.
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner.process(), 0);
}

TEST(IntegrationTestsTest, TwoStuckChildrenSecondOneHasNoJob) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  TestRunner runner2(JOB_NONE, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner2.SetTimeout(2000);
  runner2.SetAsynchronous(true);
  runner2.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner2.RunTest(L"IntegrationTestsTest_stuck 2000"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the processes are not tracked by the broker and should be
  // still active.
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(1, exit_code);
  ASSERT_TRUE(::GetExitCodeProcess(runner2.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner2.process(), 0);
}

TEST(IntegrationTestsTest, TwoStuckChildrenFirstOneHasNoJob) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  TestRunner runner2(JOB_NONE, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner2.SetTimeout(2000);
  runner2.SetAsynchronous(true);
  runner2.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner2.RunTest(L"IntegrationTestsTest_stuck 2000"));
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the processes are not tracked by the broker and should be
  // still active.
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(1, exit_code);
  ASSERT_TRUE(::GetExitCodeProcess(runner2.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner2.process(), 0);
}

TEST(IntegrationTestsTest, MultipleStuckChildrenSequential) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  TestRunner runner2(JOB_NONE, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner2.SetTimeout(2000);
  runner2.SetAsynchronous(true);
  runner2.SetKillOnDestruction(false);

  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner2.RunTest(L"IntegrationTestsTest_stuck 2000"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());

  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(1, exit_code);
  ASSERT_TRUE(::GetExitCodeProcess(runner2.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner2.process(), 0);

  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(1, exit_code);
}

}  // namespace sandbox
