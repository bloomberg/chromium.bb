// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <atlsecurity.h>

#include "base/process/process_info.h"
#include "base/win/access_token.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sandbox {

SBOX_TESTS_COMMAND int CheckUntrustedIntegrityLevel(int argc, wchar_t** argv) {
  return (base::GetCurrentProcessIntegrityLevel() == base::UNTRUSTED_INTEGRITY)
             ? SBOX_TEST_SUCCEEDED
             : SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int CheckLowIntegrityLevel(int argc, wchar_t** argv) {
  return (base::GetCurrentProcessIntegrityLevel() == base::LOW_INTEGRITY)
             ? SBOX_TEST_SUCCEEDED
             : SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int CheckIntegrityLevel(int argc, wchar_t** argv) {
  absl::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromEffective();
  if (!token)
    return SBOX_TEST_FAILED;

  if (token->IntegrityLevel() == SECURITY_MANDATORY_LOW_RID)
    return SBOX_TEST_SUCCEEDED;

  return SBOX_TEST_DENIED;
}

TEST(IntegrityLevelTest, TestLowILReal) {
  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  runner.GetPolicy()->SetAlternateDesktop(true);
  runner.GetPolicy()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(DelayedIntegrityLevelTest, TestLowILDelayed) {
  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  runner.GetPolicy()->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestNoILChange) {
  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestUntrustedIL) {
  TestRunner runner(JOB_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner.GetPolicy()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  runner.GetPolicy()->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
  runner.GetPolicy()->SetLockdownDefaultDacl();

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckUntrustedIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestLowIL) {
  TestRunner runner(JOB_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner.GetPolicy()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);
  runner.GetPolicy()->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);
  runner.GetPolicy()->SetLockdownDefaultDacl();

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckLowIntegrityLevel"));
}

}  // namespace sandbox
