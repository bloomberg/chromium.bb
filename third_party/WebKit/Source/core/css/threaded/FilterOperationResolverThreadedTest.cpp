// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/FilterOperationResolver.h"

#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/threaded/MultiThreadedTestUtil.h"
#include "core/style/FilterOperation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TSAN_TEST(FilterOperationResolverThreadedTest, SimpleMatrixFilter) {
  RunOnThreads([]() {
    const CSSValue* value = CSSParser::ParseSingleValue(
        CSSPropertyFilter, "sepia(50%)",
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
    ASSERT_TRUE(value);

    FilterOperations fo =
        FilterOperationResolver::CreateOffscreenFilterOperations(*value);
    ASSERT_EQ(fo.size(), 1ul);
    EXPECT_EQ(*fo.at(0), *BasicColorMatrixFilterOperation::Create(
                             0.5, FilterOperation::SEPIA));
  });
}

TSAN_TEST(FilterOperationResolverThreadedTest, SimpleTransferFilter) {
  RunOnThreads([]() {
    const CSSValue* value = CSSParser::ParseSingleValue(
        CSSPropertyFilter, "brightness(50%)",
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
    ASSERT_TRUE(value);

    FilterOperations fo =
        FilterOperationResolver::CreateOffscreenFilterOperations(*value);
    ASSERT_EQ(fo.size(), 1ul);
    EXPECT_EQ(*fo.at(0), *BasicComponentTransferFilterOperation::Create(
                             0.5, FilterOperation::BRIGHTNESS));
  });
}

TSAN_TEST(FilterOperationResolverThreadedTest, SimpleBlurFilter) {
  RunOnThreads([]() {
    const CSSValue* value = CSSParser::ParseSingleValue(
        CSSPropertyFilter, "blur(10px)",
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
    ASSERT_TRUE(value);

    FilterOperations fo =
        FilterOperationResolver::CreateOffscreenFilterOperations(*value);
    ASSERT_EQ(fo.size(), 1ul);
    EXPECT_EQ(*fo.at(0),
              *BlurFilterOperation::Create(Length(10, LengthType::kFixed)));
  });
}

TSAN_TEST(FilterOperationResolverThreadedTest, SimpleDropShadow) {
  RunOnThreads([]() {
    const CSSValue* value = CSSParser::ParseSingleValue(
        CSSPropertyFilter, "drop-shadow(10px 5px 1px black)",
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
    ASSERT_TRUE(value);

    FilterOperations fo =
        FilterOperationResolver::CreateOffscreenFilterOperations(*value);
    ASSERT_EQ(fo.size(), 1ul);
    EXPECT_EQ(*fo.at(0), *DropShadowFilterOperation::Create(ShadowData(
                             FloatPoint(10, 5), 1, 0, ShadowStyle::kNormal,
                             StyleColor(Color::kBlack))));
  });
}

TSAN_TEST(FilterOperationResolverThreadedTest, CompoundFilter) {
  RunOnThreads([]() {
    const CSSValue* value = CSSParser::ParseSingleValue(
        CSSPropertyFilter, "sepia(50%) brightness(50%)",
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
    ASSERT_TRUE(value);

    FilterOperations fo =
        FilterOperationResolver::CreateOffscreenFilterOperations(*value);
    EXPECT_FALSE(fo.IsEmpty());
    ASSERT_EQ(fo.size(), 2ul);
    EXPECT_EQ(*fo.at(0), *BasicColorMatrixFilterOperation::Create(
                             0.5, FilterOperation::SEPIA));
    EXPECT_EQ(*fo.at(1), *BasicComponentTransferFilterOperation::Create(
                             0.5, FilterOperation::BRIGHTNESS));
  });
}

}  // namespace blink
