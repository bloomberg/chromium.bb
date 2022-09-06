// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <winternl.h>

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/handle_hooks_win.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace base {
namespace win {

namespace {

std::string FailureMessage(const std::string& msg) {
#if !defined(DEBUG) && defined(OFFICIAL_BUILD)
  // Official release builds strip all fatal messages for saving binary size,
  // see base/check.h.
  return "";
#else
  return msg;
#endif  // !defined(DEBUG) && defined(OFFICIAL_BUILD)
}

}  // namespace

namespace testing {
extern "C" bool __declspec(dllexport) RunTest();
}  // namespace testing

class ScopedHandleTest : public ::testing::Test,
                         public ::testing::WithParamInterface<bool> {
 public:
  ScopedHandleTest(const ScopedHandleTest&) = delete;
  ScopedHandleTest& operator=(const ScopedHandleTest&) = delete;

 protected:
  ScopedHandleTest() {
    if (HooksEnabled()) {
#if defined(ARCH_CPU_32_BITS)
      // EAT patch is only supported on 32-bit.
      base::debug::HandleHooks::AddEATPatch();
#endif
      base::debug::HandleHooks::PatchLoadedModules();
    }
  }

  static bool HooksEnabled() { return GetParam(); }
  static bool DoDeathTestsWork() {
    // Death tests don't seem to work on Windows 7 32-bit native with hooks
    // enabled.
    // TODO(crbug.com/1328022): Investigate why.
    if (!HooksEnabled())
      return true;
#if defined(ARCH_CPU_32_BITS)
    const auto* os_info = base::win::OSInfo::GetInstance();
    if (os_info->version() <= base::win::Version::WIN7 &&
        os_info->IsWowDisabled()) {
      return false;
    }
#endif  // defined(ARCH_CPU_32_BITS)
    return true;
  }
};

using ScopedHandleDeathTest = ScopedHandleTest;

TEST_P(ScopedHandleTest, ScopedHandle) {
  // Any illegal error code will do. We just need to test that it is preserved
  // by ScopedHandle to avoid https://crbug.com/528394.
  const DWORD magic_error = 0x12345678;

  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  // Call SetLastError after creating the handle.
  ::SetLastError(magic_error);
  base::win::ScopedHandle handle_holder(handle);
  EXPECT_EQ(magic_error, ::GetLastError());

  // Create a new handle and then set LastError again.
  handle = ::CreateMutex(nullptr, false, nullptr);
  ::SetLastError(magic_error);
  handle_holder.Set(handle);
  EXPECT_EQ(magic_error, ::GetLastError());

  // Create a new handle and then set LastError again.
  handle = ::CreateMutex(nullptr, false, nullptr);
  base::win::ScopedHandle handle_source(handle);
  ::SetLastError(magic_error);
  handle_holder = std::move(handle_source);
  EXPECT_EQ(magic_error, ::GetLastError());
}

TEST_P(ScopedHandleDeathTest, HandleVerifierTrackedHasBeenClosed) {
  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);
  using NtCloseFunc = decltype(&::NtClose);
  NtCloseFunc ntclose = reinterpret_cast<NtCloseFunc>(
      GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtClose"));
  ASSERT_NE(nullptr, ntclose);

  ASSERT_DEATH(
      {
        base::win::ScopedHandle handle_holder(handle);
        ntclose(handle);
        // Destructing a ScopedHandle with an illegally closed handle should
        // fail.
      },
      FailureMessage("CloseHandle failed"));
}

TEST_P(ScopedHandleDeathTest, HandleVerifierCloseTrackedHandle) {
  // This test is only valid if hooks are enabled.
  if (!HooksEnabled())
    return;
  if (!DoDeathTestsWork())
    return;

  ASSERT_DEATH(
      {
        HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
        ASSERT_NE(HANDLE(nullptr), handle);

        // Start tracking the handle so that closes outside of the checker are
        // caught.
        base::win::CheckedScopedHandle handle_holder(handle);

        // Closing a tracked handle using ::CloseHandle should crash due to hook
        // noticing the illegal close.
        ::CloseHandle(handle);
      },
      // This test must match the CloseHandleHook causing this failure, because
      // if the hook doesn't crash and instead the handle is double closed by
      // the `handle_holder` going out of scope, then there is still a crash,
      // but a different crash and one we are not explicitly testing here. This
      // other crash is tested in HandleVerifierTrackedHasBeenClosed above.
      FailureMessage("CloseHandleHook validation failure"));
}

TEST_P(ScopedHandleDeathTest, HandleVerifierDoubleTracking) {
  if (!DoDeathTestsWork())
    return;

  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  base::win::CheckedScopedHandle handle_holder(handle);

  ASSERT_DEATH({ base::win::CheckedScopedHandle handle_holder2(handle); },
               FailureMessage("Handle Already Tracked"));
}

TEST_P(ScopedHandleDeathTest, HandleVerifierWrongOwner) {
  if (!DoDeathTestsWork())
    return;

  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  base::win::CheckedScopedHandle handle_holder(handle);
  ASSERT_DEATH(
      {
        base::win::CheckedScopedHandle handle_holder2;
        handle_holder2.handle_ = handle;
      },
      FailureMessage("Closing a handle owned by something else"));
  ASSERT_TRUE(handle_holder.is_valid());
  handle_holder.Close();
}

TEST_P(ScopedHandleDeathTest, HandleVerifierUntrackedHandle) {
  if (!DoDeathTestsWork())
    return;

  HANDLE handle = ::CreateMutex(nullptr, false, nullptr);
  ASSERT_NE(HANDLE(nullptr), handle);

  ASSERT_DEATH(
      {
        base::win::CheckedScopedHandle handle_holder;
        handle_holder.handle_ = handle;
      },
      FailureMessage("Closing an untracked handle"));

  ASSERT_TRUE(::CloseHandle(handle));
}

// Under ASan, the multi-process test crashes during process shutdown for
// unknown reasons. Disable it for now. http://crbug.com/685262
#if defined(ADDRESS_SANITIZER)
#define MAYBE_MultiProcess DISABLED_MultiProcess
#else
#define MAYBE_MultiProcess MultiProcess
#endif

TEST_P(ScopedHandleTest, MAYBE_MultiProcess) {
  // Initializing ICU in the child process causes a scoped handle to be created
  // before the test gets a chance to test the race condition, so disable ICU
  // for the child process here.
  CommandLine command_line(base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitch(switches::kTestDoNotInitializeIcu);

  base::Process test_child_process = base::SpawnMultiProcessTestChild(
      "HandleVerifierChildProcess", command_line, LaunchOptions());

  int rv = -1;
  ASSERT_TRUE(test_child_process.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(HandleVerifierChildProcess) {
  ScopedNativeLibrary module(
      FilePath(FILE_PATH_LITERAL("scoped_handle_test_dll.dll")));

  if (!module.is_valid())
    return 1;
  auto run_test_function = reinterpret_cast<decltype(&testing::RunTest)>(
      module.GetFunctionPointer("RunTest"));
  if (!run_test_function)
    return 1;
  if (!run_test_function())
    return 1;

  return 0;
}

INSTANTIATE_TEST_SUITE_P(HooksEnabled,
                         ScopedHandleTest,
                         ::testing::Values(true));
INSTANTIATE_TEST_SUITE_P(HooksDisabled,
                         ScopedHandleTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(HooksEnabled,
                         ScopedHandleDeathTest,
                         ::testing::Values(true));
INSTANTIATE_TEST_SUITE_P(HooksDisabled,
                         ScopedHandleDeathTest,
                         ::testing::Values(false));

}  // namespace win
}  // namespace base
