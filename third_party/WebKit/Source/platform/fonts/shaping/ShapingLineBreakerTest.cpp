// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapingLineBreaker.h"

#include <unicode/uscript.h>
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextRun.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ShapingLineBreakerTest : public ::testing::Test {
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
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

static inline String To16Bit(const char* text, unsigned length) {
  return String::Make16BitFrom8BitSource(reinterpret_cast<const LChar*>(text),
                                         length);
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatin) {
  String string = To16Bit(
      "Test run with multiple words and breaking "
      "opportunities.",
      56);
  LazyLineBreakIterator break_iterator(string, "en-US", LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 56);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  // "Test run with multiple"
  RefPtr<ShapeResult> first4 = shaper.Shape(&font, direction, 0, 22);
  ASSERT_LT(first4->SnappedWidth(), result->SnappedWidth());

  // "Test run with"
  RefPtr<ShapeResult> first3 = shaper.Shape(&font, direction, 0, 13);
  ASSERT_LT(first3->SnappedWidth(), first4->SnappedWidth());

  // "Test run"
  RefPtr<ShapeResult> first2 = shaper.Shape(&font, direction, 0, 8);
  ASSERT_LT(first2->SnappedWidth(), first3->SnappedWidth());

  // "Test"
  RefPtr<ShapeResult> first1 = shaper.Shape(&font, direction, 0, 4);
  ASSERT_LT(first1->SnappedWidth(), first2->SnappedWidth());

  ShapingLineBreaker breaker(&shaper, &font, result.Get(), &break_iterator);
  RefPtr<ShapeResult> line;
  unsigned break_offset = 0;

  // Test the case where the entire string fits.
  line = breaker.ShapeLine(0, result->SnappedWidth(), &break_offset);
  EXPECT_EQ(56u, break_offset);  // After the end of the string.
  EXPECT_EQ(result->SnappedWidth(), line->SnappedWidth());

  // Test cases where we break between words.
  line = breaker.ShapeLine(0, first4->SnappedWidth(), &break_offset);
  EXPECT_EQ(22u, break_offset);  // Between "multiple" and " words"
  EXPECT_EQ(first4->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first4->SnappedWidth() + 10, &break_offset);
  EXPECT_EQ(22u, break_offset);  // Between "multiple" and " words"
  EXPECT_EQ(first4->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first4->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(13u, break_offset);  // Between "width" and "multiple"
  EXPECT_EQ(first3->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first3->SnappedWidth(), &break_offset);
  EXPECT_EQ(13u, break_offset);  // Between "width" and "multiple"
  EXPECT_EQ(first3->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first3->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(8u, break_offset);  // Between "run" and "width"
  EXPECT_EQ(first2->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first2->SnappedWidth(), &break_offset);
  EXPECT_EQ(8u, break_offset);  // Between "run" and "width"
  EXPECT_EQ(first2->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first2->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(4u, break_offset);  // Between "Test" and "run"
  EXPECT_EQ(first1->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(0, first1->SnappedWidth(), &break_offset);
  EXPECT_EQ(4u, break_offset);  // Between "Test" and "run"
  EXPECT_EQ(first1->SnappedWidth(), line->SnappedWidth());

  // Test the case where we cannot break earlier.
  line = breaker.ShapeLine(0, first1->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(4u, break_offset);  // Between "Test" and "run"
  EXPECT_EQ(first1->SnappedWidth(), line->SnappedWidth());
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatinMultiLine) {
  String string = To16Bit("Line breaking test case.", 24);
  LazyLineBreakIterator break_iterator(string, "en-US", LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 24);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);
  RefPtr<ShapeResult> first = shaper.Shape(&font, direction, 0, 4);
  RefPtr<ShapeResult> mid_third = shaper.Shape(&font, direction, 0, 16);

  ShapingLineBreaker breaker(&shaper, &font, result.Get(), &break_iterator);
  unsigned break_offset = 0;

  breaker.ShapeLine(0, result->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(18u, break_offset);

  breaker.ShapeLine(0, first->SnappedWidth(), &break_offset);
  EXPECT_EQ(4u, break_offset);

  breaker.ShapeLine(0, mid_third->SnappedWidth(), &break_offset);
  EXPECT_EQ(13u, break_offset);

  breaker.ShapeLine(13u, mid_third->SnappedWidth(), &break_offset);
  EXPECT_EQ(24u, break_offset);
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatinBreakAll) {
  String string = To16Bit("Testing break type-break all.", 29);
  LazyLineBreakIterator break_iterator(string, "en-US",
                                       LineBreakType::kBreakAll);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), 29);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);
  RefPtr<ShapeResult> midpoint = shaper.Shape(&font, direction, 0, 16);

  ShapingLineBreaker breaker(&shaper, &font, result.Get(), &break_iterator);
  RefPtr<ShapeResult> line;
  unsigned break_offset = 0;

  line = breaker.ShapeLine(0, midpoint->SnappedWidth(), &break_offset);
  EXPECT_EQ(16u, break_offset);
  EXPECT_EQ(midpoint->SnappedWidth(), line->SnappedWidth());

  line = breaker.ShapeLine(16u, result->SnappedWidth(), &break_offset);
  EXPECT_EQ(29u, break_offset);
  EXPECT_GE(midpoint->SnappedWidth(), line->SnappedWidth());
}

TEST_F(ShapingLineBreakerTest, ShapeLineZeroAvailableWidth) {
  String string(u"Testing overflow line break.");
  LazyLineBreakIterator break_iterator(string, "en-US", LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), string.length());
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);

  ShapingLineBreaker breaker(&shaper, &font, result.Get(), &break_iterator);
  RefPtr<ShapeResult> line;
  unsigned break_offset = 0;
  LayoutUnit zero(0);

  line = breaker.ShapeLine(0, zero, &break_offset);
  EXPECT_EQ(7u, break_offset);

  line = breaker.ShapeLine(7, zero, &break_offset);
  EXPECT_EQ(16u, break_offset);

  line = breaker.ShapeLine(16, zero, &break_offset);
  EXPECT_EQ(21u, break_offset);

  line = breaker.ShapeLine(21, zero, &break_offset);
  EXPECT_EQ(28u, break_offset);
}

TEST_F(ShapingLineBreakerTest, ShapeLineArabicThaiHanLatin) {
  UChar mixed_string[] = {0x628, 0x20,   0x64A, 0x629, 0x20,
                          0xE20, 0x65E5, 0x62,  0};
  LazyLineBreakIterator break_iterator(mixed_string, "ar_AE",
                                       LineBreakType::kNormal);
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(mixed_string, 8);
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction);
  RefPtr<ShapeResult> words[] = {shaper.Shape(&font, direction, 0, 1),
                                 shaper.Shape(&font, direction, 2, 4),
                                 shaper.Shape(&font, direction, 5, 6),
                                 shaper.Shape(&font, direction, 6, 7),
                                 shaper.Shape(&font, direction, 7, 8)};
  const auto& longest_word = std::max_element(
      std::begin(words), std::end(words),
      [](const RefPtr<ShapeResult>& a, const RefPtr<ShapeResult>& b) {
        return a->SnappedWidth() < b->SnappedWidth();
      });
  LayoutUnit longest_word_width = (*longest_word)->SnappedWidth();

  ShapingLineBreaker breaker(&shaper, &font, result.Get(), &break_iterator);
  RefPtr<ShapeResult> line;
  unsigned break_offset = 0;

  breaker.ShapeLine(0, longest_word_width, &break_offset);
  EXPECT_EQ(1u, break_offset);

  breaker.ShapeLine(1, longest_word_width, &break_offset);
  EXPECT_EQ(4u, break_offset);

  breaker.ShapeLine(4, longest_word_width, &break_offset);
  EXPECT_EQ(6u, break_offset);

  breaker.ShapeLine(6, longest_word_width, &break_offset);
  EXPECT_EQ(7u, break_offset);

  breaker.ShapeLine(7, longest_word_width, &break_offset);
  EXPECT_EQ(8u, break_offset);
}

TEST_F(ShapingLineBreakerTest, ShapeLineRangeEndMidWord) {
  String string(u"Mid word");
  LazyLineBreakIterator break_iterator(string, "en-US", LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string.Characters16(), string.length());
  RefPtr<ShapeResult> result = shaper.Shape(&font, direction, 0, 2);

  ShapingLineBreaker breaker(&shaper, &font, result.Get(), &break_iterator);
  RefPtr<ShapeResult> line;
  unsigned break_offset = 0;

  line = breaker.ShapeLine(0, LayoutUnit::Max(), &break_offset);
  EXPECT_EQ(2u, break_offset);
  EXPECT_EQ(result->Width(), line->Width());
}
}  // namespace blink
