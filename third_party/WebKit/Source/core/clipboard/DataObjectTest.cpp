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

class DataObjectObserver : public GarbageCollected<DataObjectObserver>,
                           public DataObject::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(DataObjectObserver);

 public:
  DataObjectObserver() : call_count_(0) {}
  void OnItemListChanged() override { call_count_++; }
  size_t call_count() { return call_count_; }

 private:
  size_t call_count_;
};

TEST_F(DataObjectTest, DataObjectObserver) {
  DataObjectObserver* observer = new DataObjectObserver;
  data_object_->AddObserver(observer);

  data_object_->ClearAll();
  EXPECT_EQ(0U, data_object_->length());
  EXPECT_EQ(0U, observer->call_count());

  data_object_->SetData("text/plain", "foobar");
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(1U, observer->call_count());

  DataObjectItem* item = data_object_->Add("bar quux", "text/plain");
  EXPECT_EQ(nullptr, item);
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(1U, observer->call_count());

  item = data_object_->Add("bar quux", "application/octet-stream");
  EXPECT_NE(nullptr, item);
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(2U, observer->call_count());

  data_object_->DeleteItem(42);
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(2U, observer->call_count());

  data_object_->DeleteItem(0);
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(3U, observer->call_count());

  DataObjectObserver* observer2 = new DataObjectObserver;
  data_object_->AddObserver(observer2);

  String file_path = testing::BlinkRootDir();
  file_path.append("/Source/core/clipboard/DataObjectTest.cpp");
  data_object_->AddFilename(file_path, String(), String());
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(4U, observer->call_count());
  EXPECT_EQ(1U, observer2->call_count());

  data_object_->ClearData("application/octet-stream");
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(5U, observer->call_count());
  EXPECT_EQ(2U, observer2->call_count());

  data_object_->ClearAll();
  EXPECT_EQ(0U, data_object_->length());
  EXPECT_EQ(6U, observer->call_count());
  EXPECT_EQ(3U, observer2->call_count());
}

TEST_F(DataObjectTest, addItemWithFilenameAndNoTitle) {
  String file_path = testing::BlinkRootDir();
  file_path.append("/Source/core/clipboard/DataObjectTest.cpp");

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
  file_path.append("/Source/core/clipboard/DataObjectTest.cpp");

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
  file_path.append("/Source/core/clipboard/DataObjectTest.cpp");
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
