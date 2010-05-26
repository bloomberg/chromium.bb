// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "sandbox/src/win_utils.h"
#include "sandbox/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WinUtils, IsReparsePoint) {
  using sandbox::IsReparsePoint;

  // Create a temp file because we need write access to it.
  wchar_t temp_directory[MAX_PATH];
  wchar_t my_folder[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, my_folder), 0);

  // Delete the file and create a directory instead.
  ASSERT_TRUE(::DeleteFile(my_folder));
  ASSERT_TRUE(::CreateDirectory(my_folder, NULL));

  bool result = true;
  EXPECT_EQ(ERROR_SUCCESS, IsReparsePoint(my_folder, &result));
  EXPECT_FALSE(result);

  // We have to fix Bug 32224 to pass this test.
  std::wstring not_found = std::wstring(my_folder) + L"\\foo\\bar";
  // EXPECT_EQ(ERROR_PATH_NOT_FOUND, IsReparsePoint(not_found, &result));

  std::wstring new_file = std::wstring(my_folder) + L"\\foo";
  EXPECT_EQ(ERROR_SUCCESS, IsReparsePoint(new_file, &result));
  EXPECT_FALSE(result);

  // Replace the directory with a reparse point to %temp%.
  HANDLE dir = ::CreateFile(my_folder, FILE_ALL_ACCESS,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  EXPECT_TRUE(INVALID_HANDLE_VALUE != dir);

  std::wstring temp_dir_nt = std::wstring(L"\\??\\") + temp_directory;
  EXPECT_TRUE(SetReparsePoint(dir, temp_dir_nt.c_str()));

  EXPECT_EQ(ERROR_SUCCESS, IsReparsePoint(new_file, &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(DeleteReparsePoint(dir));
  EXPECT_TRUE(::CloseHandle(dir));
  EXPECT_TRUE(::RemoveDirectory(my_folder));
}
