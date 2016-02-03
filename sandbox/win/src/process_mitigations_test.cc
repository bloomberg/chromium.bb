// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// API defined in winbase.h.
typedef decltype(GetProcessDEPPolicy)* GetProcessDEPPolicyFunction;

// API defined in processthreadsapi.h.
typedef decltype(
    GetProcessMitigationPolicy)* GetProcessMitigationPolicyFunction;
GetProcessMitigationPolicyFunction get_process_mitigation_policy;

// APIs defined in wingdi.h.
typedef decltype(AddFontMemResourceEx)* AddFontMemResourceExFunction;
typedef decltype(RemoveFontMemResourceEx)* RemoveFontMemResourceExFunction;

#if !defined(_WIN64)
bool CheckWin8DepPolicy() {
  PROCESS_MITIGATION_DEP_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(), ProcessDEPPolicy,
                                     &policy, sizeof(policy))) {
    return false;
  }
  return policy.Enable && policy.Permanent;
}
#endif  // !defined(_WIN64)

#if defined(NDEBUG)
bool CheckWin8AslrPolicy() {
  PROCESS_MITIGATION_ASLR_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(), ProcessASLRPolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.EnableForceRelocateImages && policy.DisallowStrippedImages;
}
#endif  // defined(NDEBUG)

bool CheckWin8StrictHandlePolicy() {
  PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessStrictHandleCheckPolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.RaiseExceptionOnInvalidHandleReference &&
         policy.HandleExceptionsPermanentlyEnabled;
}

bool CheckWin8Win32CallPolicy() {
  PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessSystemCallDisablePolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.DisallowWin32kSystemCalls;
}

bool CheckWin8DllExtensionPolicy() {
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessExtensionPointDisablePolicy,
                                     &policy, sizeof(policy))) {
     return false;
  }
  return policy.DisableExtensionPoints;
}

bool CheckWin10FontPolicy() {
  PROCESS_MITIGATION_FONT_DISABLE_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessFontDisablePolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.DisableNonSystemFonts;
}

bool CheckWin10ImageLoadNoRemotePolicy() {
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessImageLoadPolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.NoRemoteImages;
}

void TestWin10ImageLoadRemote(bool is_success_test) {
  // ***Insert your manual testing share UNC path here!
  // E.g.: \\\\hostname\\sharename\\calc.exe
  std::wstring unc = L"\"\\\\hostname\\sharename\\calc.exe\"";

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

  std::wstring test = L"TestChildProcess ";
  test += unc.c_str();
  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));
}

bool CheckWin10ImageLoadNoLowLabelPolicy() {
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
  if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                     ProcessImageLoadPolicy, &policy,
                                     sizeof(policy))) {
    return false;
  }
  return policy.NoLowMandatoryLabelImages;
}

void TestWin10ImageLoadLowLabel(bool is_success_test) {
  // Setup a mandatory low executable for this test (calc.exe).
  // If anything fails during setup, ASSERT to end test.
  base::FilePath orig_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &orig_path));
  orig_path = orig_path.Append(L"calc.exe");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath new_path = temp_dir.path();
  new_path = new_path.Append(L"lowIL_calc.exe");

  // Test file will be cleaned up by the ScopedTempDir.
  ASSERT_TRUE(base::CopyFileW(orig_path, new_path));

  std::wstring cmd_line = L"icacls \"";
  cmd_line += new_path.value().c_str();
  cmd_line += L"\" /setintegritylevel Low";

  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::Process setup_proc = base::LaunchProcess(cmd_line.c_str(), options);
  ASSERT_TRUE(setup_proc.IsValid());

  int exit_code = 1;
  if (!setup_proc.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(10),
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

  std::wstring test = L"TestChildProcess ";
  test += new_path.value().c_str();

  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test.c_str()));
}

}  // namespace

namespace sandbox {

// A shared helper test command that will attempt to CreateProcess
// with a given command line.
//
// ***Make sure you've enabled basic process creation in the
// test sandbox settings via:
// sandbox::TargetPolicy::SetJobLevel(),
// sandbox::TargetPolicy::SetTokenLevel(),
// and TestRunner::SetDisableCsrss().
SBOX_TESTS_COMMAND int TestChildProcess(int argc, wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_INVALID_PARAMETER;

  std::wstring cmd = argv[0];
  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::Process setup_proc = base::LaunchProcess(cmd.c_str(), options);

  if (setup_proc.IsValid()) {
    setup_proc.Terminate(0, false);
    return SBOX_TEST_SUCCEEDED;
  }
  // Note: GetLastError from CreateProcess returns 5, "ERROR_ACCESS_DENIED".
  return SBOX_TEST_FAILED;
}

//------------------------------------------------------------------------------
// Win8 Checks:
// MITIGATION_DEP(_NO_ATL_THUNK)
// MITIGATION_EXTENSION_DLL_DISABLE
// MITIGATION_RELOCATE_IMAGE(_REQUIRED) - ASLR, release only
// MITIGATION_STRICT_HANDLE_CHECKS
// >= Win8
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin8(int argc, wchar_t **argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(
          ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                           "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
  if (!CheckWin8DepPolicy())
    return SBOX_TEST_FIRST_ERROR;
#endif

#if defined(NDEBUG)  // ASLR cannot be forced in debug builds.
  if (!CheckWin8AslrPolicy())
    return SBOX_TEST_SECOND_ERROR;
#endif

  if (!CheckWin8StrictHandlePolicy())
    return SBOX_TEST_THIRD_ERROR;

  if (!CheckWin8DllExtensionPolicy())
    return SBOX_TEST_FIFTH_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

TEST(ProcessMitigationsTest, CheckWin8) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  sandbox::MitigationFlags mitigations = MITIGATION_DEP |
                                         MITIGATION_DEP_NO_ATL_THUNK |
                                         MITIGATION_EXTENSION_DLL_DISABLE;
#if defined(NDEBUG)  // ASLR cannot be forced in debug builds.
  mitigations |= MITIGATION_RELOCATE_IMAGE |
                 MITIGATION_RELOCATE_IMAGE_REQUIRED;
#endif

  EXPECT_EQ(policy->SetProcessMitigations(mitigations), SBOX_ALL_OK);

  mitigations |= MITIGATION_STRICT_HANDLE_CHECKS;

  EXPECT_EQ(policy->SetDelayedProcessMitigations(mitigations), SBOX_ALL_OK);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8"));
}

//------------------------------------------------------------------------------
// DEP (MITIGATION_DEP)
// < Win8 x86
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckDep(int argc, wchar_t **argv) {
  GetProcessDEPPolicyFunction get_process_dep_policy =
      reinterpret_cast<GetProcessDEPPolicyFunction>(
          ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                           "GetProcessDEPPolicy"));
  if (get_process_dep_policy) {
    BOOL is_permanent = FALSE;
    DWORD dep_flags = 0;

    if (!get_process_dep_policy(::GetCurrentProcess(), &dep_flags,
        &is_permanent)) {
      return SBOX_TEST_FIRST_ERROR;
    }

    if (!(dep_flags & PROCESS_DEP_ENABLE) || !is_permanent)
      return SBOX_TEST_SECOND_ERROR;

  } else {
    NtQueryInformationProcessFunction query_information_process = NULL;
    ResolveNTFunctionPtr("NtQueryInformationProcess",
                          &query_information_process);
    if (!query_information_process)
      return SBOX_TEST_NOT_FOUND;

    ULONG size = 0;
    ULONG dep_flags = 0;
    if (!SUCCEEDED(query_information_process(::GetCurrentProcess(),
                                             ProcessExecuteFlags, &dep_flags,
                                             sizeof(dep_flags), &size))) {
      return SBOX_TEST_THIRD_ERROR;
    }

    static const int MEM_EXECUTE_OPTION_DISABLE = 2;
    static const int MEM_EXECUTE_OPTION_PERMANENT = 8;
    dep_flags &= 0xff;

    if (dep_flags != (MEM_EXECUTE_OPTION_DISABLE |
                      MEM_EXECUTE_OPTION_PERMANENT)) {
      return SBOX_TEST_FOURTH_ERROR;
    }
  }

  return SBOX_TEST_SUCCEEDED;
}

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
TEST(ProcessMitigationsTest, CheckDep) {
  if (base::win::GetVersion() > base::win::VERSION_WIN7)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(
                MITIGATION_DEP |
                MITIGATION_DEP_NO_ATL_THUNK |
                MITIGATION_SEHOP),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDep"));
}
#endif

//------------------------------------------------------------------------------
// Win32k Lockdown (MITIGATION_WIN32K_DISABLE)
// >= Win8
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin8Lockdown(int argc, wchar_t **argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(
          ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"),
                           "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin8Win32CallPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation on
// the target process causes the launch to fail in process initialization.
// The test process itself links against user32/gdi32.
TEST(ProcessMitigationsTest, CheckWin8Win32KLockDownFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_NE(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8Lockdown"));
}

// This test validates that setting the MITIGATION_WIN32K_DISABLE mitigation
// along with the policy to fake user32 and gdi32 initialization successfully
// launches the target process.
// The test process itself links against user32/gdi32.
TEST(ProcessMitigationsTest, CheckWin8Win32KLockDownSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_WIN32K_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(policy->AddRule(sandbox::TargetPolicy::SUBSYS_WIN32K_LOCKDOWN,
                            sandbox::TargetPolicy::FAKE_USER_GDI_INIT, NULL),
            sandbox::SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin8Lockdown"));
}

//------------------------------------------------------------------------------
// Disable non-system font loads (MITIGATION_NONSYSTEM_FONT_DISABLE)
// >= Win10
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin10FontLockDown(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin10FontPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int CheckWin10FontLoad(int argc, wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_INVALID_PARAMETER;

  HMODULE gdi_module = ::LoadLibraryW(L"gdi32.dll");
  if (!gdi_module)
    return SBOX_TEST_NOT_FOUND;

  AddFontMemResourceExFunction add_font_mem_resource =
      reinterpret_cast<AddFontMemResourceExFunction>(
          ::GetProcAddress(gdi_module, "AddFontMemResourceEx"));

  RemoveFontMemResourceExFunction rem_font_mem_resource =
      reinterpret_cast<RemoveFontMemResourceExFunction>(
          ::GetProcAddress(gdi_module, "RemoveFontMemResourceEx"));

  if (!add_font_mem_resource || !rem_font_mem_resource)
    return SBOX_TEST_NOT_FOUND;

  // Open font file passed in as an argument.
  base::File file(base::FilePath(argv[0]),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    // Failed to open the font file passed in.
    return SBOX_TEST_NOT_FOUND;

  std::vector<char> font_data;
  int64_t len = file.GetLength();
  font_data.resize(len);

  int read = file.Read(0, &font_data[0], len);
  file.Close();

  if (read != len)
    return SBOX_TEST_NOT_FOUND;

  DWORD font_count = 0;
  HANDLE font_handle = add_font_mem_resource(
      &font_data[0], static_cast<DWORD>(font_data.size()), NULL, &font_count);

  if (font_handle) {
    rem_font_mem_resource(font_handle);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_FAILED;
}

// This test validates that setting the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownPolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_NONSYSTEM_FONT_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckWin10FontLockDown"));
}

// This test validates that we can load a non-system font
// if the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownLoadSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  base::FilePath font_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_WINDOWS_FONTS, &font_path));
  // Arial font should always be available
  font_path = font_path.Append(L"arial.ttf");

  TestRunner runner;
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               font_path.value().c_str()));

  std::wstring test_command = L"CheckWin10FontLoad \"";
  test_command += font_path.value().c_str();
  test_command += L"\"";
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates that setting the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation prevents the loading of a non-system font.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownLoadFailure) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  base::FilePath font_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_WINDOWS_FONTS, &font_path));
  // Arial font should always be available
  font_path = font_path.Append(L"arial.ttf");

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               font_path.value().c_str()));

  // Turn on the non-system font disable mitigation.
  EXPECT_EQ(policy->SetProcessMitigations(MITIGATION_NONSYSTEM_FONT_DISABLE),
            SBOX_ALL_OK);

  std::wstring test_command = L"CheckWin10FontLoad \"";
  test_command += font_path.value().c_str();
  test_command += L"\"";

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(test_command.c_str()));
}

//------------------------------------------------------------------------------
// Disable image load from remote devices (MITIGATION_IMAGE_LOAD_NO_REMOTE).
// >= Win10_TH2
//------------------------------------------------------------------------------

SBOX_TESTS_COMMAND int CheckWin10ImageLoadNoRemote(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin10ImageLoadNoRemotePolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoRemotePolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(
      policy->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_REMOTE),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin10ImageLoadNoRemote"));
}

// This test validates that we CAN create a new process from
// a remote UNC device, if the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation is NOT set.
//
// DISABLED for automated testing bots.  Enable for manual testing.
TEST(ProcessMitigationsTest, DISABLED_CheckWin10ImageLoadNoRemoteSuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestWin10ImageLoadRemote(true);
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_REMOTE
// mitigation prevents creating a new process from a remote
// UNC device.
//
// DISABLED for automated testing bots.  Enable for manual testing.
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

SBOX_TESTS_COMMAND int CheckWin10ImageLoadNoLowLabel(int argc, wchar_t** argv) {
  get_process_mitigation_policy =
      reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
  if (!get_process_mitigation_policy)
    return SBOX_TEST_NOT_FOUND;

  if (!CheckWin10ImageLoadNoLowLabelPolicy())
    return SBOX_TEST_FIRST_ERROR;
  return SBOX_TEST_SUCCEEDED;
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelPolicySuccess) {
  if (base::win::GetVersion() < base::win::VERSION_WIN10_TH2)
    return;

  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  EXPECT_EQ(
      policy->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CheckWin10ImageLoadNoLowLabel"));
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
// Disable child process creation.
// - JobLevel <= JOB_LIMITED_USER (on < WIN10_TH2).
// - JobLevel <= JOB_LIMITED_USER which also triggers setting
//   PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY to
//   PROCESS_CREATION_CHILD_PROCESS_RESTRICTED in
//   BrokerServicesBase::SpawnTarget (on >= WIN10_TH2).
//------------------------------------------------------------------------------

// This test validates that we can spawn a child process if
// MITIGATION_CHILD_PROCESS_CREATION_RESTRICTED mitigation is
// not set.
TEST(ProcessMitigationsTest, CheckChildProcessSuccess) {
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Set a policy that would normally allow for process creation.
  policy->SetJobLevel(JOB_INTERACTIVE, 0);
  policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess ";
  test_command += cmd.value().c_str();

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates that setting the
// MITIGATION_CHILD_PROCESS_CREATION_RESTRICTED mitigation prevents
// the spawning of child processes.
TEST(ProcessMitigationsTest, CheckChildProcessFailure) {
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Now set the job level to be <= JOB_LIMITED_USER
  // and ensure we can no longer create a child process.
  policy->SetJobLevel(JOB_LIMITED_USER, 0);
  policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess ";
  test_command += cmd.value().c_str();

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(test_command.c_str()));
}

}  // namespace sandbox

