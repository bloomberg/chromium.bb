// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <string>

namespace blink {

struct FallbackTestRun {
  std::string text;
  FontFallbackPriority font_fallback_priority;
};

struct FallbackExpectedRun {
  unsigned limit;
  FontFallbackPriority font_fallback_priority;

  FallbackExpectedRun(unsigned the_limit,
                      FontFallbackPriority the_font_fallback_priority)
      : limit(the_limit), font_fallback_priority(the_font_fallback_priority) {}
};

class SymbolsIteratorTest : public testing::Test {
 protected:
  void CheckRuns(const Vector<FallbackTestRun>& runs) {
    String text(g_empty_string16_bit);
    Vector<FallbackExpectedRun> expect;
    for (auto& run : runs) {
      text.append(String::FromUTF8(run.text.c_str()));
      expect.push_back(
          FallbackExpectedRun(text.length(), run.font_fallback_priority));
    }
    SymbolsIterator symbols_iterator(text.Characters16(), text.length());
    VerifyRuns(&symbols_iterator, expect);
  }

  void VerifyRuns(SymbolsIterator* symbols_iterator,
                  const Vector<FallbackExpectedRun>& expect) {
    unsigned limit;
    FontFallbackPriority font_fallback_priority;
    unsigned long run_count = 0;
    while (symbols_iterator->Consume(&limit, &font_fallback_priority)) {
      ASSERT_LT(run_count, expect.size());
      ASSERT_EQ(expect[run_count].limit, limit);
      ASSERT_EQ(expect[run_count].font_fallback_priority,
                font_fallback_priority);
      ++run_count;
    }
    ASSERT_EQ(expect.size(), run_count);
  }
};

TEST_F(SymbolsIteratorTest, Empty) {
  String empty(g_empty_string16_bit);
  SymbolsIterator symbols_iterator(empty.Characters16(), empty.length());
  unsigned limit = 0;
  FontFallbackPriority symbols_font = FontFallbackPriority::kInvalid;
  DCHECK(!symbols_iterator.Consume(&limit, &symbols_font));
  ASSERT_EQ(limit, 0u);
  ASSERT_EQ(symbols_font, FontFallbackPriority::kInvalid);
}

TEST_F(SymbolsIteratorTest, Space) {
  CheckRuns({{" ", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, Latin) {
  CheckRuns({{"Aa", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, LatinColorEmojiTextEmoji) {
  CheckRuns({{"a", FontFallbackPriority::kText},
             {"⌚", FontFallbackPriority::kEmojiEmoji},
             {"☎", FontFallbackPriority::kEmojiText}});
}

TEST_F(SymbolsIteratorTest, IgnoreVSInMath) {
  CheckRuns({{u8"⊆⊇⊈\U0000FE0E⊙⊚⊚", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, IgnoreVS15InText) {
  CheckRuns({{u8"abcdef\U0000FE0Eghji", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, IgnoreVS16InText) {
  CheckRuns({{u8"abcdef\U0000FE0Fghji", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, AllHexValuesText) {
  // Helps with detecting incorrect emoji pattern definitions which are
  // missing a \U000... prefix for example.
  CheckRuns({{"abcdef0123456789ABCDEF", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, NumbersAndHashNormalAndEmoji) {
  CheckRuns({{"0123456789#*", FontFallbackPriority::kText},
             {"0⃣1⃣2⃣3⃣4⃣5⃣6⃣7⃣8⃣9⃣*⃣", FontFallbackPriority::kEmojiEmoji},
             {"0123456789#*", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, VS16onDigits) {
  CheckRuns({{"#", FontFallbackPriority::kText},
             {"#\uFE0F#\uFE0F\u20E3", FontFallbackPriority::kEmojiEmoji},
             {"#", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, SingleFlag) {
  CheckRuns({{"🇺", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, CombiningCircle) {
  CheckRuns({{"◌́◌̀◌̈◌̂◌̄◌̊", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, CombiningEnclosingCircleBackslash) {
  CheckRuns({{"A⃠B⃠C⃠", FontFallbackPriority::kText},
             {"🚷🚯🚱🔞📵🚭🚫", FontFallbackPriority::kEmojiEmoji},
             {"🎙⃠", FontFallbackPriority::kEmojiText},
             {"📸⃠🔫⃠", FontFallbackPriority::kEmojiEmoji},
             {"a⃠b⃠c⃠", FontFallbackPriority::kText}});
}

// TODO: Perhaps check for invalid country indicator combinations?

TEST_F(SymbolsIteratorTest, FlagsVsNonFlags) {
  CheckRuns({{"🇺🇸🇸", FontFallbackPriority::kEmojiEmoji},  // "US"
             {"abc", FontFallbackPriority::kText},
             {"🇺🇸", FontFallbackPriority::kEmojiEmoji},
             {"a🇿", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, EmojiVS15) {
  // A VS15 after the anchor must trigger text display.
  CheckRuns({{"⚓\U0000FE0E", FontFallbackPriority::kEmojiText},
             {"⛵", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiZWSSequences) {
  CheckRuns({{"👩‍👩‍👧‍👦👩‍❤️‍💋‍👨",
              FontFallbackPriority::kEmojiEmoji},
             {"abcd", FontFallbackPriority::kText},
             {"👩‍👩‍", FontFallbackPriority::kEmojiEmoji},
             {"efgh", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, AllEmojiZWSSequences) {
  // clang-format gets confused by Emojis, http://llvm.org/PR30530
  // clang-format off
  CheckRuns(
      {{"💏👩‍❤️‍💋‍👨👨‍❤️‍💋‍👨👩‍❤️‍💋‍👩💑👩‍❤️‍👨👨‍❤"
        "️"
        "‍👨👩‍❤️"
        "‍👩👪👨‍👩‍👦👨‍👩‍👧👨‍👩‍👧‍👦👨‍👩‍👦‍👦👨‍👩‍👧‍👧👨‍👨"
        "‍"
        "👦👨‍👨‍👧👨‍👨‍👧‍👦👨‍👨‍👦‍👦👨‍👨‍👧"
        "‍"
        "👧"
        "👩‍👩‍👦👩‍👩‍👧👩‍👩‍👧‍👦👩‍👩‍👦‍👦👩‍👩‍👧‍👧👁"
        "‍"
        "🗨",
        FontFallbackPriority::kEmojiEmoji}});
  // clang-format on
}

TEST_F(SymbolsIteratorTest, ModifierPlusGender) {
  CheckRuns({{"⛹🏻‍♂", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, TextMemberZwjSequence) {
  CheckRuns({{"👨‍⚕", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, FacepalmCartwheelShrugModifierFemale) {
  CheckRuns({{"🤦‍♀🤸‍♀🤷‍♀🤷🏾‍♀",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, AesculapiusMaleFemalEmoji) {
  // Emoji Data 4 has upgraded those three characters to Emoji.
  CheckRuns({{"a", FontFallbackPriority::kText},
             {"⚕♀♂", FontFallbackPriority::kEmojiText}});
}

TEST_F(SymbolsIteratorTest, EyeSpeechBubble) {
  CheckRuns({{"👁‍🗨", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, Modifier) {
  CheckRuns({{"👶🏿", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, DingbatsMiscSymbolsModifier) {
  CheckRuns({{"⛹🏻✍🏻✊🏼", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, ExtraZWJPrefix) {
  CheckRuns({{u8"\U0000200D", FontFallbackPriority::kText},
             {u8"\U0001F469\U0000200D\U00002764\U0000FE0F\U0000200D\U0001F48B"
              u8"\U0000200D\U0001F468",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, Arrows) {
  CheckRuns({{"x→←x←↑↓→", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, JudgePilot) {
  CheckRuns({{"👨‍⚖️👩‍⚖️👨🏼‍⚖️👩🏼‍⚖️",
              FontFallbackPriority::kEmojiEmoji}});
}

// Extracted from http://unicode.org/emoji/charts/emoji-released.html for Emoji
// v5.0, except for the subdivision-flag section.
// Before ICU 59 new emoji sequences and new single emoji are not detected as
// emoji type text and sequences get split up in the middle so that shaping
// cannot form the right glyph from the emoji font. Running this as one run in
// one test ensures that the new emoji form an unbroken emoji-type sequence.
TEST_F(SymbolsIteratorTest, Emoji5AdditionsExceptFlags) {
  CheckRuns(
      {{u8"\U0001F9D4\U0001F3FB\U0001F9D4\U0001F3FC\U0001F9D4\U0001F3FD"
        u8"\U0001F9D4\U0001F3FE\U0001F9D4\U0001F3FF\U0001F931\U0001F931"
        u8"\U0001F3FB\U0001F931\U0001F3FC\U0001F931\U0001F3FD\U0001F931"
        u8"\U0001F3FE\U0001F931\U0001F3FF\U0001F9D9\U0001F9D9\U0001F3FB"
        u8"\U0001F9D9\U0001F3FC\U0001F9D9\U0001F3FD\U0001F9D9\U0001F3FE"
        u8"\U0001F9D9\U0001F3FF\U0001F9D9\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D9\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9D9"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9D9\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D9\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D9\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D9\U0000200D\U00002642\U0000FE0F\U0001F9D9"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9D9\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9D9\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D9\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D9\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DA\U0001F9DA\U0001F3FB\U0001F9DA\U0001F3FC\U0001F9DA"
        u8"\U0001F3FD\U0001F9DA\U0001F3FE\U0001F9DA\U0001F3FF\U0001F9DA"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DA\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DA\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DA\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DA\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9DA"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9DA\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DA\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DA\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DA\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9DA"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9DA\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F9DB\U0001F3FB"
        u8"\U0001F9DB\U0001F3FC\U0001F9DB\U0001F3FD\U0001F9DB\U0001F3FE"
        u8"\U0001F9DB\U0001F3FF\U0001F9DB\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DB\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9DB"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9DB\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DB\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DB\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DB\U0000200D\U00002642\U0000FE0F\U0001F9DB"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DB\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DB\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DC\U0001F9DC\U0001F3FB\U0001F9DC\U0001F3FC\U0001F9DC"
        u8"\U0001F3FD\U0001F9DC\U0001F3FE\U0001F9DC\U0001F3FF\U0001F9DC"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DC\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DC\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DC\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DC\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9DC"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9DC\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DC\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DC\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DC\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9DC"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9DC\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F9DD\U0001F3FB"
        u8"\U0001F9DD\U0001F3FC\U0001F9DD\U0001F3FD\U0001F9DD\U0001F3FE"
        u8"\U0001F9DD\U0001F3FF\U0001F9DD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DD\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9DD"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9DD\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DD\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DD\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DD\U0000200D\U00002642\U0000FE0F\U0001F9DD"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DD\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DD\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DE\U0001F9DE\U0000200D\U00002640\U0000FE0F\U0001F9DE"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DF\U0001F9DF\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D6\U0001F9D6\U0001F3FB\U0001F9D6\U0001F3FC\U0001F9D6"
        u8"\U0001F3FD\U0001F9D6\U0001F3FE\U0001F9D6\U0001F3FF\U0001F9D6"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D6\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D6\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D6\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D6\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9D6"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9D6\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D6\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D6\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D6\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9D6"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9D6\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F9D7\U0001F3FB"
        u8"\U0001F9D7\U0001F3FC\U0001F9D7\U0001F3FD\U0001F9D7\U0001F3FE"
        u8"\U0001F9D7\U0001F3FF\U0001F9D7\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D7\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9D7"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9D7\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D7\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D7\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D7\U0000200D\U00002642\U0000FE0F\U0001F9D7"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D7\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D7\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D8\U0001F9D8\U0001F3FB\U0001F9D8\U0001F3FC\U0001F9D8"
        u8"\U0001F3FD\U0001F9D8\U0001F3FE\U0001F9D8\U0001F3FF\U0001F9D8"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D8\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D8\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D8\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D8\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9D8"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9D8\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D8\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D8\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D8\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9D8"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9D8\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F91F\U0001F91F\U0001F3FB"
        u8"\U0001F91F\U0001F3FC\U0001F91F\U0001F3FD\U0001F91F\U0001F3FE"
        u8"\U0001F91F\U0001F3FF\U0001F932\U0001F932\U0001F3FB\U0001F932"
        u8"\U0001F3FC\U0001F932\U0001F3FD\U0001F932\U0001F3FE\U0001F932"
        u8"\U0001F3FF\U0001F9E0\U0001F9E1\U0001F9E3\U0001F9E4\U0001F9E5"
        u8"\U0001F9E6\U0001F9E2\U0001F993\U0001F992\U0001F994\U0001F995"
        u8"\U0001F996\U0001F997\U0001F965\U0001F966\U0001F968\U0001F969"
        u8"\U0001F96A\U0001F963\U0001F96B\U0001F95F\U0001F960\U0001F961"
        u8"\U0001F967\U0001F964\U0001F962\U0001F6F8\U0001F6F7\U0001F94C",
        FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiSubdivisionFlags) {
  CheckRuns(
      {{"🏴󠁧󠁢󠁷󠁬󠁳󠁿🏴󠁧󠁢󠁳󠁣󠁴󠁿🏴󠁧󠁢",
        FontFallbackPriority::kEmojiEmoji}});
}

}  // namespace blink
