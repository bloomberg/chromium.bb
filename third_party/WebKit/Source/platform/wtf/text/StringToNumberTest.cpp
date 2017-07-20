// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/StringToNumber.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <cstring>

namespace WTF {

TEST(StringToNumberTest, TestCharactersToIntStrict) {
#define EXPECT_VALID(string, expectedValue)                                   \
  do {                                                                        \
    bool ok;                                                                  \
    int value = CharactersToIntStrict(reinterpret_cast<const LChar*>(string), \
                                      std::strlen(string), &ok);              \
    EXPECT_TRUE(ok);                                                          \
    EXPECT_EQ(value, expectedValue);                                          \
  } while (false)

#define EXPECT_INVALID(string)                                    \
  do {                                                            \
    bool ok;                                                      \
    CharactersToIntStrict(reinterpret_cast<const LChar*>(string), \
                          std::strlen(string), &ok);              \
    EXPECT_FALSE(ok);                                             \
  } while (false)

  EXPECT_VALID("1", 1);
  EXPECT_VALID("2", 2);
  EXPECT_VALID("9", 9);
  EXPECT_VALID("10", 10);
  EXPECT_VALID("0", 0);
  EXPECT_VALID("-0", 0);
  EXPECT_VALID("-1", -1);
  EXPECT_VALID("-2", -2);
  EXPECT_VALID("-9", -9);
  EXPECT_VALID("-10", -10);
  EXPECT_VALID("+0", 0);
  EXPECT_VALID("+1", 1);
  EXPECT_VALID("+2", 2);
  EXPECT_VALID("+9", 9);
  EXPECT_VALID("+10", 10);
  EXPECT_VALID("00", 0);
  EXPECT_VALID("+00", 0);
  EXPECT_VALID("-00", 0);
  EXPECT_VALID("01", 1);
  EXPECT_VALID("-01", -1);
  EXPECT_VALID("00000000000000000000", 0);
  EXPECT_INVALID("a");
  EXPECT_INVALID("1a");
  EXPECT_INVALID("a1");
  EXPECT_INVALID("-a");
  EXPECT_INVALID("");
  EXPECT_INVALID("-");
  EXPECT_INVALID("--1");
  EXPECT_INVALID("++1");
  EXPECT_INVALID("+-1");
  EXPECT_INVALID("-+1");
  EXPECT_INVALID("0-");
  EXPECT_INVALID("0+");

  EXPECT_VALID("2147483647", 2147483647);
  EXPECT_VALID("02147483647", 2147483647);
  EXPECT_INVALID("2147483648");
  EXPECT_INVALID("2147483649");
  EXPECT_INVALID("2147483650");
  EXPECT_INVALID("2147483700");
  EXPECT_INVALID("2147484000");
  EXPECT_INVALID("2200000000");
  EXPECT_INVALID("3000000000");
  EXPECT_INVALID("10000000000");
  EXPECT_VALID("-2147483647", -2147483647);
  EXPECT_VALID("-2147483648", -2147483647 - 1);
  EXPECT_INVALID("-2147483649");
  EXPECT_INVALID("-2147483650");
  EXPECT_INVALID("-2147483700");
  EXPECT_INVALID("-2147484000");
  EXPECT_INVALID("-2200000000");
  EXPECT_INVALID("-3000000000");
  EXPECT_INVALID("-10000000000");

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

TEST(StringToNumberTest, CharactersToUIntStrict) {
#define EXPECT_VALID(string, expectedValue)                                \
  do {                                                                     \
    bool ok;                                                               \
    unsigned value = CharactersToUIntStrict(                               \
        reinterpret_cast<const LChar*>(string), std::strlen(string), &ok); \
    EXPECT_TRUE(ok);                                                       \
    EXPECT_EQ(value, expectedValue);                                       \
  } while (false)

#define EXPECT_INVALID(string)                                     \
  do {                                                             \
    bool ok;                                                       \
    CharactersToUIntStrict(reinterpret_cast<const LChar*>(string), \
                           std::strlen(string), &ok);              \
    EXPECT_FALSE(ok);                                              \
  } while (false)

  EXPECT_VALID("1", 1u);
  EXPECT_VALID("2", 2u);
  EXPECT_VALID("9", 9u);
  EXPECT_VALID("10", 10u);
  EXPECT_VALID("0", 0u);
  EXPECT_VALID("+0", 0u);
  EXPECT_VALID("+1", 1u);
  EXPECT_VALID("+2", 2u);
  EXPECT_VALID("+9", 9u);
  EXPECT_VALID("+10", 10u);
  EXPECT_VALID("00", 0u);
  EXPECT_VALID("+00", 0u);
  EXPECT_VALID("01", 1u);
  EXPECT_VALID("00000000000000000000", 0u);
  EXPECT_INVALID("a");
  EXPECT_INVALID("1a");
  EXPECT_INVALID("a1");
  EXPECT_INVALID("-a");
  EXPECT_INVALID("");
  EXPECT_INVALID("-");
  EXPECT_INVALID("-0");
  EXPECT_INVALID("-1");
  EXPECT_INVALID("-2");
  EXPECT_INVALID("-9");
  EXPECT_INVALID("-10");
  EXPECT_INVALID("-00");
  EXPECT_INVALID("-01");
  EXPECT_INVALID("--1");
  EXPECT_INVALID("++1");
  EXPECT_INVALID("+-1");
  EXPECT_INVALID("-+1");
  EXPECT_INVALID("0-");
  EXPECT_INVALID("0+");

  EXPECT_VALID("2147483647", 2147483647u);
  EXPECT_VALID("02147483647", 2147483647u);
  EXPECT_VALID("2147483648", 2147483648u);
  EXPECT_VALID("4294967295", 4294967295u);
  EXPECT_VALID("0004294967295", 4294967295u);
  EXPECT_INVALID("4294967296");
  EXPECT_INVALID("4294967300");
  EXPECT_INVALID("4300000000");
  EXPECT_INVALID("5000000000");
  EXPECT_INVALID("10000000000");
  EXPECT_INVALID("-2147483647");
  EXPECT_INVALID("-2147483648");
  EXPECT_INVALID("-2147483649");
  EXPECT_INVALID("-2147483650");
  EXPECT_INVALID("-2147483700");
  EXPECT_INVALID("-2147484000");
  EXPECT_INVALID("-2200000000");
  EXPECT_INVALID("-3000000000");
  EXPECT_INVALID("-10000000000");

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

TEST(StringToNumberTest, HexCharactersToUIntStrict) {
#define EXPECT_VALID(string, expectedValue)                                \
  do {                                                                     \
    bool ok;                                                               \
    unsigned value = HexCharactersToUIntStrict(                            \
        reinterpret_cast<const LChar*>(string), std::strlen(string), &ok); \
    EXPECT_TRUE(ok);                                                       \
    EXPECT_EQ(value, expectedValue);                                       \
  } while (false)

#define EXPECT_INVALID(string)                                        \
  do {                                                                \
    bool ok;                                                          \
    HexCharactersToUIntStrict(reinterpret_cast<const LChar*>(string), \
                              std::strlen(string), &ok);              \
    EXPECT_FALSE(ok);                                                 \
  } while (false)

  EXPECT_VALID("1", 1u);
  EXPECT_VALID("a", 0xAu);
  EXPECT_VALID("A", 0xAu);
  EXPECT_VALID("+a", 0xAu);
  EXPECT_VALID("+A", 0xAu);
  EXPECT_INVALID("-a");
  EXPECT_INVALID("-A");

  EXPECT_VALID("7fffffff", 0x7FFFFFFFu);
  EXPECT_VALID("80000000", 0x80000000u);
  EXPECT_VALID("fffffff0", 0xFFFFFFF0u);
  EXPECT_VALID("ffffffff", 0xFFFFFFFFu);
  EXPECT_VALID("00ffffffff", 0xFFFFFFFFu);
  EXPECT_INVALID("100000000");
  EXPECT_INVALID("7fffffff0");
  EXPECT_INVALID("-7fffffff");
  EXPECT_INVALID("-80000000");
  EXPECT_INVALID("-80000001");
  EXPECT_INVALID("-8000000a");
  EXPECT_INVALID("-8000000f");
  EXPECT_INVALID("-80000010");
  EXPECT_INVALID("-90000000");
  EXPECT_INVALID("-f0000000");
  EXPECT_INVALID("-fffffff0");
  EXPECT_INVALID("-ffffffff");

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

NumberParsingState ParseUInt(const char* str, unsigned* value) {
  NumberParsingState state;
  *value = CharactersToUIntStrict(reinterpret_cast<const LChar*>(str),
                                  std::strlen(str), &state);
  return state;
}

TEST(StringToNumberTest, NumberParsingState) {
  unsigned value;
  EXPECT_EQ(NumberParsingState::kOverflowMax, ParseUInt("10000000000", &value));
  EXPECT_EQ(NumberParsingState::kError, ParseUInt("10000000000abc", &value));
  EXPECT_EQ(NumberParsingState::kError, ParseUInt("-10000000000", &value));
  EXPECT_EQ(NumberParsingState::kError, ParseUInt("-0", &value));
  EXPECT_EQ(NumberParsingState::kSuccess, ParseUInt("10", &value));
}

}  // namespace WTF
