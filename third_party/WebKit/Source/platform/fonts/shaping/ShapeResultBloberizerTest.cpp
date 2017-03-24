// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Optional.h"

namespace blink {

namespace {

// Minimal TestSimpleFontData implementation.
// Font has no glyphs, but that's okay.
class TestSimpleFontData : public SimpleFontData {
 public:
  static PassRefPtr<TestSimpleFontData> create(bool forceRotation = false) {
    FontPlatformData platformData(
        SkTypeface::MakeDefault(), nullptr, 10, false, false,
        forceRotation ? FontOrientation::VerticalUpright
                      : FontOrientation::Horizontal);
    RefPtr<OpenTypeVerticalData> verticalData(
        forceRotation ? OpenTypeVerticalData::create(platformData) : nullptr);
    return adoptRef(
        new TestSimpleFontData(platformData, std::move(verticalData)));
  }

 private:
  TestSimpleFontData(const FontPlatformData& platformData,
                     PassRefPtr<OpenTypeVerticalData> verticalData)
      : SimpleFontData(platformData, std::move(verticalData)) {}
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

TEST(ShapeResultBloberizerTest, MixedBlobRotation) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  // Normal (horizontal) font.
  RefPtr<SimpleFontData> fontNormal = TestSimpleFontData::create();
  ASSERT_FALSE(fontNormal->platformData().isVerticalAnyUpright());
  ASSERT_EQ(fontNormal->verticalData(), nullptr);

  // Rotated (vertical upright) font.
  RefPtr<SimpleFontData> fontRotated = TestSimpleFontData::create(true);
  ASSERT_TRUE(fontRotated->platformData().isVerticalAnyUpright());
  ASSERT_NE(fontRotated->verticalData(), nullptr);

  struct {
    const SimpleFontData* fontData;
    size_t expectedPendingGlyphs;
    size_t expectedPendingRuns;
    size_t expectedCommittedBlobs;
  } appendOps[] = {
      // append 2 horizontal glyphs -> these go into the pending glyph buffer
      {fontNormal.get(), 1u, 0u, 0u},
      {fontNormal.get(), 2u, 0u, 0u},

      // append 3 vertical rotated glyphs -> push the prev pending (horizontal)
      // glyphs into a new run in the current (horizontal) blob
      {fontRotated.get(), 1u, 1u, 0u},
      {fontRotated.get(), 2u, 1u, 0u},
      {fontRotated.get(), 3u, 1u, 0u},

      // append 2 more horizontal glyphs -> flush the current (horizontal) blob,
      // push prev (vertical) pending glyphs into new vertical blob run
      {fontNormal.get(), 1u, 1u, 1u},
      {fontNormal.get(), 2u, 1u, 1u},

      // append 1 more vertical glyph -> flush current (vertical) blob, push
      // prev (horizontal) pending glyphs into a new horizontal blob run
      {fontRotated.get(), 1u, 1u, 2u},
  };

  for (const auto& op : appendOps) {
    bloberizer.add(42, op.fontData, FloatPoint());
    EXPECT_EQ(
        op.expectedPendingGlyphs,
        ShapeResultBloberizerTestInfo::pendingRunGlyphs(bloberizer).size());
    EXPECT_EQ(op.expectedPendingRuns,
              ShapeResultBloberizerTestInfo::pendingBlobRunCount(bloberizer));
    EXPECT_EQ(op.expectedCommittedBlobs,
              ShapeResultBloberizerTestInfo::committedBlobCount(bloberizer));
  }

  // flush everything -> 4 blobs total
  EXPECT_EQ(4u, bloberizer.blobs().size());
}

}  // namespace blink
