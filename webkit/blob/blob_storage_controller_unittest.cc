// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/time.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/upload_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using net::UploadData;

namespace webkit_blob {

TEST(BlobStorageControllerTest, RegisterBlobUrl) {
  // Setup a set of blob data for testing.
  base::Time time1, time2;
  base::Time::FromString(L"Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
  base::Time::FromString(L"Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

  scoped_refptr<BlobData> blob_data1 = new BlobData();
  blob_data1->AppendData("Data1");
  blob_data1->AppendData("Data2");
  blob_data1->AppendFile(FilePath(FILE_PATH_LITERAL("File1.txt")),
    10, 1024, time1);

  scoped_refptr<BlobData> blob_data2 = new BlobData();
  blob_data2->AppendData("Data3");
  blob_data2->AppendBlob(GURL("blob://url_1"), 8, 100);
  blob_data2->AppendFile(FilePath(FILE_PATH_LITERAL("File2.txt")),
      0, 20, time2);

  scoped_refptr<BlobData> canonicalized_blob_data2 = new BlobData();
  canonicalized_blob_data2->AppendData("Data3");
  canonicalized_blob_data2->AppendData("Data2", 3, 2);
  canonicalized_blob_data2->AppendFile(FilePath(FILE_PATH_LITERAL("File1.txt")),
      10, 98, time1);
  canonicalized_blob_data2->AppendFile(FilePath(FILE_PATH_LITERAL("File2.txt")),
      0, 20, time2);

  scoped_ptr<BlobStorageController> blob_storage_controller(
      new BlobStorageController());

  // Test registering a blob URL referring to the blob data containing only
  // data and file.
  GURL blob_url1("blob://url_1");
  blob_storage_controller->RegisterBlobUrl(blob_url1, blob_data1);

  BlobData* blob_data_found =
      blob_storage_controller->GetBlobDataFromUrl(blob_url1);
  ASSERT_TRUE(blob_data_found != NULL);
  EXPECT_TRUE(*blob_data_found == *blob_data1);

  // Test registering a blob URL referring to the blob data containing data,
  // file and blob.
  GURL blob_url2("blob://url_2");
  blob_storage_controller->RegisterBlobUrl(blob_url2, blob_data2);

  blob_data_found = blob_storage_controller->GetBlobDataFromUrl(blob_url2);
  ASSERT_TRUE(blob_data_found != NULL);
  EXPECT_TRUE(*blob_data_found == *canonicalized_blob_data2);

  // Test registering a blob URL referring to existent blob URL.
  GURL blob_url3("blob://url_3");
  blob_storage_controller->RegisterBlobUrlFrom(blob_url3, blob_url1);

  blob_data_found = blob_storage_controller->GetBlobDataFromUrl(blob_url3);
  ASSERT_TRUE(blob_data_found != NULL);
  EXPECT_TRUE(*blob_data_found == *blob_data1);

  // Test unregistering a blob URL.
  blob_storage_controller->UnregisterBlobUrl(blob_url3);
  blob_data_found = blob_storage_controller->GetBlobDataFromUrl(blob_url3);
  EXPECT_TRUE(!blob_data_found);
}

TEST(BlobStorageControllerTest, ResolveBlobReferencesInUploadData) {
  // Setup blob data for testing.
  base::Time time1, time2;
  base::Time::FromString(L"Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
  base::Time::FromString(L"Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

  scoped_ptr<BlobStorageController> blob_storage_controller(
      new BlobStorageController());

  scoped_refptr<BlobData> blob_data = new BlobData();

  GURL blob_url0("blob://url_0");
  blob_storage_controller->RegisterBlobUrl(blob_url0, blob_data);

  blob_data->AppendData("BlobData");
  blob_data->AppendFile(
      FilePath(FILE_PATH_LITERAL("BlobFile.txt")), 0, 20, time1);

  GURL blob_url1("blob://url_1");
  blob_storage_controller->RegisterBlobUrl(blob_url1, blob_data);

  GURL blob_url2("blob://url_2");
  blob_storage_controller->RegisterBlobUrlFrom(blob_url2, blob_url1);

  GURL blob_url3("blob://url_3");
  blob_storage_controller->RegisterBlobUrlFrom(blob_url3, blob_url2);

  // Setup upload data elements for comparison.
  UploadData::Element blob_element1, blob_element2;
  blob_element1.SetToBytes(
      blob_data->items().at(0).data().c_str() +
          static_cast<int>(blob_data->items().at(0).offset()),
      static_cast<int>(blob_data->items().at(0).length()));
  blob_element2.SetToFilePathRange(
      blob_data->items().at(1).file_path(),
      blob_data->items().at(1).offset(),
      blob_data->items().at(1).length(),
      blob_data->items().at(1).expected_modification_time());

  UploadData::Element upload_element1, upload_element2;
  upload_element1.SetToBytes("Hello", 5);
  upload_element2.SetToFilePathRange(
      FilePath(FILE_PATH_LITERAL("foo1.txt")), 0, 20, time2);

  // Test no blob reference.
  scoped_refptr<UploadData> upload_data = new UploadData();
  upload_data->AppendBytes(
      &upload_element1.bytes().at(0),
      upload_element1.bytes().size());
  upload_data->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 2U);
  EXPECT_TRUE(upload_data->elements()->at(0) == upload_element1);
  EXPECT_TRUE(upload_data->elements()->at(1) == upload_element2);

  // Test having only one blob reference that refers to empty blob data.
  upload_data = new UploadData();
  upload_data->AppendBlob(blob_url0);

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 0U);

  // Test having only one blob reference.
  upload_data = new UploadData();
  upload_data->AppendBlob(blob_url1);

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 2U);
  EXPECT_TRUE(upload_data->elements()->at(0) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(1) == blob_element2);

  // Test having one blob reference at the beginning.
  upload_data = new UploadData();
  upload_data->AppendBlob(blob_url1);
  upload_data->AppendBytes(
      &upload_element1.bytes().at(0),
      upload_element1.bytes().size());
  upload_data->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 4U);
  EXPECT_TRUE(upload_data->elements()->at(0) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(1) == blob_element2);
  EXPECT_TRUE(upload_data->elements()->at(2) == upload_element1);
  EXPECT_TRUE(upload_data->elements()->at(3) == upload_element2);

  // Test having one blob reference at the end.
  upload_data = new UploadData();
  upload_data->AppendBytes(
      &upload_element1.bytes().at(0),
      upload_element1.bytes().size());
  upload_data->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());
  upload_data->AppendBlob(blob_url1);

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 4U);
  EXPECT_TRUE(upload_data->elements()->at(0) == upload_element1);
  EXPECT_TRUE(upload_data->elements()->at(1) == upload_element2);
  EXPECT_TRUE(upload_data->elements()->at(2) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(3) == blob_element2);

  // Test having one blob reference in the middle.
  upload_data = new UploadData();
  upload_data->AppendBytes(
      &upload_element1.bytes().at(0),
      upload_element1.bytes().size());
  upload_data->AppendBlob(blob_url1);
  upload_data->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 4U);
  EXPECT_TRUE(upload_data->elements()->at(0) == upload_element1);
  EXPECT_TRUE(upload_data->elements()->at(1) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(2) == blob_element2);
  EXPECT_TRUE(upload_data->elements()->at(3) == upload_element2);

  // Test having multiple blob references.
  upload_data = new UploadData();
  upload_data->AppendBlob(blob_url1);
  upload_data->AppendBytes(
      &upload_element1.bytes().at(0),
      upload_element1.bytes().size());
  upload_data->AppendBlob(blob_url2);
  upload_data->AppendBlob(blob_url3);
  upload_data->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  blob_storage_controller->ResolveBlobReferencesInUploadData(upload_data.get());
  ASSERT_EQ(upload_data->elements()->size(), 8U);
  EXPECT_TRUE(upload_data->elements()->at(0) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(1) == blob_element2);
  EXPECT_TRUE(upload_data->elements()->at(2) == upload_element1);
  EXPECT_TRUE(upload_data->elements()->at(3) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(4) == blob_element2);
  EXPECT_TRUE(upload_data->elements()->at(5) == blob_element1);
  EXPECT_TRUE(upload_data->elements()->at(6) == blob_element2);
  EXPECT_TRUE(upload_data->elements()->at(7) == upload_element2);
}

}  // namespace webkit_blob
