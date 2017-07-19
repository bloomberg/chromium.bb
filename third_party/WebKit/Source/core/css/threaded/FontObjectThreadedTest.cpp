// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/FilterOperationResolver.h"

#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/resolver/FontStyleResolver.h"
#include "core/css/threaded/MultiThreadedTestUtil.h"
#include "platform/Language.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"
#include "platform/fonts/shaping/CachingWordShapeIterator.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/testing/FontTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::testing::CreateTestFont;

namespace blink {

TSAN_TEST(FontObjectThreadedTest, Language) {
  RunOnThreads([]() { EXPECT_EQ(DefaultLanguage(), "en-US"); });
}

TSAN_TEST(FontObjectThreadedTest, GetFontDefinition) {
  RunOnThreads([]() {
    MutableStylePropertySet* style =
        MutableStylePropertySet::Create(kHTMLStandardMode);
    CSSParser::ParseValue(style, CSSPropertyFont, "15px Ahem", true);

    FontDescription desc = FontStyleResolver::ComputeFont(*style);

    EXPECT_EQ(desc.SpecifiedSize(), 15);
    EXPECT_EQ(desc.ComputedSize(), 15);
    EXPECT_EQ(desc.Family().Family(), "Ahem");
  });
}

TSAN_TEST(FontObjectThreadedTest, GetDefaultFontData) {
  callbacks_per_thread_ = 30;
  num_threads_ = 5;
  RunOnThreads([]() {
    for (FontDescription::GenericFamilyType family_type :
         {FontDescription::kStandardFamily, FontDescription::kSerifFamily,
          FontDescription::kSansSerifFamily, FontDescription::kMonospaceFamily,
          FontDescription::kCursiveFamily, FontDescription::kFantasyFamily,
          FontDescription::kPictographFamily}) {
      FontDescription font_description;
      font_description.SetComputedSize(12.0);
      font_description.SetLocale(LayoutLocale::Get("en"));
      ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
      font_description.SetGenericFamily(family_type);

      Font font = Font(font_description);
      font.Update(nullptr);

      ASSERT_TRUE(font.PrimaryFont());
    }
  });
}

// This test passes by not crashing TSAN.
TSAN_TEST(FontObjectThreadedTest, FontSelector) {
  RunOnThreads([]() {
    Font font =
        CreateTestFont("Ahem", testing::CoreTestDataPath("Ahem.ttf"), 16);
  });
}

TSAN_TEST(FontObjectThreadedTest, TextIntercepts) {
  callbacks_per_thread_ = 10;
  RunOnThreads([]() {
    Font font =
        CreateTestFont("Ahem", testing::CoreTestDataPath("Ahem.ttf"), 16);
    // A sequence of LATIN CAPITAL LETTER E WITH ACUTE and LATIN SMALL LETTER P
    // characters. E ACUTES are squares above the baseline in Ahem, while p's
    // are rectangles below the baseline.
    UChar ahem_above_below_baseline_string[] = {0xc9, 0x70, 0xc9, 0x70, 0xc9,
                                                0x70, 0xc9, 0x70, 0xc9};
    TextRun ahem_above_below_baseline(ahem_above_below_baseline_string, 9);
    TextRunPaintInfo text_run_paint_info(ahem_above_below_baseline);
    PaintFlags default_paint;
    float device_scale_factor = 1;
    std::tuple<float, float> below_baseline_bounds = std::make_tuple(2, 4);
    Vector<Font::TextIntercept> text_intercepts;

    // 4 intercept ranges for below baseline p glyphs in the test string
    font.GetTextIntercepts(text_run_paint_info, device_scale_factor,
                           default_paint, below_baseline_bounds,
                           text_intercepts);
    EXPECT_EQ(text_intercepts.size(), 4u);
    for (auto text_intercept : text_intercepts) {
      EXPECT_GT(text_intercept.end_, text_intercept.begin_);
    }

    std::tuple<float, float> above_baseline_bounds = std::make_tuple(-4, -2);
    // 5 intercept ranges for the above baseline E ACUTE glyphs
    font.GetTextIntercepts(text_run_paint_info, device_scale_factor,
                           default_paint, above_baseline_bounds,
                           text_intercepts);
    EXPECT_EQ(text_intercepts.size(), 5u);
    for (auto text_intercept : text_intercepts) {
      EXPECT_GT(text_intercept.end_, text_intercept.begin_);
    }
  });
}

TSAN_TEST(FontObjectThreadedTest, WordShaperTest) {
  RunOnThreads([]() {
    FontDescription font_description;
    font_description.SetComputedSize(12.0);
    font_description.SetLocale(LayoutLocale::Get("en"));
    ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
    font_description.SetGenericFamily(FontDescription::kStandardFamily);

    Font font = Font(font_description);
    font.Update(nullptr);
    ASSERT_TRUE(font.CanShapeWordByWord());
    ShapeCache cache;

    TextRun text_run(reinterpret_cast<const LChar*>("ABC DEF."), 8);

    RefPtr<const ShapeResult> result;
    CachingWordShapeIterator iter(&cache, text_run, &font);

    ASSERT_TRUE(iter.Next(&result));
    EXPECT_EQ(0u, result->StartIndexForResult());
    EXPECT_EQ(3u, result->EndIndexForResult());

    ASSERT_TRUE(iter.Next(&result));
    EXPECT_EQ(0u, result->StartIndexForResult());
    EXPECT_EQ(1u, result->EndIndexForResult());

    ASSERT_TRUE(iter.Next(&result));
    EXPECT_EQ(0u, result->StartIndexForResult());
    EXPECT_EQ(4u, result->EndIndexForResult());

    ASSERT_FALSE(iter.Next(&result));
  });
}

}  // namespace blink
