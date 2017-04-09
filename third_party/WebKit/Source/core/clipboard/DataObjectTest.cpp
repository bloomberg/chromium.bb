// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/clipboard/DataObject.h"

#include "core/clipboard/DataObjectItem.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DataObjectTest : public ::testing::Test {
 public:
  DataObjectTest() : data_object_(DataObject::Create()) {}

 protected:
  Persistent<DataObject> data_object_;
};

TEST_F(DataObjectTest, addItemWithFilenameAndNoTitle) {
  String file_path = testing::BlinkRootDir();
  file_path.Append("/Source/core/clipboard/DataObjectTest.cpp");

  data_object_->AddFilename(file_path, String(), String());
  EXPECT_EQ(1U, data_object_->length());

  DataObjectItem* item = data_object_->Item(0);
  EXPECT_EQ(DataObjectItem::kFileKind, item->Kind());

  Blob* blob = item->GetAsFile();
  ASSERT_TRUE(blob->IsFile());
  File* file = ToFile(blob);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ(file_path, file->GetPath());
}

TEST_F(DataObjectTest, addItemWithFilenameAndTitle) {
  String file_path = testing::BlinkRootDir();
  file_path.Append("/Source/core/clipboard/DataObjectTest.cpp");

  data_object_->AddFilename(file_path, "name.cpp", String());
  EXPECT_EQ(1U, data_object_->length());

  DataObjectItem* item = data_object_->Item(0);
  EXPECT_EQ(DataObjectItem::kFileKind, item->Kind());

  Blob* blob = item->GetAsFile();
  ASSERT_TRUE(blob->IsFile());
  File* file = ToFile(blob);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ(file_path, file->GetPath());
  EXPECT_EQ("name.cpp", file->name());
}

TEST_F(DataObjectTest, fileSystemId) {
  String file_path = testing::BlinkRootDir();
  file_path.Append("/Source/core/clipboard/DataObjectTest.cpp");
  KURL url;

  data_object_->AddFilename(file_path, String(), String());
  data_object_->AddFilename(file_path, String(), "fileSystemIdForFilename");
  data_object_->Add(
      File::CreateForFileSystemFile(url, FileMetadata(), File::kIsUserVisible),
      "fileSystemIdForFileSystemFile");

  ASSERT_EQ(3U, data_object_->length());

  {
    DataObjectItem* item = data_object_->Item(0);
    EXPECT_FALSE(item->HasFileSystemId());
  }

  {
    DataObjectItem* item = data_object_->Item(1);
    EXPECT_TRUE(item->HasFileSystemId());
    EXPECT_EQ("fileSystemIdForFilename", item->FileSystemId());
  }

  {
    DataObjectItem* item = data_object_->Item(2);
    EXPECT_TRUE(item->HasFileSystemId());
    EXPECT_EQ("fileSystemIdForFileSystemFile", item->FileSystemId());
  }
}

}  // namespace blink
