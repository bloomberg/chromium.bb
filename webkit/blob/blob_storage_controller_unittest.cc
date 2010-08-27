// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/time.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

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

  // Test registering a blob URL referring to non-existent blob URL.
  GURL nonexistent_blob_url("blob://url_none");
  GURL blob_url4("blob://url_4");
  blob_storage_controller->RegisterBlobUrlFrom(blob_url4, nonexistent_blob_url);

  blob_data_found = blob_storage_controller->GetBlobDataFromUrl(blob_url4);
  EXPECT_FALSE(blob_data_found != NULL);

  // Test unregistering a blob URL.
  blob_storage_controller->UnregisterBlobUrl(blob_url3);
  blob_data_found = blob_storage_controller->GetBlobDataFromUrl(blob_url3);
  EXPECT_FALSE(blob_data_found != NULL);
}

}  // namespace webkit_blob
