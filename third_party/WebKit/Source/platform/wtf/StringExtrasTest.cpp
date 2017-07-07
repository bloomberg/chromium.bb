/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/wtf/StringExtras.h"

#include <limits>

#include "build/build_config.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

template <typename IntegerType>
struct PrintfFormatTrait {
  static const char kFormat[];
};

template <>
struct PrintfFormatTrait<short> {
  static const char kFormat[];
};
const char PrintfFormatTrait<short>::kFormat[] = "%hd";

template <>
struct PrintfFormatTrait<int> {
  static const char kFormat[];
};
const char PrintfFormatTrait<int>::kFormat[] = "%d";

template <>
struct PrintfFormatTrait<long> {
  static const char kFormat[];
};
const char PrintfFormatTrait<long>::kFormat[] = "%ld";

template <>
struct PrintfFormatTrait<long long> {
  static const char kFormat[];
};
#if defined(OS_WIN)
const char PrintfFormatTrait<long long>::kFormat[] = "%I64i";
#else
const char PrintfFormatTrait<long long>::kFormat[] = "%lli";
#endif  // defined(OS_WIN)

template <>
struct PrintfFormatTrait<unsigned short> {
  static const char kFormat[];
};
const char PrintfFormatTrait<unsigned short>::kFormat[] = "%hu";

template <>
struct PrintfFormatTrait<unsigned> {
  static const char kFormat[];
};
const char PrintfFormatTrait<unsigned>::kFormat[] = "%u";

template <>
struct PrintfFormatTrait<unsigned long> {
  static const char kFormat[];
};
const char PrintfFormatTrait<unsigned long>::kFormat[] = "%lu";

template <>
struct PrintfFormatTrait<unsigned long long> {
  static const char kFormat[];
};
#if defined(OS_WIN)
const char PrintfFormatTrait<unsigned long long>::kFormat[] = "%I64u";
#else
const char PrintfFormatTrait<unsigned long long>::kFormat[] = "%llu";
#endif  // defined(OS_WIN)

// FIXME: use snprintf from StringExtras.h
template <typename IntegerType>
void TestBoundaries() {
  const unsigned kBufferSize = 256;
  Vector<char, kBufferSize> buffer;
  buffer.resize(kBufferSize);

  const IntegerType min = std::numeric_limits<IntegerType>::min();
  CString min_string_data = String::Number(min).Latin1();
  snprintf(buffer.data(), kBufferSize, PrintfFormatTrait<IntegerType>::kFormat,
           min);
  EXPECT_STREQ(buffer.data(), min_string_data.data());

  const IntegerType max = std::numeric_limits<IntegerType>::max();
  CString max_string_data = String::Number(max).Latin1();
  snprintf(buffer.data(), kBufferSize, PrintfFormatTrait<IntegerType>::kFormat,
           max);
  EXPECT_STREQ(buffer.data(), max_string_data.data());
}

template <typename IntegerType>
void TestNumbers() {
  const unsigned kBufferSize = 256;
  Vector<char, kBufferSize> buffer;
  buffer.resize(kBufferSize);

  for (int i = -100; i < 100; ++i) {
    const IntegerType number = static_cast<IntegerType>(i);
    CString number_string_data = String::Number(number).Latin1();
    snprintf(buffer.data(), kBufferSize,
             PrintfFormatTrait<IntegerType>::kFormat, number);
    EXPECT_STREQ(buffer.data(), number_string_data.data());
  }
}

TEST(StringExtraTest, IntegerToStringConversionSignedIntegerBoundaries) {
  TestBoundaries<short>();
  TestBoundaries<int>();
  TestBoundaries<long>();
  TestBoundaries<long long>();
}

TEST(StringExtraTest, IntegerToStringConversionSignedIntegerRegularNumbers) {
  TestNumbers<short>();
  TestNumbers<int>();
  TestNumbers<long>();
  TestNumbers<long long>();
}

TEST(StringExtraTest, IntegerToStringConversionUnsignedIntegerBoundaries) {
  TestBoundaries<unsigned short>();
  TestBoundaries<unsigned>();
  TestBoundaries<unsigned long>();
  TestBoundaries<unsigned long long>();
}

TEST(StringExtraTest, IntegerToStringConversionUnsignedIntegerRegularNumbers) {
  TestNumbers<unsigned short>();
  TestNumbers<unsigned>();
  TestNumbers<unsigned long>();
  TestNumbers<unsigned long long>();
}

}  // namespace WTF
