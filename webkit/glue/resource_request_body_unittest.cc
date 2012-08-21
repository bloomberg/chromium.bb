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

namespace webkit_glue {

TEST(ResourceRequestBodyTest, CreateUploadDataTest) {
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

  scoped_refptr<net::UploadData> upload = request_body->CreateUploadData();

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

}  // namespace webkit_glue
