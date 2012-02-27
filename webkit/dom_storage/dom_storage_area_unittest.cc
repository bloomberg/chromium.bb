// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

TEST(DomStorageAreaTest, DomStorageAreaBasics) {
  const GURL kOrigin("http://dom_storage/");
  const string16 kKey(ASCIIToUTF16("key"));
  const string16 kValue(ASCIIToUTF16("value"));
  const string16 kKey2(ASCIIToUTF16("key2"));
  const string16 kValue2(ASCIIToUTF16("value2"));

  scoped_refptr<DomStorageArea> area(
      new DomStorageArea(1, kOrigin, FilePath(), NULL));
  string16 old_value;
  NullableString16 old_nullable_value;
  scoped_refptr<DomStorageArea> copy;

  // We don't focus on the underlying DomStorageMap functionality
  // since that's covered by seperate unit tests.
  EXPECT_EQ(kOrigin, area->origin());
  EXPECT_EQ(1, area->namespace_id());
  EXPECT_EQ(0u, area->Length());
  area->SetItem(kKey, kValue, &old_nullable_value);
  area->SetItem(kKey2, kValue2, &old_nullable_value);

  // Verify that a copy shares the same map.
  copy = area->ShallowCopy(2);
  EXPECT_EQ(kOrigin, copy->origin());
  EXPECT_EQ(2, copy->namespace_id());
  EXPECT_EQ(area->Length(), copy->Length());
  EXPECT_EQ(area->GetItem(kKey).string(), copy->GetItem(kKey).string());
  EXPECT_EQ(area->Key(0).string(), copy->Key(0).string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());

  // But will deep copy-on-write as needed.
  EXPECT_TRUE(area->RemoveItem(kKey, &old_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_NE(0u, area->Length());
  EXPECT_TRUE(area->Clear());
  EXPECT_EQ(0u, area->Length());
  EXPECT_NE(copy->map_.get(), area->map_.get());
}

TEST(DomStorageAreaTest, BackingDatabaseOpened) {
  const int64 kSessionStorageNamespaceId = kLocalStorageNamespaceId + 1;
  const GURL kOrigin("http://www.google.com");

  const string16 kKey = ASCIIToUTF16("test");
  const string16 kKey2 = ASCIIToUTF16("test2");
  const string16 kValue = ASCIIToUTF16("value");
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const FilePath kExpectedOriginFilePath = temp_dir.path().Append(
      DomStorageArea::DatabaseFileNameFromOrigin(kOrigin));

  // No directory, backing should be null.
  {
    scoped_refptr<DomStorageArea> area(
        new DomStorageArea(kLocalStorageNamespaceId, kOrigin, FilePath(),
                           NULL));
    EXPECT_EQ(NULL, area->backing_.get());
    EXPECT_TRUE(area->initial_import_done_);
    EXPECT_FALSE(file_util::PathExists(kExpectedOriginFilePath));
  }

  // Valid directory and origin but non-local namespace id. Backing should
  // be null.
  {
    scoped_refptr<DomStorageArea> area(
        new DomStorageArea(kSessionStorageNamespaceId, kOrigin,
                           temp_dir.path(), NULL));
    EXPECT_EQ(NULL, area->backing_.get());
    EXPECT_TRUE(area->initial_import_done_);

    NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
    ASSERT_TRUE(old_value.is_null());

    // Check that saving a value has still left us without a backing database.
    EXPECT_EQ(NULL, area->backing_.get());
    EXPECT_FALSE(file_util::PathExists(kExpectedOriginFilePath));
  }

  // This should set up a DomStorageArea that is correctly backed to disk.
  {
    scoped_refptr<DomStorageArea> area(
        new DomStorageArea(kLocalStorageNamespaceId, kOrigin,
            temp_dir.path(),
            new MockDomStorageTaskRunner(base::MessageLoopProxy::current())));

    EXPECT_TRUE(area->backing_.get());
    EXPECT_FALSE(area->backing_->IsOpen());
    EXPECT_FALSE(area->initial_import_done_);

    // Switch out the file-backed db with an in-memory db to speed up the test.
    // We will verify that something is written into the database but not
    // that a file is written to disk - DOMStorageDatabase unit tests cover
    // that.
    area->backing_.reset(new DomStorageDatabase());

    // Need to write something to ensure that the database is created.
    NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, &old_value));
    ASSERT_TRUE(old_value.is_null());
    EXPECT_TRUE(area->SetItem(kKey2, kValue, &old_value));
    ASSERT_TRUE(old_value.is_null());
    EXPECT_TRUE(area->initial_import_done_);
    EXPECT_TRUE(area->commit_in_flight_);

    MessageLoop::current()->RunAllPending();

    EXPECT_FALSE(area->commit_in_flight_);
    EXPECT_TRUE(area->backing_->IsOpen());
    EXPECT_EQ(2u, area->Length());
    EXPECT_EQ(kValue, area->GetItem(kKey).string());

    // Verify the content made it to the in memory database.
    ValuesMap values;
    area->backing_->ReadAllValues(&values);
    EXPECT_EQ(2u, values.size());
    EXPECT_EQ(kValue, values[kKey].string());
  }
}

TEST(DomStorageAreaTest, TestDatabaseFilePath) {
  EXPECT_EQ(FilePath().AppendASCII("file_path_to_0.localstorage"),
      DomStorageArea::DatabaseFileNameFromOrigin(
          GURL("file://path_to/index.html")));

  EXPECT_EQ(FilePath().AppendASCII("https_www.google.com_0.localstorage"),
      DomStorageArea::DatabaseFileNameFromOrigin(
          GURL("https://www.google.com/")));

  EXPECT_EQ(FilePath().AppendASCII("https_www.google.com_8080.localstorage"),
      DomStorageArea::DatabaseFileNameFromOrigin(
          GURL("https://www.google.com:8080")));
}

}  // namespace dom_storage
