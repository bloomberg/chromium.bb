// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParser.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static int ComputeNumberOfTracks(const CSSValueList* value_list) {
  int number_of_tracks = 0;
  for (auto& value : *value_list) {
    if (value->IsGridLineNamesValue())
      continue;
    ++number_of_tracks;
  }
  return number_of_tracks;
}

TEST(CSSPropertyParserTest, CSSPaint_Functions) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(foo, func1(1px, 3px), red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(foo, func1(1px, 3px), red)");
}

TEST(CSSPropertyParserTest, CSSPaint_NoArguments) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(foo)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(foo)");
}

TEST(CSSPropertyParserTest, CSSPaint_ValidArguments) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(bar, 10px, red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(value->CssText(), "paint(bar, 10px, red)");
}

TEST(CSSPropertyParserTest, CSSPaint_InvalidFormat) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(foo bar)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  // Illegal format should not be parsed.
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, CSSPaint_TrailingComma) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(bar, 10px, red,)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, CSSPaint_PaintArgumentsDiabled) {
  ScopedCSSPaintAPIArgumentsForTest css_paint_api_arguments(false);
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundImage, "paint(bar, 10px, red)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, GridTrackLimit1) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(999, 20px)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit2) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(999, 20px)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit3) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(1000000, 10%)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit4) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(1000000, 10%)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit5) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(1000000, [first] min-content [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit6) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(1000000, [first] min-content [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit7) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(1000001, auto)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit8) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(1000001, auto)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit9) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(400000, 2em minmax(10px, max-content) 0.5fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit10) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(400000, 2em minmax(10px, max-content) 0.5fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 999);
}

TEST(CSSPropertyParserTest, GridTrackLimit11) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

TEST(CSSPropertyParserTest, GridTrackLimit12) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(600000, [first] 3vh 10% 2fr [nav] 10px auto 1fr 6em [last])",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

TEST(CSSPropertyParserTest, GridTrackLimit13) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns, "repeat(100000000000000000000, 10% 1fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit14) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows, "repeat(100000000000000000000, 10% 1fr)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 1000);
}

TEST(CSSPropertyParserTest, GridTrackLimit15) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateColumns,
      "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

TEST(CSSPropertyParserTest, GridTrackLimit16) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridTemplateRows,
      "repeat(100000000000000000000, 10% 5em 1fr auto auto 15px min-content)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsValueList());
  EXPECT_EQ(ComputeNumberOfTracks(ToCSSValueList(value)), 994);
}

static int GetGridPositionInteger(const CSSValue& value) {
  DCHECK(value.IsValueList());
  const auto& list = ToCSSValueList(value);
  DCHECK_EQ(list.length(), static_cast<size_t>(1));
  const auto& primitive_value = ToCSSPrimitiveValue(list.Item(0));
  DCHECK(primitive_value.IsNumber());
  return primitive_value.GetIntValue();
}

TEST(CSSPropertyParserTest, GridPositionLimit1) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnStart, "999",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 999);
}

TEST(CSSPropertyParserTest, GridPositionLimit2) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnEnd, "1000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit3) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowStart, "1000001",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit4) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowEnd, "5000000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), 1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit5) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnStart, "-999",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -999);
}

TEST(CSSPropertyParserTest, GridPositionLimit6) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridColumnEnd, "-1000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit7) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowStart, "-1000001",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000);
}

TEST(CSSPropertyParserTest, GridPositionLimit8) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyGridRowEnd, "-5000000000",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  DCHECK(value);
  EXPECT_EQ(GetGridPositionInteger(*value), -1000);
}

TEST(CSSPropertyParserTest, ColorFunction) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundColor, "rgba(0, 0, 0, 1)",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_TRUE(value);
  EXPECT_EQ(Color::kBlack, cssvalue::ToCSSColorValue(*value).Value());
}

TEST(CSSPropertyParserTest, IncompleteColor) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyBackgroundColor, "rgba(123 45",
      StrictCSSParserContext(SecureContextMode::kSecureContext));
  ASSERT_FALSE(value);
}

TEST(CSSPropertyParserTest, ClipPathEllipse) {
  std::unique_ptr<DummyPageHolder> dummy_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  Document* doc = &dummy_holder->GetDocument();
  CSSParserContext* context = CSSParserContext::Create(
      kHTMLStandardMode, SecureContextMode::kSecureContext,
      CSSParserContext::kDynamicProfile, doc);

  CSSParser::ParseSingleValue(CSSPropertyClipPath,
                              "ellipse(1px 2px at invalid)", context);

  EXPECT_FALSE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseTwoRadius));
  CSSParser::ParseSingleValue(CSSPropertyClipPath, "ellipse(1px 2px)", context);
  EXPECT_TRUE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseTwoRadius));

  EXPECT_FALSE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseOneRadius));
  CSSParser::ParseSingleValue(CSSPropertyClipPath, "ellipse(1px)", context);
  EXPECT_TRUE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseOneRadius));

  EXPECT_FALSE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseNoRadius));
  CSSParser::ParseSingleValue(CSSPropertyClipPath, "ellipse()", context);
  EXPECT_TRUE(
      UseCounter::IsCounted(*doc, WebFeature::kBasicShapeEllipseNoRadius));
}

}  // namespace blink
