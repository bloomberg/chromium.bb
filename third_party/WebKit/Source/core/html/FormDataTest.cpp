// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/FormData.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FormDataTest, get) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("name1", "value1");

  FileOrUSVString result;
  fd->get("name1", result);
  EXPECT_TRUE(result.isUSVString());
  EXPECT_EQ("value1", result.getAsUSVString());

  const FormData::Entry& entry = *fd->Entries()[0];
  EXPECT_STREQ("name1", entry.name().Data());
  EXPECT_STREQ("value1", entry.Value().Data());
}

TEST(FormDataTest, getAll) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("name1", "value1");

  HeapVector<FormDataEntryValue> results = fd->getAll("name1");
  EXPECT_EQ(1u, results.size());
  EXPECT_TRUE(results[0].isUSVString());
  EXPECT_EQ("value1", results[0].getAsUSVString());

  EXPECT_EQ(1u, fd->size());
}

TEST(FormDataTest, has) {
  FormData* fd = FormData::Create(UTF8Encoding());
  fd->append("name1", "value1");

  EXPECT_TRUE(fd->has("name1"));
  EXPECT_EQ(1u, fd->size());
}

}  // namespace blink
