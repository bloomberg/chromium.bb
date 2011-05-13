// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_directory_database.h"

#include "base/memory/scoped_temp_dir.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fileapi {

class FileSystemDirectoryDatabaseTest : public testing::Test {
 public:
  typedef FileSystemDirectoryDatabase::FileId FileId;
  typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

  FileSystemDirectoryDatabaseTest() {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    InitDatabase();
  }

  FileSystemDirectoryDatabase* db() {
    return db_.get();
  }

  void InitDatabase() {
    db_.reset();
    FilePath path = base_.path().AppendASCII("db");
    db_.reset(new FileSystemDirectoryDatabase(path));
  }

  bool AddFileInfo(FileId parent_id, const std::string& name) {
    FileId file_id;
    FileInfo info;
    info.parent_id = parent_id;
    info.name = name;
    return db_->AddFileInfo(info, &file_id);
  }

 protected:
  // Common temp base for nondestructive uses.
  ScopedTempDir base_;
  scoped_ptr<FileSystemDirectoryDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDirectoryDatabaseTest);
};

TEST_F(FileSystemDirectoryDatabaseTest, TestMissingFileGetInfo) {
  FileId file_id = 888;
  FileInfo info;
  EXPECT_FALSE(db()->GetFileInfo(file_id, &info));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestMissingParentAddFileInfo) {
  FileId parent_id = 7;
  EXPECT_FALSE(AddFileInfo(parent_id, "foo"));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestAddNameClash) {
  FileInfo info;
  FileId file_id;
  info.parent_id = 0;
  info.name = "dir 0";
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id));

  // Check for name clash in the root directory.
  std::string name = info.name;
  EXPECT_FALSE(AddFileInfo(0, name));
  name = "dir 1";
  EXPECT_TRUE(AddFileInfo(0, name));

  name = "subdir 0";
  EXPECT_TRUE(AddFileInfo(file_id, name));

  // Check for name clash in a subdirectory.
  EXPECT_FALSE(AddFileInfo(file_id, name));
  name = "subdir 1";
  EXPECT_TRUE(AddFileInfo(file_id, name));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestRenameNoMoveNameClash) {
  FileInfo info;
  FileId file_id0;
  std::string name0 = "foo";
  std::string name1 = "bar";
  std::string name2 = "bas";
  info.parent_id = 0;
  info.name = name0;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id0));
  EXPECT_TRUE(AddFileInfo(0, name1));
  info.name = name1;
  EXPECT_FALSE(db()->UpdateFileInfo(file_id0, info));
  info.name = name2;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id0, info));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestMoveSameNameNameClash) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  std::string name0 = "foo";
  std::string name1 = "bar";
  info.parent_id = 0;
  info.name = name0;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id1));
  info.parent_id = 0;
  EXPECT_FALSE(db()->UpdateFileInfo(file_id1, info));
  info.name = name1;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id1, info));
}

// TODO: Test UpdateFileInfo without move or rename.

TEST_F(FileSystemDirectoryDatabaseTest, TestMoveRenameNameClash) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  std::string name0 = "foo";
  std::string name1 = "bar";
  std::string name2 = "bas";
  info.parent_id = 0;
  info.name = name0;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  info.name = name1;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id1));
  info.parent_id = 0;
  info.name = name0;
  EXPECT_FALSE(db()->UpdateFileInfo(file_id1, info));
  info.name = name1;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id1, info));
  // Also test a successful move+rename.
  info.parent_id = file_id0;
  info.name = name2;
  EXPECT_TRUE(db()->UpdateFileInfo(file_id1, info));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestRemoveWithChildren) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  info.parent_id = 0;
  info.name = "foo";
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id1));
  EXPECT_FALSE(db()->RemoveFileInfo(file_id0));
  EXPECT_TRUE(db()->RemoveFileInfo(file_id1));
  EXPECT_TRUE(db()->RemoveFileInfo(file_id0));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestGetChildWithName) {
  FileInfo info;
  FileId file_id0;
  FileId file_id1;
  std::string name0 = "foo";
  std::string name1 = "bar";
  info.parent_id = 0;
  info.name = name0;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id0));
  info.parent_id = file_id0;
  info.name = name1;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id1));
  EXPECT_NE(file_id0, file_id1);

  FileId check_file_id;
  EXPECT_FALSE(db()->GetChildWithName(0, name1, &check_file_id));
  EXPECT_TRUE(db()->GetChildWithName(0, name0, &check_file_id));
  EXPECT_EQ(file_id0, check_file_id);
  EXPECT_FALSE(db()->GetChildWithName(file_id0, name0, &check_file_id));
  EXPECT_TRUE(db()->GetChildWithName(file_id0, name1, &check_file_id));
  EXPECT_EQ(file_id1, check_file_id);
}

// TODO(ericu): Test GetFileWithPath.

TEST_F(FileSystemDirectoryDatabaseTest, TestListChildren) {
  // No children in the root.
  std::vector<FileId> children;
  EXPECT_TRUE(db()->ListChildren(0, &children));
  EXPECT_TRUE(children.empty());

  // One child in the root.
  FileId file_id0;
  FileInfo info;
  info.parent_id = 0;
  info.name = "foo";
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id0));
  EXPECT_TRUE(db()->ListChildren(0, &children));
  EXPECT_EQ(children.size(), 1UL);
  EXPECT_EQ(children[0], file_id0);

  // Two children in the root.
  FileId file_id1;
  info.name = "bar";
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id1));
  EXPECT_TRUE(db()->ListChildren(0, &children));
  EXPECT_EQ(2UL, children.size());
  if (children[0] == file_id0) {
    EXPECT_EQ(children[1], file_id1);
  } else {
    EXPECT_EQ(children[1], file_id0);
    EXPECT_EQ(children[0], file_id1);
  }

  // No children in a subdirectory.
  EXPECT_TRUE(db()->ListChildren(file_id0, &children));
  EXPECT_TRUE(children.empty());

  // One child in a subdirectory.
  info.parent_id = file_id0;
  info.name = "foo";
  FileId file_id2;
  FileId file_id3;
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id2));
  EXPECT_TRUE(db()->ListChildren(file_id0, &children));
  EXPECT_EQ(1UL, children.size());
  EXPECT_EQ(children[0], file_id2);

  // Two children in a subdirectory.
  info.name = "bar";
  EXPECT_TRUE(db()->AddFileInfo(info, &file_id3));
  EXPECT_TRUE(db()->ListChildren(file_id0, &children));
  EXPECT_EQ(2UL, children.size());
  if (children[0] == file_id2) {
    EXPECT_EQ(children[1], file_id3);
  } else {
    EXPECT_EQ(children[1], file_id2);
    EXPECT_EQ(children[0], file_id3);
  }
}

TEST_F(FileSystemDirectoryDatabaseTest, TestUpdateModificationTime) {
  FileInfo info0;
  FileId file_id;
  info0.parent_id = 0;
  info0.name = "name";
  info0.data_path = FilePath().AppendASCII("fake path");
  info0.modification_time = base::Time::Now();
  EXPECT_TRUE(db()->AddFileInfo(info0, &file_id));
  FileInfo info1;
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info1));
  EXPECT_EQ(info0.name, info1.name);
  EXPECT_EQ(info0.parent_id, info1.parent_id);
  EXPECT_EQ(info0.data_path, info1.data_path);
  EXPECT_EQ(info0.modification_time, info1.modification_time);

  EXPECT_TRUE(db()->UpdateModificationTime(file_id, base::Time::UnixEpoch()));
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info1));
  EXPECT_EQ(info0.name, info1.name);
  EXPECT_EQ(info0.parent_id, info1.parent_id);
  EXPECT_EQ(info0.data_path, info1.data_path);
  EXPECT_NE(info0.modification_time, info1.modification_time);
  EXPECT_EQ(info1.modification_time, base::Time::UnixEpoch());

  EXPECT_FALSE(db()->UpdateModificationTime(999, base::Time::UnixEpoch()));
}

TEST_F(FileSystemDirectoryDatabaseTest, TestSimpleFileOperations) {
  FileId file_id = 888;
  FileInfo info0;
  EXPECT_FALSE(db()->GetFileInfo(file_id, &info0));
  info0.parent_id = 0;
  info0.data_path = FilePath().AppendASCII("foo");
  info0.name = "file name";
  info0.modification_time = base::Time::Now();
  EXPECT_TRUE(db()->AddFileInfo(info0, &file_id));
  FileInfo info1;
  EXPECT_TRUE(db()->GetFileInfo(file_id, &info1));
  EXPECT_EQ(info0.parent_id, info1.parent_id);
  EXPECT_EQ(info0.data_path, info1.data_path);
  EXPECT_EQ(info0.name, info1.name);
  EXPECT_EQ(info0.modification_time, info1.modification_time);
}

// TODO: Test for OverwritingMoveFile.

TEST_F(FileSystemDirectoryDatabaseTest, TestGetNextInteger) {
  int64 next;
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(0, next);
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(1, next);
  InitDatabase();
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(2, next);
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(3, next);
  InitDatabase();
  EXPECT_TRUE(db()->GetNextInteger(&next));
  EXPECT_EQ(4, next);
}

}  // namespace fileapi
