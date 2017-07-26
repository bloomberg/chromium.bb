// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/TextCodecICU.h"

#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CharacterNames.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(TextCodecICUTest, IgnorableCodePoint) {
  TextEncoding iso2022jp("iso-2022-jp");
  std::unique_ptr<TextCodec> codec = TextCodecICU::Create(iso2022jp, nullptr);
  Vector<UChar> source;
  source.push_back('a');
  source.push_back(kZeroWidthJoinerCharacter);
  CString encoded =
      codec->Encode(source.data(), source.size(), kEntitiesForUnencodables);
  EXPECT_STREQ("a&#8205;", encoded.data());
}
}
