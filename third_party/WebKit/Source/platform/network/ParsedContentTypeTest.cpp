// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ParsedContentType.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentType::Mode;

bool IsValid(const String& input, Mode mode = Mode::kNormal) {
  return ParsedContentType(input, mode).IsValid();
}

TEST(ParsedContentTypeTest, MimeTypeWithoutCharset) {
  ParsedContentType t("text/plain");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ(String(), t.Charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithCharSet) {
  ParsedContentType t("text /  plain  ;  x=y; charset = utf-8 ");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ("utf-8", t.Charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithQuotedCharSet) {
  ParsedContentType t("text/plain; charset=\"x=y;y=\\\"\\pz; ;;\"");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ("x=y;y=\"pz; ;;", t.Charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithoutCharset) {
  ParsedContentType t(" ");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(String(), t.MimeType());
  EXPECT_EQ(String(), t.Charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithCharset) {
  ParsedContentType t("text/plain; charset;");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ(String(), t.Charset());
}

TEST(ParsedContentTypeTest, CaseInsensitiveCharset) {
  ParsedContentType t("text/plain; cHaRsEt=utf-8");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("text/plain", t.MimeType());
  EXPECT_EQ("utf-8", t.Charset());
}

TEST(ParsedContentTypeTest, Validity) {
  EXPECT_TRUE(IsValid("text/plain"));
  EXPECT_TRUE(IsValid("text/plain; charset=utf-8"));
  EXPECT_TRUE(IsValid(" text/plain ;charset=utf-8  "));
  EXPECT_TRUE(IsValid("  text/plain  "));
  EXPECT_TRUE(IsValid("unknown/unknown"));
  EXPECT_TRUE(IsValid("unknown/unknown; charset=unknown"));

  EXPECT_FALSE(IsValid("A"));
  EXPECT_FALSE(IsValid("text/plain\r"));
  EXPECT_FALSE(IsValid("text/plain\n"));
  EXPECT_FALSE(IsValid("text/plain charset=utf-8"));
  EXPECT_FALSE(IsValid("text/plain;charset=utf-8;"));
  EXPECT_FALSE(IsValid(""));
  EXPECT_FALSE(IsValid("   "));
  EXPECT_FALSE(IsValid("\"x\""));
  EXPECT_FALSE(IsValid("\"x\"/\"y\""));
  EXPECT_FALSE(IsValid("\"x\"/y"));
  EXPECT_FALSE(IsValid("x/\"y\""));
  EXPECT_FALSE(IsValid("text/plain;"));
  EXPECT_FALSE(IsValid("text/plain;  "));
  EXPECT_FALSE(IsValid("text/plain; charset"));
  EXPECT_FALSE(IsValid("text/plain; charset;"));
}

}  // namespace

}  // namespace blink
