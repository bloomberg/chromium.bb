// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Optional.h"

namespace blink {

namespace {

// Minimal TestSimpleFontData implementation.
// Font has no glyphs, but that's okay.
class TestSimpleFontData : public SimpleFontData {
 public:
  static PassRefPtr<TestSimpleFontData> create() {
    return adoptRef(new TestSimpleFontData);
  }

 private:
  TestSimpleFontData() : SimpleFontData(nullptr, 10, false, false) {}
};

}  // anonymous namespace

TEST(ShapeResultBloberizerTest, StartsEmpty) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunFontData(bloberizer),
            nullptr);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunGlyphs(bloberizer).size(),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunOffsets(bloberizer).size(),
            0ul);
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::hasPendingRunVerticalOffsets(bloberizer));
  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::committedBlobCount(bloberizer), 0ul);

  EXPECT_TRUE(bloberizer.blobs().isEmpty());
}

TEST(ShapeResultBloberizerTest, StoresGlyphsOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  RefPtr<SimpleFontData> font1 = TestSimpleFontData::create();
  RefPtr<SimpleFontData> font2 = TestSimpleFontData::create();

  // 2 pending glyphs
  bloberizer.add(42, font1.get(), 10);
  bloberizer.add(43, font1.get(), 15);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunFontData(bloberizer),
            font1.get());
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::hasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::pendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 2ul);
    EXPECT_EQ(42, glyphs[0]);
    EXPECT_EQ(43, glyphs[1]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::pendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 2ul);
    EXPECT_EQ(10, offsets[0]);
    EXPECT_EQ(15, offsets[1]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::committedBlobCount(bloberizer), 0ul);

  // one more glyph, different font => pending run flush
  bloberizer.add(44, font2.get(), 12);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunFontData(bloberizer),
            font2.get());
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::hasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::pendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 1ul);
    EXPECT_EQ(44, glyphs[0]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::pendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 1ul);
    EXPECT_EQ(12, offsets[0]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingBlobRunCount(bloberizer),
            1ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::committedBlobCount(bloberizer), 0ul);

  // flush everything (1 blob w/ 2 runs)
  EXPECT_EQ(bloberizer.blobs().size(), 1ul);
}

TEST(ShapeResultBloberizerTest, StoresGlyphsVerticalOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  RefPtr<SimpleFontData> font1 = TestSimpleFontData::create();
  RefPtr<SimpleFontData> font2 = TestSimpleFontData::create();

  // 2 pending glyphs
  bloberizer.add(42, font1.get(), FloatPoint(10, 0));
  bloberizer.add(43, font1.get(), FloatPoint(15, 0));

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunFontData(bloberizer),
            font1.get());
  EXPECT_TRUE(
      ShapeResultBloberizerTestInfo::hasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::pendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 2ul);
    EXPECT_EQ(42, glyphs[0]);
    EXPECT_EQ(43, glyphs[1]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::pendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 4ul);
    EXPECT_EQ(10, offsets[0]);
    EXPECT_EQ(0, offsets[1]);
    EXPECT_EQ(15, offsets[2]);
    EXPECT_EQ(0, offsets[3]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::committedBlobCount(bloberizer), 0ul);

  // one more glyph, different font => pending run flush
  bloberizer.add(44, font2.get(), FloatPoint(12, 2));

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingRunFontData(bloberizer),
            font2.get());
  EXPECT_TRUE(
      ShapeResultBloberizerTestInfo::hasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::pendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 1ul);
    EXPECT_EQ(44, glyphs[0]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::pendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 2ul);
    EXPECT_EQ(12, offsets[0]);
    EXPECT_EQ(2, offsets[1]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::pendingBlobRunCount(bloberizer),
            1ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::committedBlobCount(bloberizer), 0ul);

  // flush everything (1 blob w/ 2 runs)
  EXPECT_EQ(bloberizer.blobs().size(), 1ul);
}

}  // namespace blink
