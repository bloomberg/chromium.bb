// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/fx_extension.h"

#include <math.h>

#include <iterator>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcrt, FXSYS_IsLowerASCII) {
  EXPECT_TRUE(FXSYS_IsLowerASCII('a'));
  EXPECT_TRUE(FXSYS_IsLowerASCII(L'a'));
  EXPECT_TRUE(FXSYS_IsLowerASCII('b'));
  EXPECT_TRUE(FXSYS_IsLowerASCII(L'b'));
  EXPECT_TRUE(FXSYS_IsLowerASCII('y'));
  EXPECT_TRUE(FXSYS_IsLowerASCII(L'y'));
  EXPECT_TRUE(FXSYS_IsLowerASCII('z'));
  EXPECT_TRUE(FXSYS_IsLowerASCII(L'z'));
  EXPECT_FALSE(FXSYS_IsLowerASCII('`'));
  EXPECT_FALSE(FXSYS_IsLowerASCII(L'`'));
  EXPECT_FALSE(FXSYS_IsLowerASCII('{'));
  EXPECT_FALSE(FXSYS_IsLowerASCII(L'{'));
  EXPECT_FALSE(FXSYS_IsLowerASCII('Z'));
  EXPECT_FALSE(FXSYS_IsLowerASCII(L'Z'));
  EXPECT_FALSE(FXSYS_IsLowerASCII('7'));
  EXPECT_FALSE(FXSYS_IsLowerASCII(L'7'));
  EXPECT_FALSE(FXSYS_IsLowerASCII(static_cast<char>(-78)));
  EXPECT_FALSE(FXSYS_IsLowerASCII(static_cast<wchar_t>(0xb2)));
}

TEST(fxcrt, FXSYS_IsUpperASCII) {
  EXPECT_TRUE(FXSYS_IsUpperASCII('A'));
  EXPECT_TRUE(FXSYS_IsUpperASCII(L'A'));
  EXPECT_TRUE(FXSYS_IsUpperASCII('B'));
  EXPECT_TRUE(FXSYS_IsUpperASCII(L'B'));
  EXPECT_TRUE(FXSYS_IsUpperASCII('Y'));
  EXPECT_TRUE(FXSYS_IsUpperASCII(L'Y'));
  EXPECT_TRUE(FXSYS_IsUpperASCII('Z'));
  EXPECT_TRUE(FXSYS_IsUpperASCII(L'Z'));
  EXPECT_FALSE(FXSYS_IsUpperASCII('@'));
  EXPECT_FALSE(FXSYS_IsUpperASCII(L'@'));
  EXPECT_FALSE(FXSYS_IsUpperASCII('['));
  EXPECT_FALSE(FXSYS_IsUpperASCII(L'['));
  EXPECT_FALSE(FXSYS_IsUpperASCII('z'));
  EXPECT_FALSE(FXSYS_IsUpperASCII(L'z'));
  EXPECT_FALSE(FXSYS_IsUpperASCII('7'));
  EXPECT_FALSE(FXSYS_IsUpperASCII(L'7'));
  EXPECT_FALSE(FXSYS_IsUpperASCII(static_cast<char>(-78)));
  EXPECT_FALSE(FXSYS_IsUpperASCII(static_cast<wchar_t>(0xb2)));
}

TEST(fxcrt, FXSYS_HexCharToInt) {
  EXPECT_EQ(10, FXSYS_HexCharToInt('a'));
  EXPECT_EQ(10, FXSYS_HexCharToInt('A'));
  EXPECT_EQ(7, FXSYS_HexCharToInt('7'));
  EXPECT_EQ(0, FXSYS_HexCharToInt('i'));
}

TEST(fxcrt, FXSYS_DecimalCharToInt) {
  EXPECT_EQ(7, FXSYS_DecimalCharToInt('7'));
  EXPECT_EQ(0, FXSYS_DecimalCharToInt('a'));
  EXPECT_EQ(7, FXSYS_DecimalCharToInt(L'7'));
  EXPECT_EQ(0, FXSYS_DecimalCharToInt(L'a'));
  EXPECT_EQ(0, FXSYS_DecimalCharToInt(static_cast<char>(-78)));
  EXPECT_EQ(0, FXSYS_DecimalCharToInt(static_cast<wchar_t>(0xb2)));
}

TEST(fxcrt, FXSYS_IsDecimalDigit) {
  EXPECT_TRUE(FXSYS_IsDecimalDigit('7'));
  EXPECT_TRUE(FXSYS_IsDecimalDigit(L'7'));
  EXPECT_FALSE(FXSYS_IsDecimalDigit('a'));
  EXPECT_FALSE(FXSYS_IsDecimalDigit(L'a'));
  EXPECT_FALSE(FXSYS_IsDecimalDigit(static_cast<char>(-78)));
  EXPECT_FALSE(FXSYS_IsDecimalDigit(static_cast<wchar_t>(0xb2)));
}

TEST(fxcrt, FXSYS_IntToTwoHexChars) {
  char buf[3] = {0};
  FXSYS_IntToTwoHexChars(0x0, buf);
  EXPECT_STREQ("00", buf);
  FXSYS_IntToTwoHexChars(0x9, buf);
  EXPECT_STREQ("09", buf);
  FXSYS_IntToTwoHexChars(0xA, buf);
  EXPECT_STREQ("0A", buf);
  FXSYS_IntToTwoHexChars(0x8C, buf);
  EXPECT_STREQ("8C", buf);
  FXSYS_IntToTwoHexChars(0xBE, buf);
  EXPECT_STREQ("BE", buf);
  FXSYS_IntToTwoHexChars(0xD0, buf);
  EXPECT_STREQ("D0", buf);
  FXSYS_IntToTwoHexChars(0xFF, buf);
  EXPECT_STREQ("FF", buf);
}

TEST(fxcrt, FXSYS_IntToFourHexChars) {
  char buf[5] = {0};
  FXSYS_IntToFourHexChars(0x0, buf);
  EXPECT_STREQ("0000", buf);
  FXSYS_IntToFourHexChars(0xA23, buf);
  EXPECT_STREQ("0A23", buf);
  FXSYS_IntToFourHexChars(0xB701, buf);
  EXPECT_STREQ("B701", buf);
  FXSYS_IntToFourHexChars(0xFFFF, buf);
  EXPECT_STREQ("FFFF", buf);
}

TEST(fxcrt, FXSYS_ToUTF16BE) {
  char buf[9] = {0};
  // Test U+0000 to U+D7FF and U+E000 to U+FFFF
  EXPECT_EQ(4U, FXSYS_ToUTF16BE(0x0, buf));
  EXPECT_STREQ("0000", buf);
  EXPECT_EQ(4U, FXSYS_ToUTF16BE(0xD7FF, buf));
  EXPECT_STREQ("D7FF", buf);
  EXPECT_EQ(4U, FXSYS_ToUTF16BE(0xE000, buf));
  EXPECT_STREQ("E000", buf);
  EXPECT_EQ(4U, FXSYS_ToUTF16BE(0xFFFF, buf));
  EXPECT_STREQ("FFFF", buf);
  // Test U+10000 to U+10FFFF
  EXPECT_EQ(8U, FXSYS_ToUTF16BE(0x10000, buf));
  EXPECT_STREQ("D800DC00", buf);
  EXPECT_EQ(8U, FXSYS_ToUTF16BE(0x10FFFF, buf));
  EXPECT_STREQ("DBFFDFFF", buf);
  EXPECT_EQ(8U, FXSYS_ToUTF16BE(0x2003E, buf));
  EXPECT_STREQ("D840DC3E", buf);
}

TEST(fxcrt, FXSYS_wcstof) {
  size_t used_len = 0;
  EXPECT_FLOAT_EQ(-12.0f, FXSYS_wcstof(L"-12", 3, &used_len));
  EXPECT_EQ(3u, used_len);

  used_len = 0;
  EXPECT_FLOAT_EQ(1.5362f, FXSYS_wcstof(L"1.5362", 6, &used_len));
  EXPECT_EQ(6u, used_len);

  used_len = 0;
  EXPECT_FLOAT_EQ(0.875f, FXSYS_wcstof(L"0.875", 5, &used_len));
  EXPECT_EQ(5u, used_len);

  used_len = 0;
  EXPECT_FLOAT_EQ(5.56e-2f, FXSYS_wcstof(L"5.56e-2", 7, &used_len));
  EXPECT_EQ(7u, used_len);

  used_len = 0;
  EXPECT_FLOAT_EQ(1.234e10f, FXSYS_wcstof(L"1.234E10", 8, &used_len));
  EXPECT_EQ(8u, used_len);

  used_len = 0;
  EXPECT_FLOAT_EQ(0.0f, FXSYS_wcstof(L"1.234E100000000000000", 21, &used_len));
  EXPECT_EQ(0u, used_len);

  used_len = 0;
  EXPECT_FLOAT_EQ(0.0f, FXSYS_wcstof(L"1.234E-128", 21, &used_len));
  EXPECT_EQ(0u, used_len);

  // TODO(dsinclair): This should round as per IEEE 64-bit values.
  // EXPECT_EQ(L"123456789.01234567", FXSYS_wcstof(L"123456789.012345678"));
  used_len = 0;
  EXPECT_FLOAT_EQ(123456789.012345678f,
                  FXSYS_wcstof(L"123456789.012345678", 19, &used_len));
  EXPECT_EQ(19u, used_len);

  // TODO(dsinclair): This is spec'd as rounding when > 16 significant digits
  // prior to the exponent.
  // EXPECT_EQ(100000000000000000, FXSYS_wcstof(L"99999999999999999"));
  used_len = 0;
  EXPECT_FLOAT_EQ(99999999999999999.0f,
                  FXSYS_wcstof(L"99999999999999999", 17, &used_len));
  EXPECT_EQ(17u, used_len);

  // For https://crbug.com/pdfium/1217
  EXPECT_FLOAT_EQ(0.0f, FXSYS_wcstof(L"e76", 3, nullptr));

  // Overflow to infinity.
  used_len = 0;
  EXPECT_TRUE(isinf(FXSYS_wcstof(
      L"88888888888888888888888888888888888888888888888888888888888888888888888"
      L"88888888888888888888888888888888888888888888888888888888888",
      130, &used_len)));
  EXPECT_EQ(130u, used_len);

  used_len = 0;
  EXPECT_TRUE(isinf(FXSYS_wcstof(
      L"-8888888888888888888888888888888888888888888888888888888888888888888888"
      L"888888888888888888888888888888888888888888888888888888888888",
      131, &used_len)));
  EXPECT_EQ(131u, used_len);
}

TEST(fxcrt, FXSYS_SafeOps) {
  const float fMin = std::numeric_limits<float>::min();
  const float fMax = std::numeric_limits<float>::max();
  const float fInf = std::numeric_limits<float>::infinity();
  const float fNan = std::numeric_limits<float>::quiet_NaN();
  const float ascending[] = {fMin, 1.0f, 2.0f, fMax, fInf, fNan};

  for (size_t i = 0; i < std::size(ascending); ++i) {
    for (size_t j = 0; j < std::size(ascending); ++j) {
      if (i == j) {
        EXPECT_TRUE(FXSYS_SafeEQ(ascending[i], ascending[j]))
            << " at " << i << " " << j;
      } else {
        EXPECT_FALSE(FXSYS_SafeEQ(ascending[i], ascending[j]))
            << " at " << i << " " << j;
      }
      if (i < j) {
        EXPECT_TRUE(FXSYS_SafeLT(ascending[i], ascending[j]))
            << " at " << i << " " << j;
      } else {
        EXPECT_FALSE(FXSYS_SafeLT(ascending[i], ascending[j]))
            << " at " << i << " " << j;
      }
    }
  }
}
