// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "platform/wtf/Optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Minimal TestSimpleFontData implementation.
// Font has no glyphs, but that's okay.
class TestSimpleFontData : public SimpleFontData {
 public:
  static PassRefPtr<TestSimpleFontData> Create(bool force_rotation = false) {
    FontPlatformData platform_data(
        SkTypeface::MakeDefault(), nullptr, 10, false, false,
        force_rotation ? FontOrientation::kVerticalUpright
                       : FontOrientation::kHorizontal);
    RefPtr<OpenTypeVerticalData> vertical_data(
        force_rotation ? OpenTypeVerticalData::Create(platform_data) : nullptr);
    return AdoptRef(
        new TestSimpleFontData(platform_data, std::move(vertical_data)));
  }

 private:
  TestSimpleFontData(const FontPlatformData& platform_data,
                     PassRefPtr<OpenTypeVerticalData> vertical_data)
      : SimpleFontData(platform_data, std::move(vertical_data)) {}
};

}  // anonymous namespace

TEST(ShapeResultBloberizerTest, StartsEmpty) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            nullptr);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer).size(),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer).size(),
            0ul);
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  EXPECT_TRUE(bloberizer.Blobs().IsEmpty());
}

TEST(ShapeResultBloberizerTest, StoresGlyphsOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  RefPtr<SimpleFontData> font1 = TestSimpleFontData::Create();
  RefPtr<SimpleFontData> font2 = TestSimpleFontData::Create();

  // 2 pending glyphs
  bloberizer.Add(42, font1.Get(), 10);
  bloberizer.Add(43, font1.Get(), 15);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font1.Get());
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 2ul);
    EXPECT_EQ(42, glyphs[0]);
    EXPECT_EQ(43, glyphs[1]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 2ul);
    EXPECT_EQ(10, offsets[0]);
    EXPECT_EQ(15, offsets[1]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // one more glyph, different font => pending run flush
  bloberizer.Add(44, font2.Get(), 12);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font2.Get());
  EXPECT_FALSE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 1ul);
    EXPECT_EQ(44, glyphs[0]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 1ul);
    EXPECT_EQ(12, offsets[0]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            1ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // flush everything (1 blob w/ 2 runs)
  EXPECT_EQ(bloberizer.Blobs().size(), 1ul);
}

TEST(ShapeResultBloberizerTest, StoresGlyphsVerticalOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  RefPtr<SimpleFontData> font1 = TestSimpleFontData::Create();
  RefPtr<SimpleFontData> font2 = TestSimpleFontData::Create();

  // 2 pending glyphs
  bloberizer.Add(42, font1.Get(), FloatPoint(10, 0));
  bloberizer.Add(43, font1.Get(), FloatPoint(15, 0));

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font1.Get());
  EXPECT_TRUE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 2ul);
    EXPECT_EQ(42, glyphs[0]);
    EXPECT_EQ(43, glyphs[1]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 4ul);
    EXPECT_EQ(10, offsets[0]);
    EXPECT_EQ(0, offsets[1]);
    EXPECT_EQ(15, offsets[2]);
    EXPECT_EQ(0, offsets[3]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            0ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // one more glyph, different font => pending run flush
  bloberizer.Add(44, font2.Get(), FloatPoint(12, 2));

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font2.Get());
  EXPECT_TRUE(
      ShapeResultBloberizerTestInfo::HasPendingRunVerticalOffsets(bloberizer));
  {
    const auto& glyphs =
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
    EXPECT_EQ(glyphs.size(), 1ul);
    EXPECT_EQ(44, glyphs[0]);

    const auto& offsets =
        ShapeResultBloberizerTestInfo::PendingRunOffsets(bloberizer);
    EXPECT_EQ(offsets.size(), 2ul);
    EXPECT_EQ(12, offsets[0]);
    EXPECT_EQ(2, offsets[1]);
  }

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer),
            1ul);
  EXPECT_EQ(ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer), 0ul);

  // flush everything (1 blob w/ 2 runs)
  EXPECT_EQ(bloberizer.Blobs().size(), 1ul);
}

TEST(ShapeResultBloberizerTest, MixedBlobRotation) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  // Normal (horizontal) font.
  RefPtr<SimpleFontData> font_normal = TestSimpleFontData::Create();
  ASSERT_FALSE(font_normal->PlatformData().IsVerticalAnyUpright());
  ASSERT_EQ(font_normal->VerticalData(), nullptr);

  // Rotated (vertical upright) font.
  RefPtr<SimpleFontData> font_rotated = TestSimpleFontData::Create(true);
  ASSERT_TRUE(font_rotated->PlatformData().IsVerticalAnyUpright());
  ASSERT_NE(font_rotated->VerticalData(), nullptr);

  struct {
    const SimpleFontData* font_data;
    size_t expected_pending_glyphs;
    size_t expected_pending_runs;
    size_t expected_committed_blobs;
  } append_ops[] = {
      // append 2 horizontal glyphs -> these go into the pending glyph buffer
      {font_normal.Get(), 1u, 0u, 0u},
      {font_normal.Get(), 2u, 0u, 0u},

      // append 3 vertical rotated glyphs -> push the prev pending (horizontal)
      // glyphs into a new run in the current (horizontal) blob
      {font_rotated.Get(), 1u, 1u, 0u},
      {font_rotated.Get(), 2u, 1u, 0u},
      {font_rotated.Get(), 3u, 1u, 0u},

      // append 2 more horizontal glyphs -> flush the current (horizontal) blob,
      // push prev (vertical) pending glyphs into new vertical blob run
      {font_normal.Get(), 1u, 1u, 1u},
      {font_normal.Get(), 2u, 1u, 1u},

      // append 1 more vertical glyph -> flush current (vertical) blob, push
      // prev (horizontal) pending glyphs into a new horizontal blob run
      {font_rotated.Get(), 1u, 1u, 2u},
  };

  for (const auto& op : append_ops) {
    bloberizer.Add(42, op.font_data, FloatPoint());
    EXPECT_EQ(
        op.expected_pending_glyphs,
        ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer).size());
    EXPECT_EQ(op.expected_pending_runs,
              ShapeResultBloberizerTestInfo::PendingBlobRunCount(bloberizer));
    EXPECT_EQ(op.expected_committed_blobs,
              ShapeResultBloberizerTestInfo::CommittedBlobCount(bloberizer));
  }

  // flush everything -> 4 blobs total
  EXPECT_EQ(4u, bloberizer.Blobs().size());
}

}  // namespace blink
