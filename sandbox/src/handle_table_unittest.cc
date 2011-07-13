// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "sandbox/src/handle_table.h"
#include "sandbox/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(HandleTable, IsTableValid) {
  using sandbox::HandleTable;
  const ULONG my_process_id = ::GetCurrentProcessId();
  ULONG last_process_id = 0;
  ULONG total_handles = 0;
  bool found_my_process = false;
  bool found_other_process = false;

  HandleTable handles;
  EXPECT_NE(0u, handles.handle_info()->NumberOfHandles);

  for (HandleTable::Iterator it = handles.begin(); it != handles.end(); ++it) {
    ULONG process_id = it->handle_entry()->ProcessId;
    bool is_current_process = process_id == my_process_id;
    found_my_process |= is_current_process;
    found_other_process |= !is_current_process;

    EXPECT_LE(last_process_id, process_id);
    last_process_id = process_id;
    total_handles++;
  }

  EXPECT_EQ(handles.handle_info()->NumberOfHandles, total_handles);
  EXPECT_TRUE(found_my_process);
  EXPECT_TRUE(found_other_process);
}

TEST(HandleTable, FindHandle) {
  using sandbox::HandleTable;

  // Create a temp file so we have a handle to find.
  wchar_t temp_directory[MAX_PATH];
  wchar_t my_file[MAX_PATH];
  ASSERT_NE(::GetTempPath(MAX_PATH, temp_directory), 0u);
  ASSERT_NE(::GetTempFileName(temp_directory, L"test", 0, my_file), 0u);
  HANDLE file = ::CreateFile(my_file, FILE_ALL_ACCESS,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
  EXPECT_NE(INVALID_HANDLE_VALUE, file);

  // Look for the handle in our process
  bool handle_found = false;
  HandleTable handles;
  for (HandleTable::Iterator it =
      handles.HandlesForProcess(::GetCurrentProcessId());
      it != handles.end(); ++it) {
    if (it->IsType(HandleTable::kTypeFile) && it->Name().compare(my_file)) {
      handle_found = true;
      break;
    }
  }
  EXPECT_TRUE(handle_found);

  // Clean up the file we made.
  EXPECT_TRUE(::CloseHandle(file));
}
