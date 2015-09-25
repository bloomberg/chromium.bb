// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int HandleInheritanceTests_PrintToStdout(int argc,
                                                            wchar_t** argv) {
  printf("Example output to stdout\n");
  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleInheritanceTests, TestStdoutInheritance) {
  base::ScopedTempDir temp_directory;
  base::FilePath temp_file_name;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  ASSERT_TRUE(CreateTemporaryFileInDir(temp_directory.path(), &temp_file_name));

  SECURITY_ATTRIBUTES attrs = {};
  attrs.nLength = sizeof(attrs);
  attrs.bInheritHandle = TRUE;
  base::win::ScopedHandle tmp_handle(
      CreateFile(temp_file_name.value().c_str(), GENERIC_WRITE,
                 FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                 &attrs, OPEN_EXISTING, 0, NULL));
  ASSERT_TRUE(tmp_handle.IsValid());

  TestRunner runner;
  ASSERT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetStdoutHandle(tmp_handle.Get()));
  int result = runner.RunTest(L"HandleInheritanceTests_PrintToStdout");
  ASSERT_EQ(SBOX_TEST_SUCCEEDED, result);

  std::string data;
  ASSERT_TRUE(base::ReadFileToString(base::FilePath(temp_file_name), &data));
  // Redirection uses a feature that was added in Windows Vista.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    ASSERT_EQ("Example output to stdout\r\n", data);
  } else {
    ASSERT_EQ("", data);
  }
}

SBOX_TESTS_COMMAND int
HandleInheritanceTests_ValidInheritedHandle(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  HANDLE handle = nullptr;
  base::StringToUint(argv[0], reinterpret_cast<unsigned int *>(&handle));

  // This handle we inherited must be both valid and closeable.
  DWORD flags = 0;
  if (!GetHandleInformation(handle, &flags))
    return SBOX_TEST_FAILED;
  if ((flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) != 0)
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleInheritanceTests, InheritByValue) {
  // Handle inheritance doesn't work on XP.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;

  base::SharedMemory test_shared_memory;
  ASSERT_TRUE(test_shared_memory.CreateAnonymous(1000));
  ASSERT_TRUE(test_shared_memory.Map(0));

  TestRunner runner;
  void* shared_handle = runner.GetPolicy()->AddHandleToShare(
      test_shared_memory.handle().GetHandle());

  std::string command_line =
      "HandleInheritanceTests_ValidInheritedHandle " +
      base::UintToString(reinterpret_cast<unsigned int>(shared_handle));
  int result = runner.RunTest(base::UTF8ToUTF16(command_line).c_str());
  ASSERT_EQ(SBOX_TEST_SUCCEEDED, result);
}

}  // namespace sandbox
