// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/TextResourceDecoder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TextResourceDecoderTest, KeepEncodingConsistent)
{
    bool useAutoEncodingDetector = true;
    OwnPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("text/plain", WTF::TextEncoding(), useAutoEncodingDetector);

    // First |decode()| call initializes the internal text codec with UTF-8
    // which was inferred by the auto encoding detector.
    decoder->decode("\xc2\xa7", 2);
    ASSERT_EQ(UTF8Encoding(), decoder->encoding());

    // Subsequent |decode()| is called with non-UTF-8 text (EUC-KR),
    // but the codec remains fixed.
    decoder->decode("\xc4\x22\xc4\x5c", 2);
    EXPECT_EQ(UTF8Encoding(), decoder->encoding());

    // |TextResourceDecoder| is created not to use auto encoding detector,
    // which activates the light-weight UTF-8 encoding detector.
    decoder = TextResourceDecoder::create("text/plain");

    decoder->decode("abcde", 5);
    ASSERT_EQ(Latin1Encoding(), decoder->encoding());

    // Verify that the encoding(Latin1) used for the first |decode()| remains
    // fixed even if the subsequent call is given UTF-8 text.
    decoder->decode("\xc2\xa7", 2);
    EXPECT_EQ(Latin1Encoding(), decoder->encoding());
}
}
