// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ParsedContentType.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ParsedContentTypeTest, MimeTypeWithoutCharset) {
  ParsedContentType t("text/plain");

  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ(String(), t.charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithCharSet) {
  ParsedContentType t(" text/plain  ;  x=y;charset=utf-8 ");

  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ("utf-8", t.charset());
}

TEST(ParsedContentTypeTest, MimeTypeWithQuotedCharSet) {
  ParsedContentType t("text/plain; \"charset\"=\"x=y;y=z; ;;\"");

  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ("x=y;y=z; ;;", t.charset());
}

// TODO(yhirano): Add tests for escaped quotation: it's currently
// mis-implemented.

TEST(ParsedContentTypeTest, InvalidMimeTypeWithoutCharset) {
  ParsedContentType t(" ");

  EXPECT_EQ(String(), t.mimeType());
  EXPECT_EQ(String(), t.charset());
}

TEST(ParsedContentTypeTest, InvalidMimeTypeWithCharset) {
  ParsedContentType t("text/plain; charset;");

  EXPECT_EQ("text/plain", t.mimeType());
  EXPECT_EQ(String(), t.charset());
}

TEST(ParsedContentTypeTest, Validity) {
  EXPECT_TRUE(isValidContentType("text/plain"));
  EXPECT_TRUE(isValidContentType("text/plain; charset=utf-8"));
  EXPECT_TRUE(isValidContentType("  text/plain  "));
  EXPECT_TRUE(isValidContentType(" text/plain;charset=utf-8  "));
  EXPECT_TRUE(isValidContentType("unknown/unknown"));
  EXPECT_TRUE(isValidContentType("unknown/unknown; charset=unknown"));
  EXPECT_TRUE(isValidContentType("x/y;\"z=\\\"q;t\"=\"ttx&r=z;;kd==\""));

  EXPECT_FALSE(isValidContentType("text/plain\r"));
  EXPECT_FALSE(isValidContentType("text/plain\n"));
  EXPECT_FALSE(isValidContentType(""));
  EXPECT_FALSE(isValidContentType("   "));
  EXPECT_FALSE(isValidContentType("text/plain;"));
  EXPECT_FALSE(isValidContentType("text/plain;  "));
  EXPECT_FALSE(isValidContentType("text/plain; charset"));
  EXPECT_FALSE(isValidContentType("text/plain; charset;"));
  EXPECT_FALSE(isValidContentType("x/y;\"xx"));
  EXPECT_FALSE(isValidContentType("x/y;\"xx=y"));

  // TODO(yhirano): Add tests for non-tokens. They are currently accepted.
}

TEST(ParsedContentTypeTest, ParameterName) {
  String input = "x; y=z; y=u;  t=r;s=x;\"Q\"=U;\"T\"=S;\"z u\"=\"q a\"";

  ParsedContentType t(input);

  EXPECT_EQ(6u, t.parameterCount());
  EXPECT_EQ(String(), t.parameterValueForName("a"));
  EXPECT_EQ(String(), t.parameterValueForName("x"));
  EXPECT_EQ("u", t.parameterValueForName("y"));
  EXPECT_EQ("r", t.parameterValueForName("t"));
  EXPECT_EQ("x", t.parameterValueForName("s"));
  EXPECT_EQ("U", t.parameterValueForName("Q"));
  EXPECT_EQ("S", t.parameterValueForName("T"));
  EXPECT_EQ("q a", t.parameterValueForName("z u"));

  // TODO(yhirano): Case-sensitivity is mis-implemented.
  // TODO(yhirano): Add tests for escaped quotations.
  // TODO(yhirano): Leading spaces of a parameter value should be ignored.
  // TODO(yhirano): Trailing spaces of a parameter value should be ignored.
}

}  // namespace blink
