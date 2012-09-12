// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/file_change.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fileapi {

namespace {

FileChange AddFile() {
  return FileChange(FileChange::FILE_CHANGE_ADD,
                    FileChange::FILE_TYPE_FILE);
}

FileChange DeleteFile() {
  return FileChange(FileChange::FILE_CHANGE_DELETE,
                    FileChange::FILE_TYPE_FILE);
}

FileChange UpdateFile() {
  return FileChange(FileChange::FILE_CHANGE_UPDATE,
                    FileChange::FILE_TYPE_FILE);
}

FileChange AddDirectory() {
  return FileChange(FileChange::FILE_CHANGE_ADD,
                    FileChange::FILE_TYPE_DIRECTORY);
}

FileChange DeleteDirectory() {
  return FileChange(FileChange::FILE_CHANGE_DELETE,
                    FileChange::FILE_TYPE_DIRECTORY);
}

template <size_t INPUT_SIZE>
void CreateList(FileChangeList* list, const FileChange (&inputs)[INPUT_SIZE]) {
  list->clear();
  for (size_t i = 0; i < INPUT_SIZE; ++i)
    list->Update(inputs[i]);
}

template <size_t EXPECTED_SIZE>
void VerifyList(const FileChangeList& list,
                const FileChange (&expected)[EXPECTED_SIZE]) {
  SCOPED_TRACE(testing::Message() << list.DebugString());
  ASSERT_EQ(EXPECTED_SIZE, list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    SCOPED_TRACE(testing::Message() << i << ": "
                 << " expected:" << expected[i].DebugString()
                 << " actual:" << list.list().at(i).DebugString());
    EXPECT_EQ(expected[i], list.list().at(i));
  }
}

}  // namespace

TEST(FileChangeListTest, UpdateSimple) {
  FileChangeList list;
  const FileChange kInput1[] = { AddFile() };
  const FileChange kExpected1[] = { AddFile() };
  CreateList(&list, kInput1);
  VerifyList(list, kExpected1);

  // Add + Delete -> empty.
  const FileChange kInput2[] = { AddFile(), DeleteFile() };
  CreateList(&list, kInput2);
  ASSERT_TRUE(list.empty());

  // Add + Delete -> empty (directory).
  const FileChange kInput3[] = { AddDirectory(), DeleteDirectory() };
  CreateList(&list, kInput3);
  ASSERT_TRUE(list.empty());

  // Add + Update -> Add.
  const FileChange kInput4[] = { AddFile(), UpdateFile() };
  const FileChange kExpected4[] = { AddFile() };
  CreateList(&list, kInput4);
  VerifyList(list, kExpected4);

  // Delete + Add -> Add.
  const FileChange kInput5[] = { DeleteFile(), AddFile() };
  const FileChange kExpected5[] = { AddFile() };
  CreateList(&list, kInput5);
  VerifyList(list, kExpected5);

  // Delete + Add -> Add (directory).
  const FileChange kInput6[] = { DeleteDirectory(), AddDirectory() };
  const FileChange kExpected6[] = { AddDirectory() };
  CreateList(&list, kInput6);
  VerifyList(list, kExpected6);

  // Update + Delete -> Delete.
  const FileChange kInput7[] = { UpdateFile(), DeleteFile() };
  const FileChange kExpected7[] = { DeleteFile() };
  CreateList(&list, kInput7);
  VerifyList(list, kExpected7);
}

TEST(FileChangeListTest, UpdateCombined) {
  FileChangeList list;

  // Longer ones.
  const FileChange kInput1[] = {
    AddFile(),
    UpdateFile(),
    UpdateFile(),
    UpdateFile(),
    DeleteFile(),
    AddDirectory(),
  };
  const FileChange kExpected1[] = { AddDirectory() };
  CreateList(&list, kInput1);
  VerifyList(list, kExpected1);

  const FileChange kInput2[] = {
    AddFile(),
    DeleteFile(),
    AddFile(),
    UpdateFile(),
    UpdateFile(),
  };
  const FileChange kExpected2[] = { AddFile() };
  CreateList(&list, kInput2);
  VerifyList(list, kExpected2);

  const FileChange kInput3[] = {
    AddDirectory(),
    DeleteDirectory(),
    AddFile(),
    UpdateFile(),
    UpdateFile(),
  };
  const FileChange kExpected3[] = { AddFile() };
  CreateList(&list, kInput3);
  VerifyList(list, kExpected3);
}

}  // namespace fileapi
