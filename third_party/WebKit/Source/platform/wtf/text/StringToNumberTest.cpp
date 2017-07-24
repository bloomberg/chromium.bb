// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/StringToNumber.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <cstring>

namespace WTF {

TEST(StringToNumberTest, CharactersToInt) {
#define EXPECT_VALID(string, options, expectedValue)                    \
  do {                                                                  \
    bool ok;                                                            \
    int value = CharactersToInt(reinterpret_cast<const LChar*>(string), \
                                std::strlen(string),                    \
                                NumberParsingOptions::options, &ok);    \
    EXPECT_TRUE(ok);                                                    \
    EXPECT_EQ(value, expectedValue);                                    \
  } while (false)

#define EXPECT_INVALID(string, options)                                       \
  do {                                                                        \
    bool ok;                                                                  \
    CharactersToInt(reinterpret_cast<const LChar*>(string),                   \
                    std::strlen(string), NumberParsingOptions::options, &ok); \
    EXPECT_FALSE(ok);                                                         \
  } while (false)

  EXPECT_VALID("1", kStrict, 1);
  EXPECT_VALID("2", kStrict, 2);
  EXPECT_VALID("9", kStrict, 9);
  EXPECT_VALID("10", kStrict, 10);
  EXPECT_VALID("0", kStrict, 0);
  EXPECT_VALID("-0", kStrict, 0);
  EXPECT_VALID("-1", kStrict, -1);
  EXPECT_VALID("-2", kStrict, -2);
  EXPECT_VALID("-9", kStrict, -9);
  EXPECT_VALID("-10", kStrict, -10);
  EXPECT_VALID("+0", kStrict, 0);
  EXPECT_VALID("+1", kStrict, 1);
  EXPECT_INVALID("+1", kNone);
  EXPECT_VALID("+2", kStrict, 2);
  EXPECT_VALID("+9", kStrict, 9);
  EXPECT_VALID("+10", kStrict, 10);
  EXPECT_VALID("00", kStrict, 0);
  EXPECT_VALID("+00", kStrict, 0);
  EXPECT_VALID("-00", kStrict, 0);
  EXPECT_VALID("01", kStrict, 1);
  EXPECT_VALID("-01", kStrict, -1);
  EXPECT_VALID("00000000000000000000", kStrict, 0);
  EXPECT_VALID(" 3 ", kStrict, 3);
  EXPECT_INVALID(" 3 ", kNone);
  EXPECT_VALID(" 3 pt", kLoose, 3);
  EXPECT_INVALID(" 3 pt", kStrict);
  EXPECT_VALID("3px", kAcceptTrailingGarbage, 3);
  EXPECT_INVALID("a", kStrict);
  EXPECT_INVALID("1a", kStrict);
  EXPECT_INVALID("a1", kStrict);
  EXPECT_INVALID("-a", kStrict);
  EXPECT_INVALID("", kStrict);
  EXPECT_INVALID("-", kStrict);
  EXPECT_INVALID("--1", kStrict);
  EXPECT_INVALID("++1", kStrict);
  EXPECT_INVALID("+-1", kStrict);
  EXPECT_INVALID("-+1", kStrict);
  EXPECT_INVALID("0-", kStrict);
  EXPECT_INVALID("0+", kStrict);

  EXPECT_VALID("2147483647", kStrict, 2147483647);
  EXPECT_VALID("02147483647", kStrict, 2147483647);
  EXPECT_INVALID("2147483648", kStrict);
  EXPECT_INVALID("2147483649", kStrict);
  EXPECT_INVALID("2147483650", kStrict);
  EXPECT_INVALID("2147483700", kStrict);
  EXPECT_INVALID("2147484000", kStrict);
  EXPECT_INVALID("2200000000", kStrict);
  EXPECT_INVALID("3000000000", kStrict);
  EXPECT_INVALID("10000000000", kStrict);
  EXPECT_VALID("-2147483647", kStrict, -2147483647);
  EXPECT_VALID("-2147483648", kStrict, -2147483647 - 1);
  EXPECT_INVALID("-2147483649", kStrict);
  EXPECT_INVALID("-2147483650", kStrict);
  EXPECT_INVALID("-2147483700", kStrict);
  EXPECT_INVALID("-2147484000", kStrict);
  EXPECT_INVALID("-2200000000", kStrict);
  EXPECT_INVALID("-3000000000", kStrict);
  EXPECT_INVALID("-10000000000", kStrict);

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

TEST(StringToNumberTest, CharactersToUInt) {
#define EXPECT_VALID(string, options, expectedValue)                          \
  do {                                                                        \
    bool ok;                                                                  \
    unsigned value = CharactersToUInt(reinterpret_cast<const LChar*>(string), \
                                      std::strlen(string),                    \
                                      NumberParsingOptions::options, &ok);    \
    EXPECT_TRUE(ok);                                                          \
    EXPECT_EQ(value, expectedValue);                                          \
  } while (false)

#define EXPECT_INVALID(string, options)                                        \
  do {                                                                         \
    bool ok;                                                                   \
    CharactersToUInt(reinterpret_cast<const LChar*>(string),                   \
                     std::strlen(string), NumberParsingOptions::options, &ok); \
    EXPECT_FALSE(ok);                                                          \
  } while (false)

  EXPECT_VALID("1", kStrict, 1u);
  EXPECT_VALID("2", kStrict, 2u);
  EXPECT_VALID("9", kStrict, 9u);
  EXPECT_VALID("10", kStrict, 10u);
  EXPECT_VALID("0", kStrict, 0u);
  EXPECT_VALID("+0", kStrict, 0u);
  EXPECT_VALID("+1", kStrict, 1u);
  EXPECT_VALID("+2", kStrict, 2u);
  EXPECT_VALID("+9", kStrict, 9u);
  EXPECT_VALID("+10", kStrict, 10u);
  EXPECT_INVALID("+10", kNone);
  EXPECT_VALID("00", kStrict, 0u);
  EXPECT_VALID("+00", kStrict, 0u);
  EXPECT_VALID("01", kStrict, 1u);
  EXPECT_VALID("00000000000000000000", kStrict, 0u);
  EXPECT_INVALID("a", kStrict);
  EXPECT_INVALID("1a", kStrict);
  EXPECT_INVALID("a1", kStrict);
  EXPECT_INVALID("-a", kStrict);
  EXPECT_INVALID("", kStrict);
  EXPECT_INVALID("-", kStrict);
  EXPECT_INVALID("-0", kStrict);
  EXPECT_INVALID("-1", kStrict);
  EXPECT_INVALID("-2", kStrict);
  EXPECT_INVALID("-9", kStrict);
  EXPECT_INVALID("-10", kStrict);
  EXPECT_INVALID("-00", kStrict);
  EXPECT_INVALID("-01", kStrict);
  EXPECT_INVALID("--1", kStrict);
  EXPECT_INVALID("++1", kStrict);
  EXPECT_INVALID("+-1", kStrict);
  EXPECT_INVALID("-+1", kStrict);
  EXPECT_INVALID("0-", kStrict);
  EXPECT_INVALID("0+", kStrict);

  EXPECT_VALID("2147483647", kStrict, 2147483647u);
  EXPECT_VALID("02147483647", kStrict, 2147483647u);
  EXPECT_VALID("2147483648", kStrict, 2147483648u);
  EXPECT_VALID("4294967295", kStrict, 4294967295u);
  EXPECT_VALID("0004294967295", kStrict, 4294967295u);
  EXPECT_INVALID("4294967296", kStrict);
  EXPECT_INVALID("4294967300", kStrict);
  EXPECT_INVALID("4300000000", kStrict);
  EXPECT_INVALID("5000000000", kStrict);
  EXPECT_INVALID("10000000000", kStrict);
  EXPECT_INVALID("-2147483647", kStrict);
  EXPECT_INVALID("-2147483648", kStrict);
  EXPECT_INVALID("-2147483649", kStrict);
  EXPECT_INVALID("-2147483650", kStrict);
  EXPECT_INVALID("-2147483700", kStrict);
  EXPECT_INVALID("-2147484000", kStrict);
  EXPECT_INVALID("-2200000000", kStrict);
  EXPECT_INVALID("-3000000000", kStrict);
  EXPECT_INVALID("-10000000000", kStrict);

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

TEST(StringToNumberTest, HexCharactersToUInt) {
#define EXPECT_VALID(string, expectedValue)                          \
  do {                                                               \
    bool ok;                                                         \
    unsigned value = HexCharactersToUInt(                            \
        reinterpret_cast<const LChar*>(string), std::strlen(string), \
        NumberParsingOptions::kStrict, &ok);                         \
    EXPECT_TRUE(ok);                                                 \
    EXPECT_EQ(value, expectedValue);                                 \
  } while (false)

#define EXPECT_INVALID(string)                                              \
  do {                                                                      \
    bool ok;                                                                \
    HexCharactersToUInt(reinterpret_cast<const LChar*>(string),             \
                        std::strlen(string), NumberParsingOptions::kStrict, \
                        &ok);                                               \
    EXPECT_FALSE(ok);                                                       \
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
  *value =
      CharactersToUInt(reinterpret_cast<const LChar*>(str), std::strlen(str),
                       NumberParsingOptions::kStrict, &state);
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
