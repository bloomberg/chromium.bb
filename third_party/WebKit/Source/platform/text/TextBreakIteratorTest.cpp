// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/TextBreakIterator.h"

#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TextBreakIteratorTest : public testing::Test {
 protected:
  void SetTestString(const char* test_string) {
    test_string_ = String::FromUTF8(test_string);
  }

  // The expected break positions must be specified UTF-16 character boundaries.
  void MatchLineBreaks(LineBreakType line_break_type,
                       const Vector<int> expected_break_positions) {
    if (test_string_.Is8Bit()) {
      test_string_ = String::Make16BitFrom8BitSource(test_string_.Characters8(),
                                                     test_string_.length());
    }
    LazyLineBreakIterator lazy_break_iterator(test_string_);
    int next_breakable = 0;
    for (auto break_position : expected_break_positions) {
      int trigger_pos = std::min(static_cast<unsigned>(next_breakable + 1),
                                 test_string_.length());
      bool is_breakable = lazy_break_iterator.IsBreakable(
          trigger_pos, next_breakable, line_break_type);
      if (is_breakable) {
        ASSERT_EQ(trigger_pos, break_position);
      }
      ASSERT_EQ(break_position, next_breakable);
    }
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

#define MATCH_LINE_BREAKS(LINEBREAKTYPE, ...) \
  {                                           \
    DECLARE_BREAKSVECTOR(__VA_ARGS__);        \
    MatchLineBreaks(LINEBREAKTYPE, breaks);   \
  }

TEST_F(TextBreakIteratorTest, Basic) {
  SetTestString("a b c");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {1, 3, 5});
}

TEST_F(TextBreakIteratorTest, Chinese) {
  SetTestString("標準萬國碼");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {5});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJFamilyIsolate) {
  SetTestString(u8"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {11});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {11});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {11});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequenceIsolate) {
  SetTestString(u8"\u261D\U0001F3FB");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {3});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {3});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {3});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJSequence) {
  SetTestString(
      u8"abc \U0001F469\u200D\U0001F469\u200D\U0001F467\u200D\U0001F467 def");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {3, 15, 19});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 3, 15, 17, 18, 19});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {3, 15, 19});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequence) {
  SetTestString(u8"abc \u261D\U0001F3FB def");
  MATCH_LINE_BREAKS(LineBreakType::kNormal, {3, 7, 11});
  MATCH_LINE_BREAKS(LineBreakType::kBreakAll, {1, 2, 3, 7, 9, 10, 11});
  MATCH_LINE_BREAKS(LineBreakType::kKeepAll, {3, 7, 11});
}

}  // namespace blink
