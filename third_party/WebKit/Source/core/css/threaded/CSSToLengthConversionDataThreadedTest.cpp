// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSToLengthConversionData.h"

#include "core/css/threaded/MultiThreadedTestUtil.h"
#include "platform/Length.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontDescription.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TSAN_TEST(CSSToLengthConversionDataThreadedTest, Construction) {
  RunOnThreads([]() {
    FontDescription fontDescription;
    Font font(fontDescription);
    CSSToLengthConversionData::FontSizes fontSizes(16, 16, &font);
    CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
    CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize,
                                             1);
  });
}

TSAN_TEST(CSSToLengthConversionDataThreadedTest, ConversionEm) {
  RunOnThreads([]() {
    FontDescription fontDescription;
    Font font(fontDescription);
    CSSToLengthConversionData::FontSizes fontSizes(16, 16, &font);
    CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
    CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize,
                                             1);

    CSSPrimitiveValue& value =
        *CSSPrimitiveValue::Create(3.14, CSSPrimitiveValue::UnitType::kEms);

    Length length = value.ConvertToLength(conversionData);
    EXPECT_EQ(length.Value(), 50.24f);
  });
}

TSAN_TEST(CSSToLengthConversionDataThreadedTest, ConversionPixel) {
  RunOnThreads([]() {
    FontDescription fontDescription;
    Font font(fontDescription);
    CSSToLengthConversionData::FontSizes fontSizes(16, 16, &font);
    CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
    CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize,
                                             1);

    CSSPrimitiveValue& value =
        *CSSPrimitiveValue::Create(44, CSSPrimitiveValue::UnitType::kPixels);

    Length length = value.ConvertToLength(conversionData);
    EXPECT_EQ(length.Value(), 44);
  });
}

TSAN_TEST(CSSToLengthConversionDataThreadedTest, ConversionViewport) {
  RunOnThreads([]() {
    FontDescription fontDescription;
    Font font(fontDescription);
    CSSToLengthConversionData::FontSizes fontSizes(16, 16, &font);
    CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
    CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize,
                                             1);

    CSSPrimitiveValue& value = *CSSPrimitiveValue::Create(
        1, CSSPrimitiveValue::UnitType::kViewportWidth);

    Length length = value.ConvertToLength(conversionData);
    EXPECT_EQ(length.Value(), 0);
  });
}

TSAN_TEST(CSSToLengthConversionDataThreadedTest, ConversionRem) {
  RunOnThreads([]() {
    FontDescription fontDescription;
    Font font(fontDescription);
    CSSToLengthConversionData::FontSizes fontSizes(16, 16, &font);
    CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
    CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize,
                                             1);

    CSSPrimitiveValue& value =
        *CSSPrimitiveValue::Create(1, CSSPrimitiveValue::UnitType::kRems);

    Length length = value.ConvertToLength(conversionData);
    EXPECT_EQ(length.Value(), 16);
  });
}

}  // namespace blink
