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
#include "sandbox/win/tests/integration_tests/hijack_dlls.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

//------------------------------------------------------------------------------
// Internal Defines & Functions
//------------------------------------------------------------------------------

// hijack_shim_dll defines
using CheckHijackResultFunction = decltype(&hijack_dlls::CheckHijackResult);
constexpr char g_hijack_shim_func[] = "CheckHijackResult";

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

//------------------------------------------------------------------------------
// ImageLoadPreferSystem32 test helper function.
//
// - Acquire the global g_hijack_dlls_mutex mutex before calling
//   (as we meddle with a shared system resource).
// - Note: Do not use ASSERTs in this function, as a global mutex is held.
//
// 1. Put a copy of the hijack DLL into system32.
// 2. Trigger test child process (with or without mitigation enabled).  When
//    the OS resolves the import table for the child process, it will either
//    choose the version in the local app directory, or the copy in system32.
//------------------------------------------------------------------------------
void TestWin10ImageLoadPreferSys32(bool is_success_test) {
  // Put a copy of the hijack dll into system32.  So there's one in the
  // local dir, and one in system32.
  base::FilePath app_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &app_path));
  base::FilePath old_dll_path = app_path.Append(hijack_dlls::g_hijack_dll_file);

  base::FilePath sys_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &sys_path));
  base::FilePath new_dll_path = sys_path.Append(hijack_dlls::g_hijack_dll_file);

  // Note: test requires admin to copy/delete files in system32.
  EXPECT_TRUE(base::CopyFileW(old_dll_path, new_dll_path));
  // Do not ASSERT after this point.  Cleanup required.

  sandbox::TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // ACCESS_DENIED errors loading DLLs without a higher token - AddFsRule
  // doesn't cut it.
  policy->SetTokenLevel(sandbox::TokenLevel::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::TokenLevel::USER_LIMITED);

  if (!is_success_test) {
    // Enable the PreferSystem32 mitigation.
    EXPECT_EQ(policy->SetDelayedProcessMitigations(
                  sandbox::MITIGATION_IMAGE_LOAD_PREFER_SYS32),
              sandbox::SBOX_ALL_OK);
  }

  // Get path for the test shim DLL to pass along.
  base::FilePath shim_dll_path =
      app_path.Append(hijack_dlls::g_hijack_shim_dll_file);

  // If this is the "success" test, expect a return value of "false" - as the
  // hijack was successful so the hijack_dll is NOT in system32.
  // The failure case has the mitigation enabled, so expect the hijack_dll to be
  // in system32.
  base::string16 test = base::StringPrintf(
      L"%ls %ls \"%ls\"", L"TestImageLoadHijack",
      (!is_success_test) ? L"true" : L"false", shim_dll_path.value().c_str());

  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));

  EXPECT_TRUE(base::DeleteFileW(new_dll_path, false));
}

}  // namespace

namespace sandbox {

//------------------------------------------------------------------------------
// Exported functions called by child test processes.
//------------------------------------------------------------------------------

// This test loading and using the shim DLL is required.
// The waterfall bots (test environment/harness) have unique resource
// access failures if the main sbox_integration_tests executable
// implicitely links against the hijack DLL (and implicit linking is required
// to test this mitigation) - regardless of whether this test runs or is
// disabled.
//
// - Arg1: "true" or "false", if the DLL path should be in system32.
// - Arg2: the full path to the test shim DLL to load.
SBOX_TESTS_COMMAND int TestImageLoadHijack(int argc, wchar_t** argv) {
  if (argc < 2 || argv[0] == nullptr || argv[1] == nullptr)
    return SBOX_TEST_INVALID_PARAMETER;

  bool expect_system = false;
  if (::wcsicmp(argv[0], L"true") == 0)
    expect_system = true;

  // Load the shim DLL for this test.
  base::ScopedNativeLibrary shim_dll((base::FilePath(argv[1])));
  if (!shim_dll.is_valid())
    return SBOX_TEST_NOT_FOUND;

  CheckHijackResultFunction check_hijack_result =
      reinterpret_cast<CheckHijackResultFunction>(
          shim_dll.GetFunctionPointer(g_hijack_shim_func));

  if (check_hijack_result == nullptr)
    return SBOX_TEST_NOT_FOUND;

  return check_hijack_result(expect_system);
}

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

//------------------------------------------------------------------------------
// Prefer system32 directory on image load (MITIGATION_IMAGE_LOAD_PREFER_SYS32).
// >= Win10_RS1 (Anniversary)
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_IMAGE_LOAD_PREFER_SYS32
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadPreferSys32PolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS1)
    return;

  base::string16 test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_LOADPREFERSYS32);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_IMAGE_LOAD_PREFER_SYS32),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetPolicy* policy2 = runner2.GetPolicy();

  EXPECT_EQ(
      policy2->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_PREFER_SYS32),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates that import hijacking succeeds, if the
// MITIGATION_IMAGE_LOAD_PREFER_SYS32 mitigation is NOT set.
//
// Must run this test as admin/elevated.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadPreferSys32_Success) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS1)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, hijack_dlls::g_hijack_dlls_mutex);
  EXPECT_TRUE(mutex != NULL);
  EXPECT_EQ(WAIT_OBJECT_0,
            ::WaitForSingleObject(mutex, SboxTestEventTimeout()));

  // Expect the DLL to be in system32.
  TestWin10ImageLoadPreferSys32(true);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_PREFER_SYS32
// mitigation prevents import hijacking.
//
// Must run this test as admin/elevated.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadPreferSys32_Failure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS1)
    return;

  HANDLE mutex = ::CreateMutexW(NULL, FALSE, hijack_dlls::g_hijack_dlls_mutex);
  EXPECT_TRUE(mutex != NULL);
  EXPECT_EQ(WAIT_OBJECT_0,
            ::WaitForSingleObject(mutex, SboxTestEventTimeout()));

  // Expect the DLL to NOT be in system32.
  TestWin10ImageLoadPreferSys32(false);

  EXPECT_TRUE(::ReleaseMutex(mutex));
  EXPECT_TRUE(::CloseHandle(mutex));
}

}  // namespace sandbox
