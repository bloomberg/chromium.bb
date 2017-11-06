// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include <memory>
#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "platform/wtf/Optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Minimal TestSimpleFontData implementation.
// Font has no glyphs, but that's okay.
class TestSimpleFontData : public SimpleFontData {
 public:
  static scoped_refptr<TestSimpleFontData> Create(bool force_rotation = false) {
    FontPlatformData platform_data(
        SkTypeface::MakeDefault(), nullptr, 10, false, false,
        force_rotation ? FontOrientation::kVerticalUpright
                       : FontOrientation::kHorizontal);
    scoped_refptr<OpenTypeVerticalData> vertical_data(
        force_rotation ? OpenTypeVerticalData::Create(platform_data) : nullptr);
    return WTF::AdoptRef(
        new TestSimpleFontData(platform_data, std::move(vertical_data)));
  }

 private:
  TestSimpleFontData(const FontPlatformData& platform_data,
                     scoped_refptr<OpenTypeVerticalData> vertical_data)
      : SimpleFontData(platform_data, std::move(vertical_data)) {}
};

class ShapeResultBloberizerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font_description.SetLocale(LayoutLocale::Get("en"));
    ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
    font_description.SetGenericFamily(FontDescription::kStandardFamily);

    font = Font(font_description);
    font.Update(nullptr);
    ASSERT_TRUE(font.CanShapeWordByWord());
    fallback_fonts = nullptr;
    cache = std::make_unique<ShapeCache>();
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font;
  std::unique_ptr<ShapeCache> cache;
  HashSet<const SimpleFontData*>* fallback_fonts;
  unsigned start_index = 0;
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

}  // anonymous namespace

TEST_F(ShapeResultBloberizerTest, StartsEmpty) {
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

TEST_F(ShapeResultBloberizerTest, StoresGlyphsOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  scoped_refptr<SimpleFontData> font1 = TestSimpleFontData::Create();
  scoped_refptr<SimpleFontData> font2 = TestSimpleFontData::Create();

  // 2 pending glyphs
  bloberizer.Add(42, font1.get(), 10);
  bloberizer.Add(43, font1.get(), 15);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font1.get());
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
  bloberizer.Add(44, font2.get(), 12);

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font2.get());
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

TEST_F(ShapeResultBloberizerTest, StoresGlyphsVerticalOffsets) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  scoped_refptr<SimpleFontData> font1 = TestSimpleFontData::Create();
  scoped_refptr<SimpleFontData> font2 = TestSimpleFontData::Create();

  // 2 pending glyphs
  bloberizer.Add(42, font1.get(), FloatPoint(10, 0));
  bloberizer.Add(43, font1.get(), FloatPoint(15, 0));

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font1.get());
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
  bloberizer.Add(44, font2.get(), FloatPoint(12, 2));

  EXPECT_EQ(ShapeResultBloberizerTestInfo::PendingRunFontData(bloberizer),
            font2.get());
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

TEST_F(ShapeResultBloberizerTest, MixedBlobRotation) {
  Font font;
  ShapeResultBloberizer bloberizer(font, 1);

  // Normal (horizontal) font.
  scoped_refptr<SimpleFontData> font_normal = TestSimpleFontData::Create();
  ASSERT_FALSE(font_normal->PlatformData().IsVerticalAnyUpright());
  ASSERT_EQ(font_normal->VerticalData(), nullptr);

  // Rotated (vertical upright) font.
  scoped_refptr<SimpleFontData> font_rotated = TestSimpleFontData::Create(true);
  ASSERT_TRUE(font_rotated->PlatformData().IsVerticalAnyUpright());
  ASSERT_NE(font_rotated->VerticalData(), nullptr);

  struct {
    const SimpleFontData* font_data;
    size_t expected_pending_glyphs;
    size_t expected_pending_runs;
    size_t expected_committed_blobs;
  } append_ops[] = {
      // append 2 horizontal glyphs -> these go into the pending glyph buffer
      {font_normal.get(), 1u, 0u, 0u},
      {font_normal.get(), 2u, 0u, 0u},

      // append 3 vertical rotated glyphs -> push the prev pending (horizontal)
      // glyphs into a new run in the current (horizontal) blob
      {font_rotated.get(), 1u, 1u, 0u},
      {font_rotated.get(), 2u, 1u, 0u},
      {font_rotated.get(), 3u, 1u, 0u},

      // append 2 more horizontal glyphs -> flush the current (horizontal) blob,
      // push prev (vertical) pending glyphs into new vertical blob run
      {font_normal.get(), 1u, 1u, 1u},
      {font_normal.get(), 2u, 1u, 1u},

      // append 1 more vertical glyph -> flush current (vertical) blob, push
      // prev (horizontal) pending glyphs into a new horizontal blob run
      {font_rotated.get(), 1u, 1u, 2u},
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

// Tests that filling a glyph buffer for a specific range returns the same
// results when shaping word by word as when shaping the full run in one go.
TEST_F(ShapeResultBloberizerTest, CommonAccentLeftToRightFillGlyphBuffer) {
  // "/. ." with an accent mark over the first dot.
  const UChar kStr[] = {0x2F, 0x301, 0x2E, 0x20, 0x2E, 0x0};
  TextRun text_run(kStr, 5);
  TextRunPaintInfo run_info(text_run);
  run_info.to = 3;

  ShapeResultBloberizer bloberizer(font, 1);
  CachingWordShaper word_shaper(font);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  bloberizer.FillGlyphs(run_info, buffer);

  Font reference_font(font_description);
  reference_font.Update(nullptr);
  reference_font.SetCanShapeWordByWordForTesting(false);

  ShapeResultBloberizer reference_bloberizer(reference_font, 1);
  CachingWordShaper reference_word_shaper(font);
  ShapeResultBuffer reference_buffer;
  reference_word_shaper.FillResultBuffer(run_info, &reference_buffer);
  reference_bloberizer.FillGlyphs(run_info, reference_buffer);

  const auto& glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
  ASSERT_EQ(glyphs.size(), 3ul);
  const auto reference_glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(reference_bloberizer);
  ASSERT_EQ(reference_glyphs.size(), 3ul);

  EXPECT_EQ(reference_glyphs[0], glyphs[0]);
  EXPECT_EQ(reference_glyphs[1], glyphs[1]);
  EXPECT_EQ(reference_glyphs[2], glyphs[2]);
}

// Tests that filling a glyph buffer for a specific range returns the same
// results when shaping word by word as when shaping the full run in one go.
TEST_F(ShapeResultBloberizerTest, CommonAccentRightToLeftFillGlyphBuffer) {
  // "[] []" with an accent mark over the last square bracket.
  const UChar kStr[] = {0x5B, 0x5D, 0x20, 0x5B, 0x301, 0x5D, 0x0};
  TextRun text_run(kStr, 6);
  text_run.SetDirection(TextDirection::kRtl);
  TextRunPaintInfo run_info(text_run);
  run_info.from = 1;

  ShapeResultBloberizer bloberizer(font, 1);
  CachingWordShaper word_shaper(font);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  bloberizer.FillGlyphs(run_info, buffer);

  Font reference_font(font_description);
  reference_font.Update(nullptr);
  reference_font.SetCanShapeWordByWordForTesting(false);

  ShapeResultBloberizer reference_bloberizer(reference_font, 1);
  CachingWordShaper reference_word_shaper(font);
  ShapeResultBuffer reference_buffer;
  reference_word_shaper.FillResultBuffer(run_info, &reference_buffer);
  reference_bloberizer.FillGlyphs(run_info, reference_buffer);

  const auto& glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(bloberizer);
  ASSERT_EQ(5u, glyphs.size());
  const auto reference_glyphs =
      ShapeResultBloberizerTestInfo::PendingRunGlyphs(reference_bloberizer);
  ASSERT_EQ(5u, reference_glyphs.size());

  EXPECT_EQ(reference_glyphs[0], glyphs[0]);
  EXPECT_EQ(reference_glyphs[1], glyphs[1]);
  EXPECT_EQ(reference_glyphs[2], glyphs[2]);
  EXPECT_EQ(reference_glyphs[3], glyphs[3]);
  EXPECT_EQ(reference_glyphs[4], glyphs[4]);
}

// Tests that runs with zero glyphs (the ZWJ non-printable character in this
// case) are handled correctly. This test passes if it does not cause a crash.
TEST_F(ShapeResultBloberizerTest, SubRunWithZeroGlyphs) {
  // "Foo &zwnj; bar"
  const UChar kStr[] = {0x46, 0x6F, 0x6F, 0x20, 0x200C,
                        0x20, 0x62, 0x61, 0x71, 0x0};
  TextRun text_run(kStr, 9);

  CachingWordShaper shaper(font);
  FloatRect glyph_bounds;
  ASSERT_GT(shaper.Width(text_run, nullptr, &glyph_bounds), 0);

  ShapeResultBloberizer bloberizer(font, 1);
  TextRunPaintInfo run_info(text_run);
  run_info.to = 8;

  CachingWordShaper word_shaper(font);
  ShapeResultBuffer buffer;
  word_shaper.FillResultBuffer(run_info, &buffer);
  bloberizer.FillGlyphs(run_info, buffer);

  shaper.GetCharacterRange(text_run, 0, 8);
}

}  // namespace blink
