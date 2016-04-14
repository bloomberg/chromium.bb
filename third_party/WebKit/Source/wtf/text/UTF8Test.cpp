// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/UTF8.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace Unicode {

TEST(UTF8Test, IsUTF8andNotASCII)
{
    EXPECT_TRUE(isUTF8andNotASCII("\xc2\x81", 2));
    EXPECT_TRUE(isUTF8andNotASCII("\xe1\x80\xbf", 3));
    EXPECT_TRUE(isUTF8andNotASCII("\xf1\x80\xa0\xbf", 4));
    EXPECT_TRUE(isUTF8andNotASCII("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf", 10));

    // Surrogate code points
    EXPECT_FALSE(isUTF8andNotASCII("\xed\xa0\x80\xed\xbf\xbf", 6));
    EXPECT_FALSE(isUTF8andNotASCII("\xed\xa0\x8f", 3));
    EXPECT_FALSE(isUTF8andNotASCII("\xed\xbf\xbf", 3));

    // Overlong sequences
    EXPECT_FALSE(isUTF8andNotASCII("\xc0\x80", 2)); // U+0000
    EXPECT_FALSE(isUTF8andNotASCII("\xc1\x80\xc1\x81", 4)); // "AB"
    EXPECT_FALSE(isUTF8andNotASCII("\xe0\x80\x80", 3)); // U+0000
    EXPECT_FALSE(isUTF8andNotASCII("\xe0\x82\x80", 3)); // U+0080
    EXPECT_FALSE(isUTF8andNotASCII("\xe0\x9f\xbf", 3)); // U+07ff
    EXPECT_FALSE(isUTF8andNotASCII("\xf0\x80\x80\x8D", 4)); // U+000D
    EXPECT_FALSE(isUTF8andNotASCII("\xf0\x80\x82\x91", 4)); // U+0091
    EXPECT_FALSE(isUTF8andNotASCII("\xf0\x80\xa0\x80", 4)); // U+0800
    EXPECT_FALSE(isUTF8andNotASCII("\xf0\x8f\xbb\xbf", 4)); // U+FEFF (BOM)
    EXPECT_FALSE(isUTF8andNotASCII("\xf8\x80\x80\x80\xbf", 5)); // U+003F
    EXPECT_FALSE(isUTF8andNotASCII("\xfc\x80\x80\x80\xa0\xa5", 6)); // U+00A5

    // Beyond U+10FFFF (the upper limit of Unicode codespace)
    EXPECT_FALSE(isUTF8andNotASCII("\xf4\x90\x80\x80", 4)); // U+110000
    EXPECT_FALSE(isUTF8andNotASCII("\xf8\xa0\xbf\x80\xbf", 5)); // 5 bytes
    EXPECT_FALSE(isUTF8andNotASCII("\xfc\x9c\xbf\x80\xbf\x80", 6)); // 6 bytes

    // Non-characters : U+xxFFF[EF] where xx is 0x00 through 0x10 and <FDD0,FDEF>
    EXPECT_FALSE(isUTF8andNotASCII("\xef\xbf\xbe", 3)); // U+FFFE
    EXPECT_FALSE(isUTF8andNotASCII("\xf0\x8f\xbf\xbe", 4)); // U+1FFFE
    EXPECT_FALSE(isUTF8andNotASCII("\xf3\xbf\xbf\xbf", 4)); // U+10FFFF
    EXPECT_FALSE(isUTF8andNotASCII("\xef\xb7\x90", 3)); // U+FDD0
    EXPECT_FALSE(isUTF8andNotASCII("\xef\xb7\xaf", 3)); // U+FDEF

    // Strings in legacy encodings.
    EXPECT_FALSE(isUTF8andNotASCII("caf\xe9", 4)); // cafe with U+00E9 in ISO-8859-1
    EXPECT_FALSE(isUTF8andNotASCII("\xb0\xa1\xb0\xa2", 4)); // U+AC00, U+AC001 in EUC-KR
    EXPECT_FALSE(isUTF8andNotASCII("\xa7\x41\xa6\x6e", 4)); // U+4F60 U+597D in Big5
    // "abc" with U+201[CD] in windows-125[0-8]
    EXPECT_FALSE(isUTF8andNotASCII("\x93" "abc\x94", 4));
    // U+0639 U+064E U+0644 U+064E in ISO-8859-6
    EXPECT_FALSE(isUTF8andNotASCII("\xd9\xee\xe4\xee", 4));
    // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
    EXPECT_FALSE(isUTF8andNotASCII("\xe3\xe5\xe9\xdC", 4));
    EXPECT_FALSE(isUTF8andNotASCII("abc", 3)); // plain ASCII
}

} // namespace Unicode
} // namespace WTF
