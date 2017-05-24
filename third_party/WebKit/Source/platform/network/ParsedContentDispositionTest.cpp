// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ParsedContentDisposition.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentDisposition::Mode;

bool IsValid(const String& input, Mode mode = Mode::kNormal) {
  return ParsedContentDisposition(input, mode).IsValid();
}

TEST(ParsedContentDispositionTest, TypeWithoutFilename) {
  ParsedContentDisposition t("attachment");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ(String(), t.Filename());
}

TEST(ParsedContentDispositionTest, TypeWithFilename) {
  ParsedContentDisposition t("  attachment  ;  x=y; filename = file1 ");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ("file1", t.Filename());
}

TEST(ParsedContentDispositionTest, TypeWithQuotedFilename) {
  ParsedContentDisposition t("attachment; filename=\"x=y;y=\\\"\\pz; ;;\"");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ("x=y;y=\"pz; ;;", t.Filename());
}

TEST(ParsedContentDispositionTest, InvalidTypeWithoutFilename) {
  ParsedContentDisposition t(" ");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(String(), t.Type());
  EXPECT_EQ(String(), t.Filename());
}

TEST(ParsedContentDispositionTest, InvalidTypeWithFilename) {
  ParsedContentDisposition t("/attachment; filename=file1;");

  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(String(), t.Type());
  EXPECT_EQ(String(), t.Filename());
}

TEST(ParsedContentDispositionTest, CaseInsensitiveFilename) {
  ParsedContentDisposition t("attachment; fIlEnAmE=file1");

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ("attachment", t.Type());
  EXPECT_EQ("file1", t.Filename());
}

TEST(ParsedContentDispositionTest, Validity) {
  EXPECT_TRUE(IsValid("attachment"));
  EXPECT_TRUE(IsValid("attachment; filename=file1"));
  EXPECT_TRUE(IsValid("attachment; filename*=UTF-8'en'file1"));
  EXPECT_TRUE(IsValid(" attachment ;filename=file1 "));
  EXPECT_TRUE(IsValid("  attachment  "));
  EXPECT_TRUE(IsValid("unknown-unknown"));
  EXPECT_TRUE(IsValid("unknown-unknown; unknown=unknown"));

  EXPECT_FALSE(IsValid("A/B"));
  EXPECT_FALSE(IsValid("attachment\r"));
  EXPECT_FALSE(IsValid("attachment\n"));
  EXPECT_FALSE(IsValid("attachment filename=file1"));
  EXPECT_FALSE(IsValid("attachment;filename=file1;"));
  EXPECT_FALSE(IsValid(""));
  EXPECT_FALSE(IsValid("   "));
  EXPECT_FALSE(IsValid("\"x\""));
  EXPECT_FALSE(IsValid("attachment;"));
  EXPECT_FALSE(IsValid("attachment;  "));
  EXPECT_FALSE(IsValid("attachment; filename"));
  EXPECT_FALSE(IsValid("attachment; filename;"));
}

}  // namespace

}  // namespace blink
