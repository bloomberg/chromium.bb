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
  EXPECT_TRUE(IsValid("  text/plain  "));
  EXPECT_TRUE(IsValid(" text/plain;charset=utf-8  "));
  EXPECT_TRUE(IsValid("unknown/unknown"));
  EXPECT_TRUE(IsValid("unknown/unknown; charset=unknown"));
  EXPECT_TRUE(IsValid("x/y;z=\"ttx&r=z;;\\u\\\"kd==\""));
  EXPECT_TRUE(IsValid("x/y; z=\"\xff\""));

  EXPECT_FALSE(IsValid("A"));
  EXPECT_FALSE(IsValid("text/plain\r"));
  EXPECT_FALSE(IsValid("text/plain\n"));
  EXPECT_FALSE(IsValid("text/plain charset=utf-8"));
  EXPECT_FALSE(IsValid("text/plain;charset=utf-8;"));
  EXPECT_FALSE(IsValid(""));
  EXPECT_FALSE(IsValid("   "));
  EXPECT_FALSE(IsValid("\"x\""));
  EXPECT_FALSE(IsValid("\"x\"/\"y\""));
  EXPECT_FALSE(IsValid("x/\"y\""));
  EXPECT_FALSE(IsValid("text/plain;"));
  EXPECT_FALSE(IsValid("text/plain;  "));
  EXPECT_FALSE(IsValid("text/plain; charset"));
  EXPECT_FALSE(IsValid("text/plain; charset;"));
  EXPECT_FALSE(IsValid("x/y;\"xx"));
  EXPECT_FALSE(IsValid("x/y;\"xx=y"));
  EXPECT_FALSE(IsValid("\"q\""));
  EXPECT_FALSE(IsValid("x/y; \"z\"=u"));
  EXPECT_FALSE(IsValid("x/y; z=\xff"));

  EXPECT_FALSE(IsValid("x/y;z=q/t:()<>@,:\\/[]?"));
  EXPECT_TRUE(IsValid("x/y;z=q/t:()<>@,:\\/[]?=", Mode::kRelaxed));
  EXPECT_FALSE(IsValid("x/y;z=q r", Mode::kRelaxed));
  EXPECT_FALSE(IsValid("x/y;z=q;r", Mode::kRelaxed));
  EXPECT_FALSE(IsValid("x/y;z=q\"r", Mode::kRelaxed));
  EXPECT_FALSE(IsValid("x/y; z=\xff", Mode::kRelaxed));
}

TEST(ParsedContentTypeTest, ParameterName) {
  String input = "x/t; y=z  ; y= u ;  t=r;k= \"t \\u\\\"x\" ;Q=U;T=S";

  ParsedContentType t(input);

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ(4u, t.ParameterCount());
  EXPECT_EQ(String(), t.ParameterValueForName("a"));
  EXPECT_EQ(String(), t.ParameterValueForName("x"));
  EXPECT_EQ("u", t.ParameterValueForName("y"));
  EXPECT_EQ("S", t.ParameterValueForName("t"));
  EXPECT_EQ("t u\"x", t.ParameterValueForName("k"));
  EXPECT_EQ("U", t.ParameterValueForName("Q"));
  EXPECT_EQ("S", t.ParameterValueForName("T"));

  String kelvin = String::FromUTF8("\xe2\x84\xaa");
  DCHECK_EQ(kelvin.Lower(), "k");
  EXPECT_EQ(String(), t.ParameterValueForName(kelvin));
}

TEST(ParsedContentTypeTest, RelaxedParameterName) {
  String input = "x/t; z=q/t:()<>@,:\\/[]?=;y=u";

  ParsedContentType t(input, Mode::kRelaxed);

  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ(2u, t.ParameterCount());
  EXPECT_EQ("q/t:()<>@,:\\/[]?=", t.ParameterValueForName("z"));
  EXPECT_EQ("u", t.ParameterValueForName("y"));
}

TEST(ParsedContentTypeTest, StrictParameterName) {
  EXPECT_TRUE(IsValid("text/plain", Mode::kStrict));
  EXPECT_TRUE(IsValid("text/plain; p1=a", Mode::kStrict));
  EXPECT_FALSE(IsValid("text/plain; p1=a; p1=b", Mode::kStrict));
  EXPECT_TRUE(IsValid("text/plain; p1=a; p2=b", Mode::kStrict));
}

}  // namespace

}  // namespace blink
