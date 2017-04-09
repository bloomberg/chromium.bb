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
      codec->Encode(source.Data(), source.size(), kEntitiesForUnencodables);
  EXPECT_STREQ("a&#8205;", encoded.Data());
}

TEST(TextCodecICUTest, UTF32AndQuestionMarks) {
  Vector<String> aliases;
  Vector<CString> results;

  const UChar kPoo[] = {0xd83d, 0xdca9};  // U+1F4A9 PILE OF POO

  aliases.push_back("UTF-32");
  results.push_back("\xFF\xFE\x00\x00\xA9\xF4\x01\x00");

  aliases.push_back("UTF-32LE");
  results.push_back("\xA9\xF4\x01\x00");

  aliases.push_back("UTF-32BE");
  results.push_back("\x00\x01\xF4\xA9");

  ASSERT_EQ(aliases.size(), results.size());
  for (unsigned i = 0; i < aliases.size(); ++i) {
    const String& alias = aliases[i];
    const CString& expected = results[i];

    TextEncoding encoding(alias);
    std::unique_ptr<TextCodec> codec = TextCodecICU::Create(encoding, nullptr);
    {
      const UChar* data = nullptr;
      CString encoded = codec->Encode(data, 0, kQuestionMarksForUnencodables);
      EXPECT_STREQ("", encoded.Data());
    }
    {
      CString encoded = codec->Encode(kPoo, WTF_ARRAY_LENGTH(kPoo),
                                      kQuestionMarksForUnencodables);
      EXPECT_STREQ(expected.Data(), encoded.Data());
    }
  }
}
}
