// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

//------------------------------------------------------------------------------
// Internal Defines & Functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ImageLoadRemote test helper function.
//
// Trigger test child process (with or without mitigation enabled).
//------------------------------------------------------------------------------
void TestWin10ImageLoadRemote(bool is_success_test) {
  // ***Insert a manual testing share UNC path here!
  // E.g.: \\\\hostname\\sharename\\calc.exe
  base::string16 unc = L"\"\\\\hostname\\sharename\\calc.exe\"";

  sandbox::TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(sandbox::JOB_NONE, 0);
  policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  if (!is_success_test) {
    // Enable the NoRemote mitigation.
    EXPECT_EQ(policy->SetDelayedProcessMitigations(
                  sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE),
              sandbox::SBOX_ALL_OK);
  }

  base::string16 test = L"TestChildProcess ";
  test += unc.c_str();
  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));
}

//------------------------------------------------------------------------------
// ImageLoadLow test helper function.
//
// 1. Set up a copy of calc, using icacls to make it low integrity.
// 2. Trigger test child process (with or without mitigation enabled).
//------------------------------------------------------------------------------
void TestWin10ImageLoadLowLabel(bool is_success_test) {
  // Setup a mandatory low executable for this test (calc.exe).
  // If anything fails during setup, ASSERT to end test.
  base::FilePath orig_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &orig_path));
  orig_path = orig_path.Append(L"calc.exe");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath new_path = temp_dir.GetPath();
  new_path = new_path.Append(L"lowIL_calc.exe");

  // Test file will be cleaned up by the ScopedTempDir.
  ASSERT_TRUE(base::CopyFileW(orig_path, new_path));

  base::string16 cmd_line = L"icacls \"";
  cmd_line += new_path.value().c_str();
  cmd_line += L"\" /setintegritylevel Low";

  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::Process setup_proc = base::LaunchProcess(cmd_line.c_str(), options);
  ASSERT_TRUE(setup_proc.IsValid());

  int exit_code = 1;
  if (!setup_proc.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                         &exit_code)) {
    // Might have timed out, or might have failed.
    // Terminate to make sure we clean up any mess.
    setup_proc.Terminate(0, false);
    ASSERT_TRUE(false);
  }
  // Make sure icacls was successful.
  ASSERT_EQ(0, exit_code);

  sandbox::TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(sandbox::JOB_NONE, 0);
  policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  if (!is_success_test) {
    // Enable the NoLowLabel mitigation.
    EXPECT_EQ(policy->SetDelayedProcessMitigations(
                  sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
              sandbox::SBOX_ALL_OK);
  }

  base::string16 test = L"TestChildProcess \"";
  test += new_path.value().c_str();
  test += L"\" false";

  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));
}

}  // namespace

namespace sandbox {

//------------------------------------------------------------------------------
// Exported Image Load Tests
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Disable image load from remote devices (MITIGATION_IMAGE_LOAD_NO_REMOTE).
// >= Win10_TH2
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoRemotePolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  base::string16 test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_LOADNOREMOTE);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_IMAGE_LOAD_NO_REMOTE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetPolicy* policy2 = runner2.GetPolicy();

  EXPECT_EQ(
      policy2->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_REMOTE),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates that we CAN create a new process from
// a remote UNC device, if the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation is NOT set.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin10ImageLoadNoRemoteSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadRemote(true);
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation prevents creating a new process from a remote
// UNC device.
//
// MANUAL testing only.
TEST(ProcessMitigationsTest, DISABLED_CheckWin10ImageLoadNoRemoteFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadRemote(false);
}

//------------------------------------------------------------------------------
// Disable image load when "mandatory low label" (integrity level).
// (MITIGATION_IMAGE_LOAD_NO_LOW_LABEL)
// >= Win10_TH2
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelPolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  base::string16 test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_LOADNOLOW);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetPolicy* policy2 = runner2.GetPolicy();

  EXPECT_EQ(
      policy2->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates that we CAN create a new process with
// low mandatory label (IL), if the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadLowLabel(true);
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation prevents creating a new process with low mandatory label (IL).
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadLowLabel(false);
}

}  // namespace sandbox
