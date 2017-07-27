/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontDescription.h"

#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static inline void AssertDescriptionMatchesMask(FontDescription& source,
                                                FontTraitsBitfield bitfield) {
  FontDescription target;
  target.SetTraits(FontTraits(bitfield));
  EXPECT_EQ(source.Style(), target.Style());
  EXPECT_EQ(source.Weight(), target.Weight());
  EXPECT_EQ(source.Stretch(), target.Stretch());
}

TEST(FontDescriptionTest, TestFontTraits) {
  FontDescription source;
  source.SetStyle(kFontStyleNormal);
  source.SetWeight(kFontWeightNormal);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleNormal);
  source.SetWeight(kFontWeightNormal);
  source.SetStretch(kFontStretchExtraCondensed);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight900);
  source.SetStretch(kFontStretchUltraExpanded);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight100);
  source.SetStretch(kFontStretchExtraExpanded);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight900);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight800);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight700);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight600);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight500);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight400);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight300);
  source.SetStretch(kFontStretchUltraExpanded);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());

  source.SetStyle(kFontStyleItalic);
  source.SetWeight(kFontWeight200);
  source.SetStretch(kFontStretchNormal);
  AssertDescriptionMatchesMask(source, source.Traits().Bitfield());
}

TEST(FontDescriptionTest, TestHashCollision) {
  FontWeight weights[] = {
      kFontWeight100, kFontWeight200, kFontWeight300,
      kFontWeight400, kFontWeight500, kFontWeight600,
      kFontWeight700, kFontWeight800, kFontWeight900,
  };
  FontStretch stretches[]{
      kFontStretchUltraCondensed, kFontStretchExtraCondensed,
      kFontStretchCondensed,      kFontStretchSemiCondensed,
      kFontStretchNormal,         kFontStretchSemiExpanded,
      kFontStretchExpanded,       kFontStretchExtraExpanded,
      kFontStretchUltraExpanded};
  FontStyle styles[] = {kFontStyleNormal, kFontStyleOblique, kFontStyleItalic};

  FontDescription source;
  WTF::Vector<unsigned> hashes;
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(weights); i++) {
    source.SetWeight(weights[i]);
    for (size_t j = 0; j < WTF_ARRAY_LENGTH(stretches); j++) {
      source.SetStretch(stretches[j]);
      for (size_t k = 0; k < WTF_ARRAY_LENGTH(styles); k++) {
        source.SetStyle(styles[k]);
        unsigned hash = source.StyleHashWithoutFamilyList();
        ASSERT_FALSE(hashes.Contains(hash));
        hashes.push_back(hash);
      }
    }
  }
}

}  // namespace blink
