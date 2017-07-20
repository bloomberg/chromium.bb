// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/TextBreakIterator.h"

#include "platform/wtf/text/WTFString.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TextBreakIteratorTest : public ::testing::Test {
 protected:
  void SetTestString(const char* test_string) {
    test_string_ = String::FromUTF8(test_string);
  }

  // The expected break positions must be specified UTF-16 character boundaries.
  void MatchLineBreaks(LineBreakType line_break_type,
                       bool break_after_space,
                       const Vector<int> expected_break_positions) {
    if (test_string_.Is8Bit()) {
      test_string_ = String::Make16BitFrom8BitSource(test_string_.Characters8(),
                                                     test_string_.length());
    }
    LazyLineBreakIterator lazy_break_iterator(test_string_);
    lazy_break_iterator.SetBreakType(line_break_type);
    lazy_break_iterator.SetBreakAfterSpace(break_after_space);
    TestIsBreakable(expected_break_positions, lazy_break_iterator);
    TestNextBreakOpportunity(expected_break_positions, lazy_break_iterator);
  }

  // Test IsBreakable() by iterating all positions. BreakingContext uses this
  // interface.
  void TestIsBreakable(const Vector<int> expected_break_positions,
                       const LazyLineBreakIterator& break_iterator) {
    Vector<int> break_positions;
    int next_breakable = -1;
    for (unsigned i = 0; i <= test_string_.length(); i++) {
      if (break_iterator.IsBreakable(i, next_breakable))
        break_positions.push_back(i);
    }
    EXPECT_THAT(break_positions,
                ::testing::ElementsAreArray(expected_break_positions))
        << break_iterator.BreakType() << " for " << test_string_;
  }

  // Test NextBreakOpportunity() by iterating break opportunities.
  // ShapingLineBreaker uses this interface.
  void TestNextBreakOpportunity(const Vector<int> expected_break_positions,
                                const LazyLineBreakIterator& break_iterator) {
    Vector<int> break_positions;
    for (unsigned i = 0; i <= test_string_.length(); i++) {
      i = break_iterator.NextBreakOpportunity(i);
      break_positions.push_back(i);
    }
    EXPECT_THAT(break_positions,
                ::testing::ElementsAreArray(expected_break_positions))
        << break_iterator.BreakType() << " for " << test_string_;
  }

 private:
  String test_string_;
};

// Initializing Vector from an initializer list still not possible, C++ feature
// banned in Blink.
#define DECLARE_BREAKSVECTOR(...)                    \
  static const int32_t kBreaksArray[] = __VA_ARGS__; \
  Vector<int> breaks;                                \
  breaks.Append(kBreaksArray, sizeof(kBreaksArray) / sizeof(*kBreaksArray));

#define MATCH_LINE_BREAKS(LINEBREAKTYPE, ...)      \
  {                                                \
    DECLARE_BREAKSVECTOR(__VA_ARGS__);             \
    MatchLineBreaks(LINEBREAKTYPE, false, breaks); \
  }

#define MATCH_BREAK_AFTER_SPACE(LINEBREAKTYPE, ...) \
  {                                                 \
    DECLARE_BREAKSVECTOR(__VA_ARGS__);              \
    MatchLineBreaks(LINEBREAKTYPE, true, breaks);   \
  }

TEST_F(TextBreakIteratorTest, Basic) {
  SetTestString("a b  c");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {1, 3, 4, 6});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {2, 5, 6});
}

TEST_F(TextBreakIteratorTest, LatinPunctuation) {
  SetTestString("(ab) cd.");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {4, 8});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {2, 4, 6, 8});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter, {1, 2, 3, 4, 5, 6, 7, 8});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {4, 8});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {5, 8});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {2, 5, 6, 8});
}

TEST_F(TextBreakIteratorTest, Chinese) {
  SetTestString("標準萬國碼");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {5});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {1, 2, 3, 4, 5});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {1, 2, 3, 4, 5});
}

TEST_F(TextBreakIteratorTest, ChineseMixed) {
  SetTestString("標（準）萬ab國.碼");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {1, 4, 5, 7, 9, 10});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 4, 5, 6, 7, 9, 10});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter,
                    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {1, 4, 9, 10});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {1, 4, 5, 7, 9, 10});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {1, 4, 5, 6, 7, 9, 10});
}

TEST_F(TextBreakIteratorTest, ChineseSpaces) {
  SetTestString("標  萬  a  國");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {1, 2, 4, 5, 7, 8, 10});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 4, 5, 7, 8, 10});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter,
                    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {1, 2, 4, 5, 7, 8, 10});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {3, 6, 9, 10});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {3, 6, 9, 10});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJFamilyIsolate) {
  SetTestString(u8"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {11});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {11});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter, {11});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {11});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {11});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {11});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequenceIsolate) {
  SetTestString(u8"\u261D\U0001F3FB");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {3});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {3});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter, {3});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {3});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {3});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {3});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJSequence) {
  SetTestString(
      u8"abc \U0001F469\u200D\U0001F469\u200D\U0001F467\u200D\U0001F467 def");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {3, 15, 19});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 3, 15, 17, 18, 19});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter,
                    {1, 2, 3, 4, 15, 16, 17, 18, 19});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {3, 15, 19});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {4, 16, 19});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {1, 2, 4, 16, 17, 18, 19});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequence) {
  SetTestString(u8"abc \u261D\U0001F3FB def");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {3, 7, 11});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 3, 7, 9, 10, 11});
  MATCH_LINE_BREAKS(LineBreakType::kBreakCharacter,
                    {1, 2, 3, 4, 7, 8, 9, 10, 11});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {3, 7, 11});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kNormal, {4, 8, 11});
  MATCH_BREAK_AFTER_SPACE(LineBreakType::kBreakAll, {1, 2, 4, 8, 9, 10, 11});
}

}  // namespace blink
