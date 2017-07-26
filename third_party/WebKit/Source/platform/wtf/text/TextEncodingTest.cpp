// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/TextEncoding.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

namespace {

TEST(TextEncoding, NonByteBased) {
  EXPECT_FALSE(TextEncoding("utf-8").IsNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-16").IsNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-16le").IsNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-16be").IsNonByteBasedEncoding());
  EXPECT_FALSE(TextEncoding("windows-1252").IsNonByteBasedEncoding());
  EXPECT_FALSE(TextEncoding("gbk").IsNonByteBasedEncoding());
  EXPECT_FALSE(TextEncoding("utf-7").IsNonByteBasedEncoding());
}

TEST(TextEncoding, ClosestByteBased) {
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-8").ClosestByteBasedEquivalent().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16").ClosestByteBasedEquivalent().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16le").ClosestByteBasedEquivalent().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16be").ClosestByteBasedEquivalent().GetName());
  EXPECT_STREQ(
      "windows-1252",
      TextEncoding("windows-1252").ClosestByteBasedEquivalent().GetName());
  EXPECT_STREQ("GBK",
               TextEncoding("gbk").ClosestByteBasedEquivalent().GetName());
  EXPECT_EQ(nullptr,
            TextEncoding("utf-7").ClosestByteBasedEquivalent().GetName());
}

TEST(TextEncoding, EncodingForFormSubmission) {
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-8").EncodingForFormSubmission().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16").EncodingForFormSubmission().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16le").EncodingForFormSubmission().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16be").EncodingForFormSubmission().GetName());
  EXPECT_STREQ(
      "windows-1252",
      TextEncoding("windows-1252").EncodingForFormSubmission().GetName());
  EXPECT_STREQ("GBK",
               TextEncoding("gbk").EncodingForFormSubmission().GetName());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-7").EncodingForFormSubmission().GetName());
}
}
}
