// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_request_body.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_storage_controller.h"

using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;

namespace webkit_glue {

TEST(ResourceRequestBodyTest, CreateUploadDataWithoutBlob) {
  scoped_refptr<ResourceRequestBody> request_body = new ResourceRequestBody;

  const char kData[] = "123";
  const FilePath::StringType kFilePath = FILE_PATH_LITERAL("abc");
  const uint64 kFileOffset = 10U;
  const uint64 kFileLength = 100U;
  const base::Time kFileTime = base::Time::FromDoubleT(999);
  const int64 kIdentifier = 12345;

  request_body->AppendBytes(kData, arraysize(kData) - 1);
  request_body->AppendFileRange(FilePath(kFilePath),
                                kFileOffset, kFileLength, kFileTime);
  request_body->set_identifier(kIdentifier);

  scoped_refptr<net::UploadData> upload =
      request_body->ResolveElementsAndCreateUploadData(NULL);

  EXPECT_EQ(kIdentifier, upload->identifier());
  ASSERT_EQ(request_body->elements()->size(), upload->elements()->size());

  const net::UploadElement& e1 = upload->elements()->at(0);
  EXPECT_EQ(net::UploadElement::TYPE_BYTES, e1.type());
  EXPECT_EQ(kData, std::string(e1.bytes(), e1.bytes_length()));

  const net::UploadElement& e2 = upload->elements()->at(1);
  EXPECT_EQ(net::UploadElement::TYPE_FILE, e2.type());
  EXPECT_EQ(kFilePath, e2.file_path().value());
  EXPECT_EQ(kFileOffset, e2.file_range_offset());
  EXPECT_EQ(kFileLength, e2.file_range_length());
  EXPECT_EQ(kFileTime, e2.expected_file_modification_time());
}

TEST(ResourceRequestBodyTest, ResolveBlobAndCreateUploadData) {
  // Setup blob data for testing.
  base::Time time1, time2;
  base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
  base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

  BlobStorageController blob_storage_controller;
  scoped_refptr<BlobData> blob_data(new BlobData());

  GURL blob_url0("blob://url_0");
  blob_storage_controller.AddFinishedBlob(blob_url0, blob_data);

  blob_data->AppendData("BlobData");
  blob_data->AppendFile(
      FilePath(FILE_PATH_LITERAL("BlobFile.txt")), 0, 20, time1);

  GURL blob_url1("blob://url_1");
  blob_storage_controller.AddFinishedBlob(blob_url1, blob_data);

  GURL blob_url2("blob://url_2");
  blob_storage_controller.CloneBlob(blob_url2, blob_url1);

  GURL blob_url3("blob://url_3");
  blob_storage_controller.CloneBlob(blob_url3, blob_url2);

  // Setup upload data elements for comparison.
  net::UploadElement blob_element1, blob_element2;
  blob_element1.SetToBytes(
      blob_data->items().at(0).data.c_str() +
          static_cast<int>(blob_data->items().at(0).offset),
      static_cast<int>(blob_data->items().at(0).length));
  blob_element2.SetToFilePathRange(
      blob_data->items().at(1).file_path,
      blob_data->items().at(1).offset,
      blob_data->items().at(1).length,
      blob_data->items().at(1).expected_modification_time);

  net::UploadElement upload_element1, upload_element2;
  upload_element1.SetToBytes("Hello", 5);
  upload_element2.SetToFilePathRange(
      FilePath(FILE_PATH_LITERAL("foo1.txt")), 0, 20, time2);

  // Test no blob reference.
  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendBytes(
      upload_element1.bytes(),
      upload_element1.bytes_length());
  request_body->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  scoped_refptr<net::UploadData> upload =
      request_body->ResolveElementsAndCreateUploadData(
          &blob_storage_controller);

  ASSERT_EQ(upload->elements()->size(), 2U);
  EXPECT_TRUE(upload->elements()->at(0) == upload_element1);
  EXPECT_TRUE(upload->elements()->at(1) == upload_element2);

  // Test having only one blob reference that refers to empty blob data.
  request_body = new ResourceRequestBody();
  request_body->AppendBlob(blob_url0);

  upload = request_body->ResolveElementsAndCreateUploadData(
      &blob_storage_controller);
  ASSERT_EQ(upload->elements()->size(), 0U);

  // Test having only one blob reference.
  request_body = new ResourceRequestBody();
  request_body->AppendBlob(blob_url1);

  upload = request_body->ResolveElementsAndCreateUploadData(
      &blob_storage_controller);
  ASSERT_EQ(upload->elements()->size(), 2U);
  EXPECT_TRUE(upload->elements()->at(0) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(1) == blob_element2);

  // Test having one blob reference at the beginning.
  request_body = new ResourceRequestBody();
  request_body->AppendBlob(blob_url1);
  request_body->AppendBytes(
      upload_element1.bytes(),
      upload_element1.bytes_length());
  request_body->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  upload = request_body->ResolveElementsAndCreateUploadData(
      &blob_storage_controller);
  ASSERT_EQ(upload->elements()->size(), 4U);
  EXPECT_TRUE(upload->elements()->at(0) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(1) == blob_element2);
  EXPECT_TRUE(upload->elements()->at(2) == upload_element1);
  EXPECT_TRUE(upload->elements()->at(3) == upload_element2);

  // Test having one blob reference at the end.
  request_body = new ResourceRequestBody();
  request_body->AppendBytes(
      upload_element1.bytes(),
      upload_element1.bytes_length());
  request_body->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());
  request_body->AppendBlob(blob_url1);

  upload = request_body->ResolveElementsAndCreateUploadData(
      &blob_storage_controller);
  ASSERT_EQ(upload->elements()->size(), 4U);
  EXPECT_TRUE(upload->elements()->at(0) == upload_element1);
  EXPECT_TRUE(upload->elements()->at(1) == upload_element2);
  EXPECT_TRUE(upload->elements()->at(2) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(3) == blob_element2);

  // Test having one blob reference in the middle.
  request_body = new ResourceRequestBody();
  request_body->AppendBytes(
      upload_element1.bytes(),
      upload_element1.bytes_length());
  request_body->AppendBlob(blob_url1);
  request_body->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  upload = request_body->ResolveElementsAndCreateUploadData(
      &blob_storage_controller);
  ASSERT_EQ(upload->elements()->size(), 4U);
  EXPECT_TRUE(upload->elements()->at(0) == upload_element1);
  EXPECT_TRUE(upload->elements()->at(1) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(2) == blob_element2);
  EXPECT_TRUE(upload->elements()->at(3) == upload_element2);

  // Test having multiple blob references.
  request_body = new ResourceRequestBody();
  request_body->AppendBlob(blob_url1);
  request_body->AppendBytes(
      upload_element1.bytes(),
      upload_element1.bytes_length());
  request_body->AppendBlob(blob_url2);
  request_body->AppendBlob(blob_url3);
  request_body->AppendFileRange(
      upload_element2.file_path(),
      upload_element2.file_range_offset(),
      upload_element2.file_range_length(),
      upload_element2.expected_file_modification_time());

  upload = request_body->ResolveElementsAndCreateUploadData(
      &blob_storage_controller);
  ASSERT_EQ(upload->elements()->size(), 8U);
  EXPECT_TRUE(upload->elements()->at(0) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(1) == blob_element2);
  EXPECT_TRUE(upload->elements()->at(2) == upload_element1);
  EXPECT_TRUE(upload->elements()->at(3) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(4) == blob_element2);
  EXPECT_TRUE(upload->elements()->at(5) == blob_element1);
  EXPECT_TRUE(upload->elements()->at(6) == blob_element2);
  EXPECT_TRUE(upload->elements()->at(7) == upload_element2);
}

}  // namespace webkit_glue
