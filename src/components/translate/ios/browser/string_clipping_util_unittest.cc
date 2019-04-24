// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/ios/browser/string_clipping_util.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that a regular sentence is clipped correctly.
TEST(StringByClippingLastWordTest, ClipRegularSentence) {
  const base::string16 kInput =
      base::UTF8ToUTF16("\nSome text here and there.");
  EXPECT_EQ(kInput, GetStringByClippingLastWord(kInput, 100));
}

// Tests that a long sentence exceeding some length is clipped correctly.
TEST(StringByClippingLastWordTest, ClipLongSentence) {
  // An arbitrary length.
  const size_t kStringLength = 10;
  base::string16 string(kStringLength, 'a');
  string.append(base::UTF8ToUTF16(" b cdefghijklmnopqrstuvwxyz"));
  // The string should be cut at the last whitespace, after the 'b' character.
  base::string16 result =
      GetStringByClippingLastWord(string, kStringLength + 3);
  EXPECT_EQ(kStringLength + 2, result.size());
  EXPECT_EQ(0u, string.find_first_of(result));
}

// Tests that a block of text with no space is truncated to kLongStringLength.
TEST(StringByClippingLastWordTest, ClipLongTextContentNoSpace) {
  // Very long string.
  const size_t kLongStringLength = 65536;
  // A string slightly longer than |kLongStringLength|.
  base::string16 long_string(kLongStringLength + 10, 'a');
  // Block of text with no space should be truncated to kLongStringLength.
  base::string16 result =
      GetStringByClippingLastWord(long_string, kLongStringLength);
  EXPECT_EQ(kLongStringLength, result.size());
  EXPECT_EQ(0u, long_string.find_first_of(result));
}
