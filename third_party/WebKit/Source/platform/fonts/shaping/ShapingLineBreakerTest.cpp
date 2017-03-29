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
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {

class ShapingLineBreakerTest : public ::testing::Test {
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

static inline String to16Bit(const char* text, unsigned length) {
  return String::make16BitFrom8BitSource(reinterpret_cast<const LChar*>(text),
                                         length);
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatin) {
  String string = to16Bit(
      "Test run with multiple words and breaking "
      "opportunities.",
      56);
  const AtomicString locale = "en-US";
  TextDirection direction = TextDirection::kLtr;
  LineBreakType breakType = LineBreakType::Normal;

  HarfBuzzShaper shaper(string.characters16(), 56);
  RefPtr<ShapeResult> result = shaper.shape(&font, direction);

  // "Test run with multiple"
  RefPtr<ShapeResult> first4 = shaper.shape(&font, direction, 0, 22);
  ASSERT_LT(first4->snappedWidth(), result->snappedWidth());

  // "Test run with"
  RefPtr<ShapeResult> first3 = shaper.shape(&font, direction, 0, 13);
  ASSERT_LT(first3->snappedWidth(), first4->snappedWidth());

  // "Test run"
  RefPtr<ShapeResult> first2 = shaper.shape(&font, direction, 0, 8);
  ASSERT_LT(first2->snappedWidth(), first3->snappedWidth());

  // "Test"
  RefPtr<ShapeResult> first1 = shaper.shape(&font, direction, 0, 4);
  ASSERT_LT(first1->snappedWidth(), first2->snappedWidth());

  ShapingLineBreaker breaker(&shaper, &font, result.get(), locale, breakType);
  RefPtr<ShapeResult> line;
  unsigned breakOffset = 0;

  // Test the case where the entire string fits.
  line = breaker.shapeLine(0, result->snappedWidth(), &breakOffset);
  EXPECT_EQ(56u, breakOffset);  // After the end of the string.
  EXPECT_EQ(result->snappedWidth(), line->snappedWidth());

  // Test cases where we break between words.
  line = breaker.shapeLine(0, first4->snappedWidth(), &breakOffset);
  EXPECT_EQ(22u, breakOffset);  // Between "multiple" and " words"
  EXPECT_EQ(first4->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first4->snappedWidth() + 10, &breakOffset);
  EXPECT_EQ(22u, breakOffset);  // Between "multiple" and " words"
  EXPECT_EQ(first4->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first4->snappedWidth() - 1, &breakOffset);
  EXPECT_EQ(13u, breakOffset);  // Between "width" and "multiple"
  EXPECT_EQ(first3->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first3->snappedWidth(), &breakOffset);
  EXPECT_EQ(13u, breakOffset);  // Between "width" and "multiple"
  EXPECT_EQ(first3->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first3->snappedWidth() - 1, &breakOffset);
  EXPECT_EQ(8u, breakOffset);  // Between "run" and "width"
  EXPECT_EQ(first2->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first2->snappedWidth(), &breakOffset);
  EXPECT_EQ(8u, breakOffset);  // Between "run" and "width"
  EXPECT_EQ(first2->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first2->snappedWidth() - 1, &breakOffset);
  EXPECT_EQ(4u, breakOffset);  // Between "Test" and "run"
  EXPECT_EQ(first1->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(0, first1->snappedWidth(), &breakOffset);
  EXPECT_EQ(4u, breakOffset);  // Between "Test" and "run"
  EXPECT_EQ(first1->snappedWidth(), line->snappedWidth());

  // Test the case where we cannot break earlier.
  line = breaker.shapeLine(0, first1->snappedWidth() - 1, &breakOffset);
  EXPECT_EQ(4u, breakOffset);  // Between "Test" and "run"
  EXPECT_EQ(first1->snappedWidth(), line->snappedWidth());
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatinMultiLine) {
  String string = to16Bit("Line breaking test case.", 24);
  const AtomicString locale = "en-US";
  TextDirection direction = TextDirection::kLtr;
  LineBreakType breakType = LineBreakType::Normal;

  HarfBuzzShaper shaper(string.characters16(), 24);
  RefPtr<ShapeResult> result = shaper.shape(&font, direction);
  RefPtr<ShapeResult> first = shaper.shape(&font, direction, 0, 4);
  RefPtr<ShapeResult> midThird = shaper.shape(&font, direction, 0, 16);

  ShapingLineBreaker breaker(&shaper, &font, result.get(), locale, breakType);
  unsigned breakOffset = 0;

  breaker.shapeLine(0, result->snappedWidth() - 1, &breakOffset);
  EXPECT_EQ(18u, breakOffset);

  breaker.shapeLine(0, first->snappedWidth(), &breakOffset);
  EXPECT_EQ(4u, breakOffset);

  breaker.shapeLine(0, midThird->snappedWidth(), &breakOffset);
  EXPECT_EQ(13u, breakOffset);

  breaker.shapeLine(13u, midThird->snappedWidth(), &breakOffset);
  EXPECT_EQ(24u, breakOffset);
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatinBreakAll) {
  String string = to16Bit("Testing break type-break all.", 29);
  const AtomicString locale = "en-US";
  TextDirection direction = TextDirection::kLtr;
  LineBreakType breakType = LineBreakType::BreakAll;

  HarfBuzzShaper shaper(string.characters16(), 29);
  RefPtr<ShapeResult> result = shaper.shape(&font, direction);
  RefPtr<ShapeResult> midpoint = shaper.shape(&font, direction, 0, 16);

  ShapingLineBreaker breaker(&shaper, &font, result.get(), locale, breakType);
  RefPtr<ShapeResult> line;
  unsigned breakOffset = 0;

  line = breaker.shapeLine(0, midpoint->snappedWidth(), &breakOffset);
  EXPECT_EQ(16u, breakOffset);
  EXPECT_EQ(midpoint->snappedWidth(), line->snappedWidth());

  line = breaker.shapeLine(16u, result->snappedWidth(), &breakOffset);
  EXPECT_EQ(29u, breakOffset);
  EXPECT_GE(midpoint->snappedWidth(), line->snappedWidth());
}

TEST_F(ShapingLineBreakerTest, ShapeLineArabicThaiHanLatinBreakAll) {
  UChar mixedString[] = {0x628, 0x20, 0x64A, 0x629, 0x20, 0xE20, 0x65E5, 0x62};
  const AtomicString locale = "ar_AE";
  TextDirection direction = TextDirection::kRtl;
  LineBreakType breakType = LineBreakType::BreakAll;

  HarfBuzzShaper shaper(mixedString, 8);
  RefPtr<ShapeResult> result = shaper.shape(&font, direction);

  ShapingLineBreaker breaker(&shaper, &font, result.get(), locale, breakType);
  unsigned breakOffset = 0;
  breaker.shapeLine(3, result->snappedWidth() / LayoutUnit(2), &breakOffset);
}

}  // namespace blink
