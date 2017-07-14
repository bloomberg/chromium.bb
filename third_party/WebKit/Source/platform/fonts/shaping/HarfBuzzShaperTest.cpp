// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include <unicode/uscript.h>

#include "build/build_config.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontTestUtilities.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextRun.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HarfBuzzShaperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font = Font(font_description);
    font.Update(nullptr);
  }

  void TearDown() override {}

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font;
  unsigned start_index = 0;
  unsigned num_characters = 0;
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

static inline ShapeResultTestInfo* TestInfo(RefPtr<ShapeResult>& result) {
  return static_cast<ShapeResultTestInfo*>(result.Get());
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLatin) {
  String latin_common = To16Bit("ABC DEF.", 8);
  HarfBuzzShaper shaper(latin_common.Characters16(), 8);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(8u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLeadingCommon) {
  String leading_common = To16Bit("... test", 8);
  HarfBuzzShaper shaper(leading_common.Characters16(), 8);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(8u, num_glyphs);
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
    RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

    EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting()) << test.name;
    ASSERT_TRUE(
        TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script))
        << test.name;
    EXPECT_EQ(0u, start_index) << test.name;
    if (num_glyphs == 2) {
// If the specified VS is not in the font, it's mapped to .notdef.
// then hb_ot_hide_default_ignorables() swaps it to a space with zero-advance.
// http://lists.freedesktop.org/archives/harfbuzz/2015-May/004888.html
#if !defined(OS_MACOSX)
      EXPECT_EQ(TestInfo(result)->FontDataForTesting(0)->SpaceGlyph(),
                TestInfo(result)->GlyphForTesting(0, 1))
          << test.name;
#endif
      EXPECT_EQ(0.f, TestInfo(result)->AdvanceForTesting(0, 1)) << test.name;
    } else {
      EXPECT_EQ(1u, num_glyphs) << test.name;
    }
    EXPECT_EQ(test.script, script) << test.name;
  }
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommon) {
  UChar devanagari_common_string[] = {0x915, 0x94d, 0x930, 0x28, 0x20, 0x29};
  String devanagari_common_latin(devanagari_common_string, 6);
  HarfBuzzShaper shaper(devanagari_common_latin.Characters16(), 6);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  EXPECT_EQ(2u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(1, start_index, num_glyphs, script));
  EXPECT_EQ(3u, start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommonLatinCommon) {
  UChar devanagari_common_latin_string[] = {0x915, 0x94d, 0x930, 0x20,
                                            0x61,  0x62,  0x2E};
  HarfBuzzShaper shaper(devanagari_common_latin_string, 7);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  EXPECT_EQ(3u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(1, start_index, num_glyphs, script));
  EXPECT_EQ(3u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script);

  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(2, start_index, num_glyphs, script));
  EXPECT_EQ(4u, start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatin) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(mixed_string, 6);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  EXPECT_EQ(4u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script);

  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(1, start_index, num_glyphs, script));
  EXPECT_EQ(3u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_THAI, script);

  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(2, start_index, num_glyphs, script));
  EXPECT_EQ(4u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_HAN, script);

  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(3, start_index, num_glyphs, script));
  EXPECT_EQ(5u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatinTwice) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(mixed_string, 6);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);
  EXPECT_EQ(4u, TestInfo(result)->NumberOfRunsForTesting());

  // Shape again on the same shape object and check the number of runs.
  // Should be equal if no state was retained between shape calls.
  RefPtr<ShapeResult> result2 = shaper.Shape(&font, TextDirection::kLtr);
  EXPECT_EQ(4u, TestInfo(result2)->NumberOfRunsForTesting());
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  HarfBuzzShaper shaper(arabic_string, 3);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kRtl);

  EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script);
}

// This is a simplified test and doesn't accuratly reflect how the shape range
// is to be used. If you instead of the string you imagine the following HTML:
// <div>Hello <span>World</span>!</div>
// It better reflects the intended use where the range given to each shape call
// corresponds to the text content of a TextNode.
TEST_F(HarfBuzzShaperTest, ShapeLatinSegment) {
  String string = To16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 12);
  RefPtr<ShapeResult> combined = shaper.Shape(&font, direction);
  RefPtr<ShapeResult> first = shaper.Shape(&font, direction, 0, 6);
  RefPtr<ShapeResult> second = shaper.Shape(&font, direction, 6, 11);
  RefPtr<ShapeResult> third = shaper.Shape(&font, direction, 11, 12);

  ASSERT_TRUE(TestInfo(first)->RunInfoForTesting(0, start_index, num_characters,
                                                 num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(6u, num_characters);
  ASSERT_TRUE(TestInfo(second)->RunInfoForTesting(
      0, start_index, num_characters, num_glyphs, script));
  EXPECT_EQ(6u, start_index);
  EXPECT_EQ(5u, num_characters);
  ASSERT_TRUE(TestInfo(third)->RunInfoForTesting(0, start_index, num_characters,
                                                 num_glyphs, script));
  EXPECT_EQ(11u, start_index);
  EXPECT_EQ(1u, num_characters);

  HarfBuzzShaper shaper2(string.Characters16(), 6);
  RefPtr<ShapeResult> first_reference = shaper2.Shape(&font, direction);

  HarfBuzzShaper shaper3(string.Characters16() + 6, 5);
  RefPtr<ShapeResult> second_reference = shaper3.Shape(&font, direction);

  HarfBuzzShaper shaper4(string.Characters16() + 11, 1);
  RefPtr<ShapeResult> third_reference = shaper4.Shape(&font, direction);

  // Width of each segment should be the same when shaped using start and end
  // offset as it is when shaping the three segments using separate shaper
  // instances.
  // A full pixel is needed for tolerance to account for kerning on some
  // platforms.
  ASSERT_NEAR(first_reference->Width(), first->Width(), 1);
  ASSERT_NEAR(second_reference->Width(), second->Width(), 1);
  ASSERT_NEAR(third_reference->Width(), third->Width(), 1);

  // Width of shape results for the entire string should match the combined
  // shape results from the three segments.
  float total_width = first->Width() + second->Width() + third->Width();
  ASSERT_NEAR(combined->Width(), total_width, 1);
}

// Represents the case where a part of a cluster has a different color.
// <div>0x647<span style="color: red;">0x64A</span></div>
// This test requires context-aware shaping which hasn't been implemented yet.
// See crbug.com/689155
TEST_F(HarfBuzzShaperTest, DISABLED_ShapeArabicWithContext) {
  UChar arabic_string[] = {0x647, 0x64A};
  HarfBuzzShaper shaper(arabic_string, 2);

  RefPtr<ShapeResult> combined = shaper.Shape(&font, TextDirection::kRtl);

  RefPtr<ShapeResult> first = shaper.Shape(&font, TextDirection::kRtl, 0, 1);
  RefPtr<ShapeResult> second = shaper.Shape(&font, TextDirection::kRtl, 1, 2);

  // Combined width should be the same when shaping the two characters
  // separately as when shaping them combined.
  ASSERT_NEAR(combined->Width(), first->Width() + second->Width(), 0.1);
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetLatin) {
  String string = To16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 12);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);
  RefPtr<ShapeResult> first = shaper.Shape(&font, direction, 0, 5);    // Hello
  RefPtr<ShapeResult> second = shaper.Shape(&font, direction, 6, 11);  // World

  EXPECT_EQ(0.0f, result->PositionForOffset(0));
  ASSERT_NEAR(first->Width(), result->PositionForOffset(5), 1);
  ASSERT_NEAR(second->Width(),
              result->PositionForOffset(11) - result->PositionForOffset(6), 1);
  ASSERT_NEAR(result->Width(), result->PositionForOffset(12), 0.1);
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(arabic_string, 3);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  EXPECT_EQ(0.0f, result->PositionForOffset(3));
  ASSERT_NEAR(result->Width(), result->PositionForOffset(0), 0.1);
}

// A Value-Parameterized Test class to test OffsetForPosition() with
// |include_partial_glyphs| parameter.
class IncludePartialGlyphs : public HarfBuzzShaperTest,
                             public ::testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_CASE_P(OffsetForPositionTest,
                        IncludePartialGlyphs,
                        ::testing::Bool());

TEST_P(IncludePartialGlyphs, OffsetForPositionMatchesPositionForOffsetLatin) {
  String string = To16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 12);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  bool include_partial_glyphs = GetParam();
  EXPECT_EQ(0u, result->OffsetForPosition(result->PositionForOffset(0),
                                          include_partial_glyphs));
  EXPECT_EQ(1u, result->OffsetForPosition(result->PositionForOffset(1),
                                          include_partial_glyphs));
  EXPECT_EQ(2u, result->OffsetForPosition(result->PositionForOffset(2),
                                          include_partial_glyphs));
  EXPECT_EQ(3u, result->OffsetForPosition(result->PositionForOffset(3),
                                          include_partial_glyphs));
  EXPECT_EQ(4u, result->OffsetForPosition(result->PositionForOffset(4),
                                          include_partial_glyphs));
  EXPECT_EQ(5u, result->OffsetForPosition(result->PositionForOffset(5),
                                          include_partial_glyphs));
  EXPECT_EQ(6u, result->OffsetForPosition(result->PositionForOffset(6),
                                          include_partial_glyphs));
  EXPECT_EQ(7u, result->OffsetForPosition(result->PositionForOffset(7),
                                          include_partial_glyphs));
  EXPECT_EQ(8u, result->OffsetForPosition(result->PositionForOffset(8),
                                          include_partial_glyphs));
  EXPECT_EQ(9u, result->OffsetForPosition(result->PositionForOffset(9),
                                          include_partial_glyphs));
  EXPECT_EQ(10u, result->OffsetForPosition(result->PositionForOffset(10),
                                           include_partial_glyphs));
  EXPECT_EQ(11u, result->OffsetForPosition(result->PositionForOffset(11),
                                           include_partial_glyphs));
  EXPECT_EQ(12u, result->OffsetForPosition(result->PositionForOffset(12),
                                           include_partial_glyphs));
}

TEST_P(IncludePartialGlyphs, OffsetForPositionMatchesPositionForOffsetArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(arabic_string, 3);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  bool include_partial_glyphs = GetParam();
  EXPECT_EQ(0u, result->OffsetForPosition(result->PositionForOffset(0),
                                          include_partial_glyphs));
  EXPECT_EQ(1u, result->OffsetForPosition(result->PositionForOffset(1),
                                          include_partial_glyphs));
  EXPECT_EQ(2u, result->OffsetForPosition(result->PositionForOffset(2),
                                          include_partial_glyphs));
  EXPECT_EQ(3u, result->OffsetForPosition(result->PositionForOffset(3),
                                          include_partial_glyphs));
}

TEST_P(IncludePartialGlyphs, OffsetForPositionMatchesPositionForOffsetMixed) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(mixed_string, 6);
  RefPtr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  bool include_partial_glyphs = GetParam();
  EXPECT_EQ(0u, result->OffsetForPosition(result->PositionForOffset(0),
                                          include_partial_glyphs));
  EXPECT_EQ(1u, result->OffsetForPosition(result->PositionForOffset(1),
                                          include_partial_glyphs));
  EXPECT_EQ(2u, result->OffsetForPosition(result->PositionForOffset(2),
                                          include_partial_glyphs));
  EXPECT_EQ(3u, result->OffsetForPosition(result->PositionForOffset(3),
                                          include_partial_glyphs));
  EXPECT_EQ(4u, result->OffsetForPosition(result->PositionForOffset(4),
                                          include_partial_glyphs));
  EXPECT_EQ(5u, result->OffsetForPosition(result->PositionForOffset(5),
                                          include_partial_glyphs));
  EXPECT_EQ(6u, result->OffsetForPosition(result->PositionForOffset(6),
                                          include_partial_glyphs));
}

TEST_F(HarfBuzzShaperTest, ShapeResultCopyRangeIntoLatin) {
  String string = To16Bit("Testing ShapeResult::createSubRun", 33);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 33);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  RefPtr<ShapeResult> composite_result =
      ShapeResult::Create(&font, 0, direction);
  result->CopyRange(0, 10, composite_result.Get());
  result->CopyRange(10, 20, composite_result.Get());
  result->CopyRange(20, 30, composite_result.Get());
  result->CopyRange(30, 33, composite_result.Get());

  EXPECT_EQ(result->NumCharacters(), composite_result->NumCharacters());
  EXPECT_EQ(result->SnappedWidth(), composite_result->SnappedWidth());
  EXPECT_EQ(result->SnappedStartPositionForOffset(0),
            composite_result->SnappedStartPositionForOffset(0));
  EXPECT_EQ(result->SnappedStartPositionForOffset(15),
            composite_result->SnappedStartPositionForOffset(15));
  EXPECT_EQ(result->SnappedStartPositionForOffset(30),
            composite_result->SnappedStartPositionForOffset(30));
  EXPECT_EQ(result->SnappedStartPositionForOffset(33),
            composite_result->SnappedStartPositionForOffset(33));
}

TEST_F(HarfBuzzShaperTest, ShapeResultCopyRangeIntoArabicThaiHanLatin) {
  UChar mixed_string[] = {0x628, 0x20, 0x64A, 0x629, 0x20, 0xE20, 0x65E5, 0x62};
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(mixed_string, 8);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  RefPtr<ShapeResult> composite_result =
      ShapeResult::Create(&font, 0, direction);
  result->CopyRange(0, 4, composite_result.Get());
  result->CopyRange(4, 6, composite_result.Get());
  result->CopyRange(6, 8, composite_result.Get());

  EXPECT_EQ(result->NumCharacters(), composite_result->NumCharacters());
  EXPECT_EQ(result->SnappedWidth(), composite_result->SnappedWidth());
  EXPECT_EQ(result->SnappedStartPositionForOffset(0),
            composite_result->SnappedStartPositionForOffset(0));
  EXPECT_EQ(result->SnappedStartPositionForOffset(1),
            composite_result->SnappedStartPositionForOffset(1));
  EXPECT_EQ(result->SnappedStartPositionForOffset(2),
            composite_result->SnappedStartPositionForOffset(2));
  EXPECT_EQ(result->SnappedStartPositionForOffset(3),
            composite_result->SnappedStartPositionForOffset(3));
  EXPECT_EQ(result->SnappedStartPositionForOffset(4),
            composite_result->SnappedStartPositionForOffset(4));
  EXPECT_EQ(result->SnappedStartPositionForOffset(5),
            composite_result->SnappedStartPositionForOffset(5));
  EXPECT_EQ(result->SnappedStartPositionForOffset(6),
            composite_result->SnappedStartPositionForOffset(6));
  EXPECT_EQ(result->SnappedStartPositionForOffset(7),
            composite_result->SnappedStartPositionForOffset(7));
  EXPECT_EQ(result->SnappedStartPositionForOffset(8),
            composite_result->SnappedStartPositionForOffset(8));
}

TEST_F(HarfBuzzShaperTest, ShapeResultCopyRangeAcrossRuns) {
  // Create 3 runs:
  // [0]: 1 character.
  // [1]: 5 characters.
  // [2]: 2 character.
  String mixed_string(u"\u65E5Hello\u65E5\u65E5");
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(mixed_string.Characters16(), mixed_string.length());
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  // CopyRange(5, 7) should copy 1 character from [1] and 1 from [2].
  RefPtr<ShapeResult> target = ShapeResult::Create(&font, 0, direction);
  result->CopyRange(5, 7, target.Get());
  EXPECT_EQ(2u, target->NumCharacters());
}

}  // namespace blink
