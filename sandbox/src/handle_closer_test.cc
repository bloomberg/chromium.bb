// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/handle_closer_agent.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/target_services.h"
#include "sandbox/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

string16 GetWindowsDirHandleName() {
  // Use the Windows directory handle because every process has it open.
  char16 windows[MAX_PATH + 1];

  EXPECT_LT(GetWindowsDirectoryW(windows, MAX_PATH),
      static_cast<UINT>(MAX_PATH));
  HANDLE handle = ::CreateFile(windows, FILE_EXECUTE | SYNCHRONIZE,
                               FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  // Resolve to the fully qualified object name.
  string16 handle_name;
  EXPECT_TRUE(sandbox::GetHandleName(handle, &handle_name));
  EXPECT_TRUE(CloseHandle(handle));

  return handle_name;
}

}  // namespace

namespace sandbox {

// Checks for the presence of a named handle and type.
SBOX_TESTS_COMMAND int CheckForHandle(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  // Brute force the handle table to find what we're looking for.
  DWORD handle_count = UINT_MAX;
  const int kInvalidHandleThreshold = 100;
  const size_t kHandleOffset = sizeof(HANDLE);
  HANDLE handle = NULL;
  int invalid_count = 0;
  string16 handle_name;

  if (!::GetProcessHandleCount(::GetCurrentProcess(), &handle_count))
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  while (handle_count && invalid_count < kInvalidHandleThreshold) {
    reinterpret_cast<size_t&>(handle) += kHandleOffset;
    if (GetHandleName(handle, &handle_name)) {
      if (handle_name == argv[0])
        return SBOX_TEST_SUCCEEDED;
      --handle_count;
    } else {
      ++invalid_count;
    }
  }

  return SBOX_TEST_FAILED;
}

TEST(HandleCloserTest, CloseNothing) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(AFTER_REVERT);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Use the Windows directory handle because every process has it open.
  string16 handle_name = GetWindowsDirHandleName();
  string16 command = string16(L"CheckForHandle ") + handle_name;

  // Make sure the Make sure the Windows handle is present on first run.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.data()));
}

TEST(HandleCloserTest, CloseWindowsDirectory) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(AFTER_REVERT);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  // Use the Windows directory handle because every process has it open.
  string16 handle_name = GetWindowsDirHandleName();
  string16 command = string16(L"CheckForHandle ") + handle_name;

  // Make sure the Windows handle is closed.
  policy->AddKernelObjectToClose(L"File", handle_name.data());
  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(command.data()));
}

}  // namespace sandbox
