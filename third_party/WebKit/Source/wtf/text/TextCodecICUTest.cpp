// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/TextCodecICU.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"

namespace WTF {

TEST(TextCodecICUTest, IgnorableCodePoint)
{
    TextEncoding iso2022jp("iso-2022-jp");
    std::unique_ptr<TextCodec> codec = TextCodecICU::create(iso2022jp, nullptr);
    Vector<UChar> source;
    source.append('a');
    source.append(zeroWidthJoinerCharacter);
    CString encoded = codec->encode(source.data(), source.size(), EntitiesForUnencodables);
    EXPECT_STREQ("a&#8205;", encoded.data());
}

}
