// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "sandbox/src/handle_closer_agent.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/target_services.h"
#include "sandbox/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns a handle to a unique marker file that can be retrieved between runs.
HANDLE GetMarkerFile() {
  wchar_t path_buffer[MAX_PATH + 1];
  CHECK(::GetTempPath(MAX_PATH, path_buffer));
  string16 marker_path = path_buffer;
  marker_path += L"\\sbox_marker_";

  // Generate a unique value from the exe's size and timestamp.
  CHECK(::GetModuleFileName(NULL, path_buffer, MAX_PATH));
  base::win::ScopedHandle module(::CreateFile(path_buffer,
                                 FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  CHECK(module.IsValid());
  FILETIME timestamp;
  CHECK(::GetFileTime(module, &timestamp, NULL, NULL));
  marker_path += base::StringPrintf(L"%08x%08x%08x",
                                    ::GetFileSize(module, NULL),
                                    timestamp.dwLowDateTime,
                                    timestamp.dwHighDateTime);

  // Make the file delete-on-close so cleanup is automatic.
  return CreateFile(marker_path.c_str(), FILE_ALL_ACCESS,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL, OPEN_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
}

}  // namespace

namespace sandbox {

// Checks for the presence of a file (in object path form).
// Format: CheckForFileHandle \path\to\file (Y|N)
// - Y or N depending if the file should exist or not.
SBOX_TESTS_COMMAND int CheckForFileHandle(int argc, wchar_t **argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  bool should_find = argv[1][0] == L'Y';
  if (argv[1][1] != L'\0' || !should_find && argv[1][0] != L'N')
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  static int state = BEFORE_INIT;
  switch (state++) {
    case BEFORE_INIT:
      // Create a unique marker file that is open while the test is running.
      // The handle leaks, but it will be closed by the test or on exit.
      EXPECT_NE(GetMarkerFile(), INVALID_HANDLE_VALUE);
      return SBOX_TEST_SUCCEEDED;

    case AFTER_REVERT: {
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
            return should_find ? SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
          --handle_count;
        } else {
          ++invalid_count;
        }
      }

      return should_find ? SBOX_TEST_FAILED : SBOX_TEST_SUCCEEDED;
    }
  }

  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleCloserTest, CheckForMarkerFile) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  base::win::ScopedHandle marker(GetMarkerFile());
  EXPECT_TRUE(marker.IsValid());
  string16 handle_name;
  EXPECT_TRUE(sandbox::GetHandleName(marker, &handle_name));
  string16 command = string16(L"CheckForFileHandle ") + handle_name + L" Y";
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.c_str())) <<
    "Could not find: " << handle_name;
}

TEST(HandleCloserTest, CloseMarkerFile) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  sandbox::TargetPolicy* policy = runner.GetPolicy();

  base::win::ScopedHandle marker(GetMarkerFile());
  EXPECT_TRUE(marker.IsValid());
  string16 handle_name;
  EXPECT_TRUE(sandbox::GetHandleName(marker, &handle_name));
  policy->AddKernelObjectToClose(L"File", handle_name.c_str());
  string16 command = string16(L"CheckForFileHandle ") + handle_name + L" N";
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(command.c_str())) <<
    "Failed closing: " << handle_name;
}

}  // namespace sandbox
