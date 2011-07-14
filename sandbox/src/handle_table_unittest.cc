// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "sandbox/src/handle_table.h"
#include "sandbox/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Flaky on the Vista bots: crbug.com/89325
TEST(HandleTable, FLAKY_IsTableValid) {
  using sandbox::HandleTable;
  const ULONG my_process_id = ::GetCurrentProcessId();
  ULONG last_process_id = 0;
  USHORT last_handle = -1;
  ULONG total_handles = 0;
  bool found_my_process = false;
  bool found_other_process = false;

  HandleTable handles;
  // Verify that we have some number of handles.
  EXPECT_NE(0u, handles.handle_info()->NumberOfHandles);
  // Verify that an invalid process ID returns no handles.
  EXPECT_TRUE(handles.HandlesForProcess(-1) == handles.end());

  for (HandleTable::Iterator it = handles.begin(); it != handles.end(); ++it) {
    ULONG process_id = it->handle_entry()->ProcessId;
    bool is_current_process = process_id == my_process_id;
    found_my_process |= is_current_process;
    found_other_process |= !is_current_process;

    // Duplicate handles in a process mean a corrupt table.
    if (last_process_id == process_id) {
      EXPECT_NE(last_handle, it->handle_entry()->Handle);
      if (last_handle == it->handle_entry()->Handle)
        return;  // Just bail rather than spew errors.
    }

    // Verify the sort order.
    EXPECT_LE(last_process_id, process_id);
    last_process_id = process_id;
    total_handles++;
  }

  EXPECT_EQ(handles.handle_info()->NumberOfHandles, total_handles);
  EXPECT_TRUE(found_my_process);
  EXPECT_TRUE(found_other_process);
}

// Flaky on the Vista bots: crbug.com/89325
TEST(HandleTable, FLAKY_FindHandle) {
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
  string16 handle_name;
  ASSERT_NE(sandbox::GetHandleName(file, &handle_name), FALSE);

  // Look for the handle in our process.
  int total_handles = 0;
  int type_handles = 0;
  bool handle_found = false;
  HandleTable handles;
  for (HandleTable::Iterator it =
      handles.HandlesForProcess(::GetCurrentProcessId());
      it != handles.end(); ++it) {
    ++total_handles;
    if (it->IsType(HandleTable::kTypeFile)) {
      ++type_handles;
      if (it->Name() == handle_name) {
        handle_found = true;
        break;
      }
    }
  }

  EXPECT_TRUE(handle_found) << "File not found in " << total_handles
      << " handles (includes " << type_handles << " " << HandleTable::kTypeFile
      << " handles):\n\t" << handle_name << "\n\t[" << my_file << "]";

  // Clean up the file we made.
  EXPECT_TRUE(::CloseHandle(file));
}
