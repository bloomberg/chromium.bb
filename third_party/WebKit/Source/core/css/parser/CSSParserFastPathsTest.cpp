// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserFastPaths.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using namespace cssvalue;

TEST(CSSParserFastPathsTest, ParseKeyword) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyFloat, "left", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsIdentifierValue());
  CSSIdentifierValue* identifier_value = ToCSSIdentifierValue(value);
  EXPECT_EQ(CSSValueLeft, identifier_value->GetValueID());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyFloat, "foo",
                                              kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}
TEST(CSSParserFastPathsTest, ParseInitialAndInheritKeyword) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyMarginTop, "inherit", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInheritedValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyMarginRight, "InHeriT",
                                              kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInheritedValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyMarginBottom,
                                              "initial", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInitialValue());
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyMarginLeft, "IniTiaL",
                                              kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->IsInitialValue());
  // Fast path doesn't handle short hands.
  value = CSSParserFastPaths::MaybeParseValue(CSSPropertyMargin, "initial",
                                              kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseTransform) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, "translate(5.5px, 5px)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ("translate(5.5px, 5px)", value->CssText());

  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, "translate3d(5px, 5px, 10.1px)", kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ("translate3d(5px, 5px, 10.1px)", value->CssText());
}

TEST(CSSParserFastPathsTest, ParseComplexTransform) {
  // Random whitespace is on purpose.
  static const char* k_complex_transform =
      "translateX(5px) "
      "translateZ(20.5px)   "
      "translateY(10px) "
      "scale3d(0.5, 1, 0.7)   "
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)   ";
  static const char* k_complex_transform_normalized =
      "translateX(5px) "
      "translateZ(20.5px) "
      "translateY(10px) "
      "scale3d(0.5, 1, 0.7) "
      "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)";
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, k_complex_transform, kHTMLStandardMode);
  ASSERT_NE(nullptr, value);
  ASSERT_TRUE(value->IsValueList());
  ASSERT_EQ(k_complex_transform_normalized, value->CssText());
}

TEST(CSSParserFastPathsTest, ParseTransformNotFastPath) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, "rotateX(1deg)", kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, "translateZ(1px) rotateX(1deg)", kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseInvalidTransform) {
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, "rotateX(1deg", kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
  value = CSSParserFastPaths::MaybeParseValue(
      CSSPropertyTransform, "translateZ(1px) (1px, 1px) rotateX(1deg",
      kHTMLStandardMode);
  ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseColorWithLargeAlpha) {
  CSSValue* value = CSSParserFastPaths::ParseColor("rgba(0,0,0,1893205797.13)",
                                                   kHTMLStandardMode);
  EXPECT_NE(nullptr, value);
  EXPECT_TRUE(value->IsColorValue());
  EXPECT_EQ(Color::kBlack, ToCSSColorValue(*value).Value());
}

}  // namespace blink
