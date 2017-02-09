// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "platform/text/TextRun.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include <unicode/uscript.h>

namespace blink {

class HarfBuzzShaperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fontDescription.setComputedSize(12.0);
    font = Font(fontDescription);
    font.update(nullptr);
  }

  void TearDown() override {}

  FontCachePurgePreventer fontCachePurgePreventer;
  FontDescription fontDescription;
  Font font;
  unsigned startIndex = 0;
  unsigned numGlyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

static inline ShapeResultTestInfo* testInfo(RefPtr<ShapeResult>& result) {
  return static_cast<ShapeResultTestInfo*>(result.get());
}

static inline String to16Bit(const char* text, unsigned length) {
  return String::make16BitFrom8BitSource(reinterpret_cast<const LChar*>(text),
                                         length);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLatin) {
  String latinCommon = to16Bit("ABC DEF.", 8);
  HarfBuzzShaper shaper(latinCommon.characters16(), 8);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);

  ASSERT_EQ(1u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(8u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLeadingCommon) {
  String leadingCommon = to16Bit("... test", 8);
  HarfBuzzShaper shaper(leadingCommon.characters16(), 8);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);

  ASSERT_EQ(1u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(8u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsUnicodeVariants) {
  struct {
    const char* name;
    UChar string[4];
    unsigned length;
    hb_script_t script;
  } testlist[] = {
      {"Standard Variants text style", {0x30, 0xFE0E}, 2, HB_SCRIPT_COMMON},
      {"Standard Variants emoji style", {0x203C, 0xFE0F}, 2, HB_SCRIPT_COMMON},
      {"Standard Variants of Ideograph", {0x4FAE, 0xFE00}, 2, HB_SCRIPT_HAN},
      {"Ideographic Variants", {0x3402, 0xDB40, 0xDD00}, 3, HB_SCRIPT_HAN},
      {"Not-defined Variants", {0x41, 0xDB40, 0xDDEF}, 3, HB_SCRIPT_LATIN},
  };
  for (auto& test : testlist) {
    HarfBuzzShaper shaper(test.string, test.length);
    RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);

    EXPECT_EQ(1u, testInfo(result)->numberOfRunsForTesting()) << test.name;
    ASSERT_TRUE(
        testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script))
        << test.name;
    EXPECT_EQ(0u, startIndex) << test.name;
    if (numGlyphs == 2) {
// If the specified VS is not in the font, it's mapped to .notdef.
// then hb_ot_hide_default_ignorables() swaps it to a space with zero-advance.
// http://lists.freedesktop.org/archives/harfbuzz/2015-May/004888.html
#if !OS(MACOSX)
      EXPECT_EQ(testInfo(result)->fontDataForTesting(0)->spaceGlyph(),
                testInfo(result)->glyphForTesting(0, 1))
          << test.name;
#endif
      EXPECT_EQ(0.f, testInfo(result)->advanceForTesting(0, 1)) << test.name;
    } else {
      EXPECT_EQ(1u, numGlyphs) << test.name;
    }
    EXPECT_EQ(test.script, script) << test.name;
  }
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommon) {
  UChar devanagariCommonString[] = {0x915, 0x94d, 0x930, 0x28, 0x20, 0x29};
  String devanagariCommonLatin(devanagariCommonString, 6);
  HarfBuzzShaper shaper(devanagariCommonLatin.characters16(), 6);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);

  ASSERT_EQ(2u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(1, startIndex, numGlyphs, script));
  EXPECT_EQ(3u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommonLatinCommon) {
  UChar devanagariCommonLatinString[] = {0x915, 0x94d, 0x930, 0x20,
                                         0x61,  0x62,  0x2E};
  HarfBuzzShaper shaper(devanagariCommonLatinString, 7);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);

  ASSERT_EQ(3u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(1, startIndex, numGlyphs, script));
  EXPECT_EQ(3u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(2, startIndex, numGlyphs, script));
  EXPECT_EQ(4u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatin) {
  UChar mixedString[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(mixedString, 6);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);

  ASSERT_EQ(4u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(1, startIndex, numGlyphs, script));
  EXPECT_EQ(3u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_THAI, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(2, startIndex, numGlyphs, script));
  EXPECT_EQ(4u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_HAN, script);

  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(3, startIndex, numGlyphs, script));
  EXPECT_EQ(5u, startIndex);
  EXPECT_EQ(1u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatinTwice) {
  UChar mixedString[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(mixedString, 6);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kLtr);
  ASSERT_EQ(4u, testInfo(result)->numberOfRunsForTesting());

  // Shape again on the same shape object and check the number of runs.
  // Should be equal if no state was retained between shape calls.
  RefPtr<ShapeResult> result2 = shaper.shape(&font, TextDirection::kLtr);
  ASSERT_EQ(4u, testInfo(result2)->numberOfRunsForTesting());
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabic) {
  UChar arabicString[] = {0x628, 0x64A, 0x629};
  HarfBuzzShaper shaper(arabicString, 3);
  RefPtr<ShapeResult> result = shaper.shape(&font, TextDirection::kRtl);

  ASSERT_EQ(1u, testInfo(result)->numberOfRunsForTesting());
  ASSERT_TRUE(
      testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
  EXPECT_EQ(0u, startIndex);
  EXPECT_EQ(3u, numGlyphs);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script);
}

// This is a simplified test and doesn't accuratly reflect how the shape range
// is to be used. If you instead of the string you imagine the following HTML:
// <div>Hello <span>World</span>!</div>
// It better reflects the intended use where the range given to each shape call
// corresponds to the text content of a TextNode.
TEST_F(HarfBuzzShaperTest, ShapeLatinSegment) {
  String string = to16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.characters16(), 12);
  RefPtr<ShapeResult> combined = shaper.shape(&font, direction);
  RefPtr<ShapeResult> first = shaper.shape(&font, direction, 0, 6);
  RefPtr<ShapeResult> second = shaper.shape(&font, direction, 6, 11);
  RefPtr<ShapeResult> third = shaper.shape(&font, direction, 11, 12);

  HarfBuzzShaper shaper2(string.characters16(), 6);
  RefPtr<ShapeResult> firstReference = shaper2.shape(&font, direction);

  HarfBuzzShaper shaper3(string.characters16() + 6, 5);
  RefPtr<ShapeResult> secondReference = shaper3.shape(&font, direction);

  HarfBuzzShaper shaper4(string.characters16() + 11, 1);
  RefPtr<ShapeResult> thirdReference = shaper4.shape(&font, direction);

  // Width of each segment should be the same when shaped using start and end
  // offset as it is when shaping the three segments using separate shaper
  // instances.
  // A full pixel is needed for tolerance to account for kerning on some
  // platforms.
  ASSERT_NEAR(firstReference->width(), first->width(), 1);
  ASSERT_NEAR(secondReference->width(), second->width(), 1);
  ASSERT_NEAR(thirdReference->width(), third->width(), 1);

  // Width of shape results for the entire string should match the combined
  // shape results from the three segments.
  float totalWidth = first->width() + second->width() + third->width();
  ASSERT_NEAR(combined->width(), totalWidth, 1);
}

// Represents the case where a part of a cluster has a different color.
// <div>0x647<span style="color: red;">0x64A</span></div>
// This test requires context-aware shaping which hasn't been implemented yet.
// See crbug.com/689155
TEST_F(HarfBuzzShaperTest, DISABLED_ShapeArabicWithContext) {
  UChar arabicString[] = {0x647, 0x64A};
  HarfBuzzShaper shaper(arabicString, 2);

  RefPtr<ShapeResult> combined = shaper.shape(&font, TextDirection::kRtl);

  RefPtr<ShapeResult> first = shaper.shape(&font, TextDirection::kRtl, 0, 1);
  RefPtr<ShapeResult> second = shaper.shape(&font, TextDirection::kRtl, 1, 2);

  // Combined width should be the same when shaping the two characters
  // separately as when shaping them combined.
  ASSERT_NEAR(combined->width(), first->width() + second->width(), 0.1);
}

}  // namespace blink
