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
                       const Vector<int> expected_break_positions,
                       BreakSpaceType break_space = BreakSpaceType::kBefore) {
    if (test_string_.Is8Bit()) {
      test_string_ = String::Make16BitFrom8BitSource(test_string_.Characters8(),
                                                     test_string_.length());
    }
    LazyLineBreakIterator lazy_break_iterator(test_string_);
    lazy_break_iterator.SetBreakType(line_break_type);
    lazy_break_iterator.SetBreakSpace(break_space);
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
        << test_string_ << " " << break_iterator.BreakType() << " "
        << break_iterator.BreakSpace();
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
        << test_string_ << " " << break_iterator.BreakType() << " "
        << break_iterator.BreakSpace();
  }

 private:
  String test_string_;
};

static const LineBreakType all_break_types[] = {
    LineBreakType::kNormal, LineBreakType::kBreakAll,
    LineBreakType::kBreakCharacter, LineBreakType::kKeepAll};

class BreakTypeTest : public TextBreakIteratorTest,
                      public ::testing::WithParamInterface<LineBreakType> {};

INSTANTIATE_TEST_CASE_P(TextBreakIteratorTest,
                        BreakTypeTest,
                        ::testing::ValuesIn(all_break_types));

TEST_P(BreakTypeTest, EmptyString) {
  LazyLineBreakIterator iterator(g_empty_string);
  iterator.SetBreakType(GetParam());
  EXPECT_TRUE(iterator.IsBreakable(0));
}

TEST_P(BreakTypeTest, EmptyNullString) {
  LazyLineBreakIterator iterator(String{});
  iterator.SetBreakType(GetParam());
  EXPECT_TRUE(iterator.IsBreakable(0));
}

TEST_P(BreakTypeTest, EmptyDefaultConstructor) {
  LazyLineBreakIterator iterator;
  iterator.SetBreakType(GetParam());
  EXPECT_TRUE(iterator.IsBreakable(0));
}

TEST_F(TextBreakIteratorTest, Basic) {
  SetTestString("a b  c");
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6});
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6},
                  BreakSpaceType::kBeforeSpace);
  MatchLineBreaks(LineBreakType::kNormal, {2, 5, 6}, BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, Newline) {
  SetTestString("a\nb\n\nc");
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6});
  MatchLineBreaks(LineBreakType::kNormal, {2, 5, 6},
                  BreakSpaceType::kBeforeSpace);
  MatchLineBreaks(LineBreakType::kNormal, {2, 5, 6}, BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, Tab) {
  SetTestString("a\tb\t\tc");
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6});
  MatchLineBreaks(LineBreakType::kNormal, {2, 5, 6},
                  BreakSpaceType::kBeforeSpace);
  MatchLineBreaks(LineBreakType::kNormal, {2, 5, 6}, BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, LatinPunctuation) {
  SetTestString("(ab) cd.");
  MatchLineBreaks(LineBreakType::kNormal, {4, 8});
  MatchLineBreaks(LineBreakType::kBreakAll, {2, 4, 6, 8});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {1, 2, 3, 4, 5, 6, 7, 8});
  MatchLineBreaks(LineBreakType::kKeepAll, {4, 8});
  MatchLineBreaks(LineBreakType::kNormal, {5, 8}, BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {2, 5, 6, 8},
                  BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, Chinese) {
  SetTestString("標準萬國碼");
  MatchLineBreaks(LineBreakType::kNormal, {1, 2, 3, 4, 5});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 4, 5});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {1, 2, 3, 4, 5});
  MatchLineBreaks(LineBreakType::kKeepAll, {5});
  MatchLineBreaks(LineBreakType::kNormal, {1, 2, 3, 4, 5},
                  BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 4, 5},
                  BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, ChineseMixed) {
  SetTestString("標（準）萬ab國.碼");
  MatchLineBreaks(LineBreakType::kNormal, {1, 4, 5, 7, 9, 10});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 4, 5, 6, 7, 9, 10});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  MatchLineBreaks(LineBreakType::kKeepAll, {1, 4, 9, 10});
  MatchLineBreaks(LineBreakType::kNormal, {1, 4, 5, 7, 9, 10},
                  BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 4, 5, 6, 7, 9, 10},
                  BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, ChineseSpaces) {
  SetTestString("標  萬  a  國");
  MatchLineBreaks(LineBreakType::kNormal, {1, 2, 4, 5, 7, 8, 10});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 4, 5, 7, 8, 10});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  MatchLineBreaks(LineBreakType::kKeepAll, {1, 2, 4, 5, 7, 8, 10});
  MatchLineBreaks(LineBreakType::kNormal, {1, 2, 4, 5, 7, 8, 10},
                  BreakSpaceType::kBeforeSpace);
  MatchLineBreaks(LineBreakType::kNormal, {3, 6, 9, 10},
                  BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {3, 6, 9, 10},
                  BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJFamilyIsolate) {
  SetTestString(u8"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  MatchLineBreaks(LineBreakType::kNormal, {11});
  MatchLineBreaks(LineBreakType::kBreakAll, {11});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {11});
  MatchLineBreaks(LineBreakType::kKeepAll, {11});
  MatchLineBreaks(LineBreakType::kNormal, {11}, BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {11}, BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequenceIsolate) {
  SetTestString(u8"\u261D\U0001F3FB");
  MatchLineBreaks(LineBreakType::kNormal, {3});
  MatchLineBreaks(LineBreakType::kBreakAll, {3});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {3});
  MatchLineBreaks(LineBreakType::kKeepAll, {3});
  MatchLineBreaks(LineBreakType::kNormal, {3}, BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {3}, BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJSequence) {
  SetTestString(
      u8"abc \U0001F469\u200D\U0001F469\u200D\U0001F467\u200D\U0001F467 def");
  MatchLineBreaks(LineBreakType::kNormal, {3, 15, 19});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 15, 17, 18, 19});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 15, 16, 17, 18, 19});
  MatchLineBreaks(LineBreakType::kKeepAll, {3, 15, 19});
  MatchLineBreaks(LineBreakType::kNormal, {4, 16, 19}, BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 4, 16, 17, 18, 19},
                  BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequence) {
  SetTestString(u8"abc \u261D\U0001F3FB def");
  MatchLineBreaks(LineBreakType::kNormal, {3, 7, 11});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 7, 9, 10, 11});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 7, 8, 9, 10, 11});
  MatchLineBreaks(LineBreakType::kKeepAll, {3, 7, 11});
  MatchLineBreaks(LineBreakType::kNormal, {4, 8, 11}, BreakSpaceType::kAfter);
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 4, 8, 9, 10, 11},
                  BreakSpaceType::kAfter);
}

TEST_F(TextBreakIteratorTest, NextBreakOpportunityAtEnd) {
  LineBreakType break_types[] = {
      LineBreakType::kNormal, LineBreakType::kBreakAll,
      LineBreakType::kBreakCharacter, LineBreakType::kKeepAll};
  for (const auto break_type : break_types) {
    LazyLineBreakIterator break_iterator(String("1"));
    break_iterator.SetBreakType(break_type);
    EXPECT_EQ(1u, break_iterator.NextBreakOpportunity(1));
  }
}

}  // namespace blink
