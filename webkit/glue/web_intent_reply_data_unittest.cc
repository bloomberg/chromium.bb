// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_reply_data.h"

using webkit_glue::WebIntentReply;

namespace {

TEST(WebIntentReplyDataTest, DefaultValues) {
  WebIntentReply reply;
  EXPECT_EQ(webkit_glue::WEB_INTENT_REPLY_INVALID, reply.type);
  EXPECT_EQ(string16(), reply.data);
  EXPECT_EQ(base::FilePath(), reply.data_file);
  EXPECT_EQ(-1, reply.data_file_size);
}

TEST(WebIntentReplyDataTest, Equality) {
  WebIntentReply data_a(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      ASCIIToUTF16("poodles"));

  WebIntentReply data_b(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      ASCIIToUTF16("poodles"));

  WebIntentReply data_c(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      ASCIIToUTF16("hamfancy"));

  EXPECT_EQ(data_a, data_b);
  EXPECT_FALSE(data_a == data_c || data_b == data_c);

  WebIntentReply file_a(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      base::FilePath(),
      22);

  WebIntentReply file_b(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      base::FilePath(),
      22);

  WebIntentReply file_c(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      base::FilePath(),
      17);

  EXPECT_EQ(file_a, file_b);
  EXPECT_FALSE(file_a == file_c || file_b == file_c);
}

}  // namespace
