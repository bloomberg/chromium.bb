// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/StringToNumber.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <cstring>

namespace WTF {

TEST(StringToNumberTest, TestCharactersToIntStrict) {
#define EXPECT_VALID(string, expectedValue, base)                             \
  do {                                                                        \
    bool ok;                                                                  \
    int value = CharactersToIntStrict(reinterpret_cast<const LChar*>(string), \
                                      std::strlen(string), &ok, base);        \
    EXPECT_TRUE(ok);                                                          \
    EXPECT_EQ(value, expectedValue);                                          \
  } while (false)

#define EXPECT_INVALID(string, base)                              \
  do {                                                            \
    bool ok;                                                      \
    CharactersToIntStrict(reinterpret_cast<const LChar*>(string), \
                          std::strlen(string), &ok, base);        \
    EXPECT_FALSE(ok);                                             \
  } while (false)

#define EXPECT_VALID_DECIMAL(string, expectedValue) \
  EXPECT_VALID(string, expectedValue, 10)
#define EXPECT_INVALID_DECIMAL(string) EXPECT_INVALID(string, 10)
#define EXPECT_VALID_HEXADECIMAL(string, expectedValue) \
  EXPECT_VALID(string, expectedValue, 16)
#define EXPECT_INVALID_HEXADECIMAL(string) EXPECT_INVALID(string, 16)

  EXPECT_VALID_DECIMAL("1", 1);
  EXPECT_VALID_DECIMAL("2", 2);
  EXPECT_VALID_DECIMAL("9", 9);
  EXPECT_VALID_DECIMAL("10", 10);
  EXPECT_VALID_DECIMAL("0", 0);
  EXPECT_VALID_DECIMAL("-0", 0);
  EXPECT_VALID_DECIMAL("-1", -1);
  EXPECT_VALID_DECIMAL("-2", -2);
  EXPECT_VALID_DECIMAL("-9", -9);
  EXPECT_VALID_DECIMAL("-10", -10);
  EXPECT_VALID_DECIMAL("+0", 0);
  EXPECT_VALID_DECIMAL("+1", 1);
  EXPECT_VALID_DECIMAL("+2", 2);
  EXPECT_VALID_DECIMAL("+9", 9);
  EXPECT_VALID_DECIMAL("+10", 10);
  EXPECT_VALID_DECIMAL("00", 0);
  EXPECT_VALID_DECIMAL("+00", 0);
  EXPECT_VALID_DECIMAL("-00", 0);
  EXPECT_VALID_DECIMAL("01", 1);
  EXPECT_VALID_DECIMAL("-01", -1);
  EXPECT_VALID_DECIMAL("00000000000000000000", 0);
  EXPECT_INVALID_DECIMAL("a");
  EXPECT_INVALID_DECIMAL("1a");
  EXPECT_INVALID_DECIMAL("a1");
  EXPECT_INVALID_DECIMAL("-a");
  EXPECT_INVALID_DECIMAL("");
  EXPECT_INVALID_DECIMAL("-");
  EXPECT_INVALID_DECIMAL("--1");
  EXPECT_INVALID_DECIMAL("++1");
  EXPECT_INVALID_DECIMAL("+-1");
  EXPECT_INVALID_DECIMAL("-+1");
  EXPECT_INVALID_DECIMAL("0-");
  EXPECT_INVALID_DECIMAL("0+");

  EXPECT_VALID_DECIMAL("2147483647", 2147483647);
  EXPECT_VALID_DECIMAL("02147483647", 2147483647);
  EXPECT_INVALID_DECIMAL("2147483648");
  EXPECT_INVALID_DECIMAL("2147483649");
  EXPECT_INVALID_DECIMAL("2147483650");
  EXPECT_INVALID_DECIMAL("2147483700");
  EXPECT_INVALID_DECIMAL("2147484000");
  EXPECT_INVALID_DECIMAL("2200000000");
  EXPECT_INVALID_DECIMAL("3000000000");
  EXPECT_INVALID_DECIMAL("10000000000");
  EXPECT_VALID_DECIMAL("-2147483647", -2147483647);
  EXPECT_VALID_DECIMAL("-2147483648", -2147483647 - 1);
  EXPECT_INVALID_DECIMAL("-2147483649");
  EXPECT_INVALID_DECIMAL("-2147483650");
  EXPECT_INVALID_DECIMAL("-2147483700");
  EXPECT_INVALID_DECIMAL("-2147484000");
  EXPECT_INVALID_DECIMAL("-2200000000");
  EXPECT_INVALID_DECIMAL("-3000000000");
  EXPECT_INVALID_DECIMAL("-10000000000");

  EXPECT_VALID_HEXADECIMAL("1", 1);
  EXPECT_VALID_HEXADECIMAL("a", 0xA);
  EXPECT_VALID_HEXADECIMAL("A", 0xA);
  EXPECT_VALID_HEXADECIMAL("+a", 0xA);
  EXPECT_VALID_HEXADECIMAL("+A", 0xA);
  EXPECT_VALID_HEXADECIMAL("-a", -0xA);
  EXPECT_VALID_HEXADECIMAL("-A", -0xA);

  EXPECT_VALID_HEXADECIMAL("7fffffff", 0x7FFFFFFF);
  EXPECT_INVALID_HEXADECIMAL("80000000");
  EXPECT_INVALID_HEXADECIMAL("8000000a");
  EXPECT_INVALID_HEXADECIMAL("8000000f");
  EXPECT_INVALID_HEXADECIMAL("90000000");
  EXPECT_INVALID_HEXADECIMAL("fffffff0");
  EXPECT_INVALID_HEXADECIMAL("ffffffff");
  EXPECT_INVALID_HEXADECIMAL("100000000");
  EXPECT_INVALID_HEXADECIMAL("7fffffff0");
  EXPECT_VALID_HEXADECIMAL("-7fffffff", -0x7FFFFFFF);
  EXPECT_VALID_HEXADECIMAL("-80000000", -0x7FFFFFFF - 1);
  EXPECT_INVALID_HEXADECIMAL("-80000001");
  EXPECT_INVALID_HEXADECIMAL("-8000000a");
  EXPECT_INVALID_HEXADECIMAL("-8000000f");
  EXPECT_INVALID_HEXADECIMAL("-80000010");
  EXPECT_INVALID_HEXADECIMAL("-90000000");
  EXPECT_INVALID_HEXADECIMAL("-f0000000");
  EXPECT_INVALID_HEXADECIMAL("-fffffff0");
  EXPECT_INVALID_HEXADECIMAL("-ffffffff");

#undef EXPECT_VALID_DECIMAL
#undef EXPECT_INVALID_DECIMAL
#undef EXPECT_VALID_HEXADECIMAL
#undef EXPECT_INVALID_HEXADECIMAL
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
}
