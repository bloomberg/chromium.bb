// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

namespace webkit_blob {

TEST(BlobStorageControllerTest, RegisterBlobUrl) {
  // Setup a set of blob data for testing.
  base::Time time1, time2;
  base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
  base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

  scoped_refptr<BlobData> blob_data1(new BlobData());
  blob_data1->AppendData("Data1");
  blob_data1->AppendData("Data2");
  blob_data1->AppendFile(base::FilePath(FILE_PATH_LITERAL("File1.txt")),
    10, 1024, time1);

  scoped_refptr<BlobData> blob_data2(new BlobData());
  blob_data2->AppendData("Data3");
  blob_data2->AppendBlob(GURL("blob://url_1"), 8, 100);
  blob_data2->AppendFile(base::FilePath(FILE_PATH_LITERAL("File2.txt")),
      0, 20, time2);

  scoped_refptr<BlobData> canonicalized_blob_data2(new BlobData());
  canonicalized_blob_data2->AppendData("Data3");
  canonicalized_blob_data2->AppendData("a2___", 2);
  canonicalized_blob_data2->AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File1.txt")),
      10, 98, time1);
  canonicalized_blob_data2->AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20, time2);

  BlobStorageController blob_storage_controller;

  // Test registering a blob URL referring to the blob data containing only
  // data and file.
  GURL blob_url1("blob://url_1");
  blob_storage_controller.AddFinishedBlob(blob_url1, blob_data1);

  BlobData* blob_data_found =
      blob_storage_controller.GetBlobDataFromUrl(blob_url1);
  ASSERT_TRUE(blob_data_found != NULL);
  EXPECT_TRUE(*blob_data_found == *blob_data1);

  // Test registering a blob URL referring to the blob data containing data,
  // file and blob.
  GURL blob_url2("blob://url_2");
  blob_storage_controller.AddFinishedBlob(blob_url2, blob_data2);

  blob_data_found = blob_storage_controller.GetBlobDataFromUrl(blob_url2);
  ASSERT_TRUE(blob_data_found != NULL);
  EXPECT_TRUE(*blob_data_found == *canonicalized_blob_data2);

  // Test registering a blob URL referring to existent blob URL.
  GURL blob_url3("blob://url_3");
  blob_storage_controller.CloneBlob(blob_url3, blob_url1);

  blob_data_found = blob_storage_controller.GetBlobDataFromUrl(blob_url3);
  ASSERT_TRUE(blob_data_found != NULL);
  EXPECT_TRUE(*blob_data_found == *blob_data1);

  // Test unregistering a blob URL.
  blob_storage_controller.RemoveBlob(blob_url3);
  blob_data_found = blob_storage_controller.GetBlobDataFromUrl(blob_url3);
  EXPECT_TRUE(!blob_data_found);
}

}  // namespace webkit_blob
