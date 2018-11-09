// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"

#include <unicode/uscript.h>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

class ShapeResultViewTest : public testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font = Font(font_description);
    font.Update(nullptr);
  }

  void TearDown() override {}

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font;
};

namespace {

struct ShapeResultViewGlyphInfo {
  unsigned character_index;
  Glyph glyph;
  float advance;
};

void AddGlyphInfo(void* context,
                  unsigned character_index,
                  Glyph glyph,
                  FloatSize glyph_offset,
                  float advance,
                  bool is_horizontal,
                  CanvasRotationInVertical rotation,
                  const SimpleFontData* font_data) {
  auto* list = static_cast<Vector<ShapeResultViewGlyphInfo>*>(context);
  ShapeResultViewGlyphInfo glyph_info = {character_index, glyph, advance};
  list->push_back(glyph_info);
}

bool CompareResultGlyphs(const Vector<ShapeResultViewGlyphInfo>& test,
                         const Vector<ShapeResultViewGlyphInfo>& reference,
                         unsigned reference_start,
                         unsigned num_glyphs) {
  float advance_offset = reference[reference_start].advance;
  bool glyphs_match = true;
  for (unsigned i = 0; i < test.size(); i++) {
    const auto& test_glyph = test[i];
    const auto& reference_glyph = reference[i + reference_start];
    if (test_glyph.character_index != reference_glyph.character_index ||
        test_glyph.glyph != reference_glyph.glyph ||
        test_glyph.advance != reference_glyph.advance - advance_offset) {
      glyphs_match = false;
      break;
    }
  }
  if (!glyphs_match) {
    fprintf(stderr, "╔══ Actual ═══════╤═══════╤═════════╗    ");
    fprintf(stderr, "╔══ Expected ═════╤═══════╤═════════╗\n");
    fprintf(stderr, "║ Character Index │ Glyph │ Advance ║    ");
    fprintf(stderr, "║ Character Index │ Glyph │ Advance ║\n");
    fprintf(stderr, "╟─────────────────┼───────┼─────────╢    ");
    fprintf(stderr, "╟─────────────────┼───────┼─────────╢\n");
    for (unsigned i = 0; i < test.size(); i++) {
      const auto& test_glyph = test[i];
      const auto& reference_glyph = reference[i + reference_start];

      if (test_glyph.character_index == reference_glyph.character_index)
        fprintf(stderr, "║      %10u │", test_glyph.character_index);
      else
        fprintf(stderr, "║▶     %10u◀│", test_glyph.character_index);

      if (test_glyph.glyph == reference_glyph.glyph)
        fprintf(stderr, "  %04X │", test_glyph.glyph);
      else
        fprintf(stderr, "▶ %04X◀│", test_glyph.glyph);

      if (test_glyph.advance == reference_glyph.advance)
        fprintf(stderr, " %7.2f ║    ", test_glyph.advance);
      else
        fprintf(stderr, "▶%7.2f◀║    ", test_glyph.advance);

      fprintf(stderr, "║      %10u │  %04X │ %7.2f ║\n",
              reference_glyph.character_index, reference_glyph.glyph,
              reference_glyph.advance - advance_offset);
    }
    fprintf(stderr, "╚═════════════════╧═══════╧═════════╝    ");
    fprintf(stderr, "╚═════════════════╧═══════╧═════════╝\n");
  }
  return glyphs_match;
}

}  // anonymous namespace

TEST_F(ShapeResultViewTest, LatinSingleView) {
  String string =
      To16Bit("Test run with multiple words and breaking opportunities.", 56);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  // Test view at the start of the result: "Test run with multiple"
  ShapeResultView::Segment segment = {result.get(), 0, 22};
  auto first4 = ShapeResultView::Create(&segment, 1);

  EXPECT_EQ(first4->StartIndex(), 0u);
  EXPECT_EQ(first4->NumCharacters(), 22u);
  EXPECT_EQ(first4->NumGlyphs(), 22u);

  Vector<ShapeResultViewGlyphInfo> first4_glyphs;
  first4->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&first4_glyphs));
  EXPECT_EQ(first4_glyphs.size(), 22u);
  EXPECT_TRUE(CompareResultGlyphs(first4_glyphs, glyphs, 0u, 22u));

  // Test view in the middle of the result: "multiple words and breaking"
  segment = {result.get(), 14, 41};
  auto middle4 = ShapeResultView::Create(&segment, 1);

  EXPECT_EQ(middle4->StartIndex(), 14u);
  EXPECT_EQ(middle4->NumCharacters(), 27u);
  EXPECT_EQ(middle4->NumGlyphs(), 27u);

  Vector<ShapeResultViewGlyphInfo> middle4_glyphs;
  middle4->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&middle4_glyphs));
  EXPECT_EQ(middle4_glyphs.size(), 27u);
  EXPECT_TRUE(CompareResultGlyphs(middle4_glyphs, glyphs, 14u, 27u));

  // Test view at the end of the result: "breaking opportunities."
  segment = {result.get(), 33, 56};
  auto last2 = ShapeResultView::Create(&segment, 1);

  EXPECT_EQ(last2->StartIndex(), 33u);
  EXPECT_EQ(last2->NumCharacters(), 23u);
  EXPECT_EQ(last2->NumGlyphs(), 23u);

  Vector<ShapeResultViewGlyphInfo> last2_glyphs;
  last2->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&last2_glyphs));
  EXPECT_EQ(last2_glyphs.size(), 23u);
  EXPECT_TRUE(CompareResultGlyphs(last2_glyphs, glyphs, 33u, 23u));
}

TEST_F(ShapeResultViewTest, ArabicSingleView) {
  String string = To16Bit("عربى نص", 7);
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  // Test view at the start of the result: "عربى"
  ShapeResultView::Segment segment = {result.get(), 0, 4};
  auto first_word = ShapeResultView::Create(&segment, 1);
  Vector<ShapeResultViewGlyphInfo> first_glyphs;
  first_word->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&first_glyphs));

  EXPECT_EQ(first_word->StartIndex(), 0u);
  EXPECT_EQ(first_word->NumCharacters(), 4u);
  EXPECT_EQ(first_word->NumGlyphs(), 4u);
  EXPECT_EQ(first_glyphs.size(), 4u);

  String first_reference_string = To16Bit("عربى", 4);
  HarfBuzzShaper first_reference_shaper(first_reference_string);
  scoped_refptr<const ShapeResult> first_wortd_reference =
      first_reference_shaper.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> first_reference_glyphs;
  first_wortd_reference->ForEachGlyph(
      0, AddGlyphInfo, static_cast<void*>(&first_reference_glyphs));
  EXPECT_EQ(first_reference_glyphs.size(), 4u);

  EXPECT_TRUE(
      CompareResultGlyphs(first_glyphs, first_reference_glyphs, 0u, 4u));
  EXPECT_TRUE(CompareResultGlyphs(first_glyphs, glyphs, 3u, 7u));

  // Test view at the end of the result: "نص"
  segment = {result.get(), 4, 7};
  auto last_word = ShapeResultView::Create(&segment, 1);
  Vector<ShapeResultViewGlyphInfo> last_glyphs;
  last_word->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&last_glyphs));

  EXPECT_EQ(last_word->StartIndex(), 4u);
  EXPECT_EQ(last_word->NumCharacters(), 3u);
  EXPECT_EQ(last_word->NumGlyphs(), 3u);
  EXPECT_EQ(last_glyphs.size(), 3u);
}

TEST_F(ShapeResultViewTest, LatinMultiRun) {
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper_a(To16Bit("hello", 5));
  HarfBuzzShaper shaper_b(To16Bit(" w", 2));
  HarfBuzzShaper shaper_c(To16Bit("orld", 4));
  HarfBuzzShaper shaper_d(To16Bit("!", 1));

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs: "hello world!"
  scoped_refptr<ShapeResult> result = ShapeResult::Create(&font, 0, direction);
  shaper_a.Shape(&font, direction)->CopyRange(0u, 5u, result.get());
  shaper_b.Shape(&font, direction)->CopyRange(0u, 2u, result.get());
  shaper_c.Shape(&font, direction)->CopyRange(0u, 4u, result.get());
  shaper_d.Shape(&font, direction)->CopyRange(0u, 1u, result.get());

  Vector<ShapeResultViewGlyphInfo> result_glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&result_glyphs));

  // Create composite view out of multiple segments where at least some of the
  // segments have multiple runs: "hello wood wold!"
  ShapeResultView::Segment segments[5] = {
      {result.get(), 0, 8},    // "hello wo"
      {result.get(), 7, 8},    // "o"
      {result.get(), 10, 11},  // "d"
      {result.get(), 5, 8},    // " wo"
      {result.get(), 9, 12},   // "ld!"
  };
  auto composite_view = ShapeResultView::Create(&segments[0], 5);
  Vector<ShapeResultViewGlyphInfo> view_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&view_glyphs));

  EXPECT_EQ(composite_view->StartIndex(), 0u);
  EXPECT_EQ(composite_view->NumCharacters(), 16u);
  EXPECT_EQ(composite_view->NumGlyphs(), 16u);
  EXPECT_EQ(view_glyphs.size(), 16u);

  HarfBuzzShaper shaper2(To16Bit("hello world!", 12));
  scoped_refptr<const ShapeResult> result2 = shaper2.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> glyphs2;
  result2->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs2));
  EXPECT_TRUE(CompareResultGlyphs(result_glyphs, glyphs2, 0u, 12u));

  HarfBuzzShaper reference_shaper(To16Bit("hello wood wold!", 16));
  scoped_refptr<const ShapeResult> reference_result =
      reference_shaper.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> reference_glyphs;
  reference_result->ForEachGlyph(0, AddGlyphInfo,
                                 static_cast<void*>(&reference_glyphs));

  scoped_refptr<ShapeResult> composite_copy =
      ShapeResult::Create(&font, 0, direction);
  result->CopyRange(0, 8, composite_copy.get());
  result->CopyRange(7, 8, composite_copy.get());
  result->CopyRange(10, 11, composite_copy.get());
  result->CopyRange(5, 8, composite_copy.get());
  result->CopyRange(9, 12, composite_copy.get());

  Vector<ShapeResultViewGlyphInfo> composite_copy_glyphs;
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_copy_glyphs));

  EXPECT_TRUE(CompareResultGlyphs(view_glyphs, reference_glyphs, 0u, 16u));
  EXPECT_TRUE(
      CompareResultGlyphs(composite_copy_glyphs, reference_glyphs, 0u, 16u));
}

TEST_F(ShapeResultViewTest, LatinCompositeView) {
  String string =
      To16Bit("Test run with multiple words and breaking opportunities.", 56);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  String reference_string = To16Bit("multiple breaking opportunities Test", 36);
  HarfBuzzShaper reference_shaper(reference_string);
  scoped_refptr<const ShapeResult> reference_result =
      reference_shaper.Shape(&font, direction);
  Vector<ShapeResultViewGlyphInfo> reference_glyphs;

  // Match the character index logic of ShapeResult::CopyRange where the the
  // character index of the first result is preserved and all subsequent ones
  // are adjusted to be sequential.
  // TODO(layout-dev): Arguably both should be updated to renumber the first
  // result as well but some callers depend on the existing behavior.
  scoped_refptr<ShapeResult> composite_copy =
      ShapeResult::Create(&font, 0, direction);
  result->CopyRange(14, 23, composite_copy.get());
  result->CopyRange(33, 55, composite_copy.get());
  result->CopyRange(4, 5, composite_copy.get());
  result->CopyRange(0, 4, composite_copy.get());
  EXPECT_EQ(composite_copy->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_copy->NumGlyphs(), reference_result->NumGlyphs());
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&reference_glyphs));

  // Create composite view out of multiple segments:
  ShapeResultView::Segment segments[4] = {
      {result.get(), 14, 23},  // "multiple "
      {result.get(), 33, 55},  // "breaking opportunities"
      {result.get(), 4, 5},    // " "
      {result.get(), 0, 4}     // "Test"
  };
  auto composite_view = ShapeResultView::Create(&segments[0], 4);

  EXPECT_EQ(composite_view->StartIndex(), composite_copy->StartIndex());
  EXPECT_EQ(composite_view->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_view->NumGlyphs(), reference_result->NumGlyphs());

  Vector<ShapeResultViewGlyphInfo> composite_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_glyphs));
  EXPECT_EQ(composite_glyphs.size(), 36u);
  EXPECT_TRUE(CompareResultGlyphs(composite_glyphs, reference_glyphs, 0u, 22u));
}

TEST_F(ShapeResultViewTest, MixedScriptsCompositeView) {
  String string_a = To16Bit("Test with multiple 字体 ", 22);
  String string_b = To16Bit("and 本書.", 7);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper_a(string_a);
  scoped_refptr<const ShapeResult> result_a = shaper_a.Shape(&font, direction);
  HarfBuzzShaper shaper_b(string_b);
  scoped_refptr<const ShapeResult> result_b = shaper_b.Shape(&font, direction);

  String reference_string = To16Bit("Test with multiple 字体 and 本書.", 29);
  HarfBuzzShaper reference_shaper(reference_string);
  scoped_refptr<const ShapeResult> reference_result =
      reference_shaper.Shape(&font, direction);

  // Create a copy using CopyRange and compare with that to ensure that the same
  // fonts are used for both the composite and the reference. The combined
  // reference_result data might use different fonts, resulting in different
  // glyph ids and metrics.
  scoped_refptr<ShapeResult> composite_copy =
      ShapeResult::Create(&font, 0, direction);
  result_a->CopyRange(0, 22, composite_copy.get());
  result_b->CopyRange(0, 7, composite_copy.get());
  EXPECT_EQ(composite_copy->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_copy->NumGlyphs(), reference_result->NumGlyphs());
  Vector<ShapeResultViewGlyphInfo> reference_glyphs;
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&reference_glyphs));

  ShapeResultView::Segment segments[4] = {{result_a.get(), 0, 22},
                                          {result_b.get(), 0, 7}};
  auto composite_view = ShapeResultView::Create(&segments[0], 2);

  EXPECT_EQ(composite_view->StartIndex(), 0u);
  EXPECT_EQ(composite_view->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_view->NumGlyphs(), reference_result->NumGlyphs());

  Vector<ShapeResultViewGlyphInfo> composite_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_glyphs));
  EXPECT_TRUE(CompareResultGlyphs(composite_glyphs, reference_glyphs, 0u,
                                  reference_glyphs.size()));
}

}  // namespace blink
