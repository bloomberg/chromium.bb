// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebString.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(WebStringTest, UTF8ConversionRoundTrip) {
  // Valid characters.
  for (UChar uchar = 0; uchar <= 0xD7FF; ++uchar) {
    WebString utf16String(&uchar, 1);
    std::string utf8String(utf16String.utf8());
    WebString utf16NewString =
        WebString::fromUTF8(utf8String.data(), utf8String.length());
    EXPECT_FALSE(utf16NewString.isNull());
    EXPECT_TRUE(utf16String == utf16NewString);
  }

  // Unpaired surrogates.
  for (UChar uchar = 0xD800; uchar <= 0xDFFF; ++uchar) {
    WebString utf16String(&uchar, 1);

    // Conversion with Strict mode results in an empty string.
    std::string utf8String(
        utf16String.utf8(WebString::UTF8ConversionMode::kStrict));
    EXPECT_TRUE(utf8String.empty());

    // Unpaired surrogates can't be converted back in Lenient mode.
    utf8String = utf16String.utf8(WebString::UTF8ConversionMode::kLenient);
    EXPECT_FALSE(utf8String.empty());
    WebString utf16NewString =
        WebString::fromUTF8(utf8String.data(), utf8String.length());
    EXPECT_TRUE(utf16NewString.isNull());

    // Round-trip works with StrictReplacingErrorsWithFFFD mode.
    utf8String = utf16String.utf8(
        WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
    EXPECT_FALSE(utf8String.empty());
    utf16NewString =
        WebString::fromUTF8(utf8String.data(), utf8String.length());
    EXPECT_FALSE(utf16NewString.isNull());
    EXPECT_FALSE(utf16String == utf16NewString);
  }
}

}  // namespace blink
