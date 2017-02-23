// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ParsedContentType.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Mode = ParsedContentType::Mode;

bool isValid(const String& input, Mode mode = Mode::Normal) {
  return ParsedContentType(input, mode).isValid();
}

TEST(ParsedContentTypeTest, MimeTypeWithoutCharset) {
  ParsedContentType t("text/plain");

  EXPECT_TRUE(t.isValid());
  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ(String(), t.charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithCharSet) {
  ParsedContentType t("text /  plain  ;  x=y; charset = utf-8 ");

  EXPECT_TRUE(t.isValid());
  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ("utf-8", t.charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithQuotedCharSet) {
  ParsedContentType t("text/plain; charset=\"x=y;y=\\\"\\pz; ;;\"");

  EXPECT_TRUE(t.isValid());
  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ("x=y;y=\"pz; ;;", t.charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithoutCharset) {
  ParsedContentType t(" ");

  EXPECT_FALSE(t.isValid());
  EXPECT_EQ(String(), t.mimeType());
  EXPECT_EQ(String(), t.charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithCharset) {
  ParsedContentType t("text/plain; charset;");

  EXPECT_FALSE(t.isValid());
  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ(String(), t.charset());
}

TEST(ParsedContentTypeTest, CaseInsensitiveCharset) {
  ParsedContentType t("text/plain; cHaRsEt=utf-8");

  EXPECT_TRUE(t.isValid());
  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ("utf-8", t.charset());
}

TEST(ParsedContentTypeTest, Validity) {
  EXPECT_TRUE(isValid("text/plain"));
  EXPECT_TRUE(isValid("text/plain; charset=utf-8"));
  EXPECT_TRUE(isValid("  text/plain  "));
  EXPECT_TRUE(isValid(" text/plain;charset=utf-8  "));
  EXPECT_TRUE(isValid("unknown/unknown"));
  EXPECT_TRUE(isValid("unknown/unknown; charset=unknown"));
  EXPECT_TRUE(isValid("x/y;z=\"ttx&r=z;;\\u\\\"kd==\""));
  EXPECT_TRUE(isValid("x/y; z=\"\xff\""));

  EXPECT_FALSE(isValid("A"));
  EXPECT_FALSE(isValid("text/plain\r"));
  EXPECT_FALSE(isValid("text/plain\n"));
  EXPECT_FALSE(isValid("text/plain charset=utf-8"));
  EXPECT_FALSE(isValid("text/plain;charset=utf-8;"));
  EXPECT_FALSE(isValid(""));
  EXPECT_FALSE(isValid("   "));
  EXPECT_FALSE(isValid("\"x\""));
  EXPECT_FALSE(isValid("\"x\"/\"y\""));
  EXPECT_FALSE(isValid("x/\"y\""));
  EXPECT_FALSE(isValid("text/plain;"));
  EXPECT_FALSE(isValid("text/plain;  "));
  EXPECT_FALSE(isValid("text/plain; charset"));
  EXPECT_FALSE(isValid("text/plain; charset;"));
  EXPECT_FALSE(isValid("x/y;\"xx"));
  EXPECT_FALSE(isValid("x/y;\"xx=y"));
  EXPECT_FALSE(isValid("\"q\""));
  EXPECT_FALSE(isValid("x/y; \"z\"=u"));
  EXPECT_FALSE(isValid("x/y; z=\xff"));

  EXPECT_FALSE(isValid("x/y;z=q/t:()<>@,:\\/[]?"));
  EXPECT_TRUE(isValid("x/y;z=q/t:()<>@,:\\/[]?=", Mode::Relaxed));
  EXPECT_FALSE(isValid("x/y;z=q r", Mode::Relaxed));
  EXPECT_FALSE(isValid("x/y;z=q;r", Mode::Relaxed));
  EXPECT_FALSE(isValid("x/y;z=q\"r", Mode::Relaxed));
  EXPECT_FALSE(isValid("x/y; z=\xff", Mode::Relaxed));
}

TEST(ParsedContentTypeTest, ParameterName) {
  String input = "x/t; y=z  ; y= u ;  t=r;k= \"t \\u\\\"x\" ;Q=U;T=S";

  ParsedContentType t(input);

  EXPECT_TRUE(t.isValid());
  EXPECT_EQ(4u, t.parameterCount());
  EXPECT_EQ(String(), t.parameterValueForName("a"));
  EXPECT_EQ(String(), t.parameterValueForName("x"));
  EXPECT_EQ("u", t.parameterValueForName("y"));
  EXPECT_EQ("S", t.parameterValueForName("t"));
  EXPECT_EQ("t u\"x", t.parameterValueForName("k"));
  EXPECT_EQ("U", t.parameterValueForName("Q"));
  EXPECT_EQ("S", t.parameterValueForName("T"));

  String kelvin = String::fromUTF8("\xe2\x84\xaa");
  DCHECK_EQ(kelvin.lower(), "k");
  EXPECT_EQ(String(), t.parameterValueForName(kelvin));
}

TEST(ParsedContentTypeTest, RelaxedParameterName) {
  String input = "x/t; z=q/t:()<>@,:\\/[]?=;y=u";

  ParsedContentType t(input, Mode::Relaxed);

  EXPECT_TRUE(t.isValid());
  EXPECT_EQ(2u, t.parameterCount());
  EXPECT_EQ("q/t:()<>@,:\\/[]?=", t.parameterValueForName("z"));
  EXPECT_EQ("u", t.parameterValueForName("y"));
}

}  // namespace

}  // namespace blink
