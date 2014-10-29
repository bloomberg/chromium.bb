// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_HARFBUZZ_H_
#define UI_GFX_RENDER_TEXT_HARFBUZZ_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "third_party/harfbuzz-ng/src/hb.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/gfx/render_text.h"

namespace base {
namespace i18n {
class BreakIterator;
}
}

namespace gfx {

namespace internal {

struct GFX_EXPORT TextRunHarfBuzz {
  TextRunHarfBuzz();
  ~TextRunHarfBuzz();

  // Returns the index of the first glyph that corresponds to the character at
  // |pos|.
  size_t CharToGlyph(size_t pos) const;

  // Returns the corresponding glyph range of the given character range.
  // |range| is in text-space (0 corresponds to |GetLayoutText()[0]|). Returned
  // value is in run-space (0 corresponds to the first glyph in the run).
  Range CharRangeToGlyphRange(const Range& range) const;

  // Returns the number of missing glyphs in the shaped text run.
  size_t CountMissingGlyphs() const;

  // Writes the character and glyph ranges of the cluster containing |pos|.
  void GetClusterAt(size_t pos, Range* chars, Range* glyphs) const;

  // Returns the grapheme bounds at |text_index|. Handles multi-grapheme glyphs.
  Range GetGraphemeBounds(base::i18n::BreakIterator* grapheme_iterator,
                          size_t text_index);

  // Returns whether the given shaped run contains any missing glyphs.
  bool HasMissingGlyphs() const;

  float width;
  float preceding_run_widths;
  Range range;
  bool is_rtl;
  UBiDiLevel level;
  UScriptCode script;

  scoped_ptr<uint16[]> glyphs;
  scoped_ptr<SkPoint[]> positions;
  std::vector<uint32> glyph_to_char;
  size_t glyph_count;

  std::string family;
  skia::RefPtr<SkTypeface> skia_face;
  FontRenderParams render_params;
  int font_size;
  int font_style;
  bool strike;
  bool diagonal_strike;
  bool underline;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextRunHarfBuzz);
};

}  // namespace internal

class GFX_EXPORT RenderTextHarfBuzz : public RenderText {
 public:
  RenderTextHarfBuzz();
  ~RenderTextHarfBuzz() override;

  // Overridden from RenderText.
  Size GetStringSize() override;
  SizeF GetStringSizeF() override;
  SelectionModel FindCursorPosition(const Point& point) override;
  std::vector<FontSpan> GetFontSpansForTesting() override;
  Range GetGlyphBounds(size_t index) override;

 protected:
  // Overridden from RenderText.
  int GetLayoutTextBaseline() override;
  SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  std::vector<Rect> GetSubstringBounds(const Range& range) override;
  size_t TextIndexToLayoutIndex(size_t index) const override;
  size_t LayoutIndexToTextIndex(size_t index) const override;
  bool IsValidCursorIndex(size_t index) override;
  void ResetLayout() override;
  void EnsureLayout() override;
  void DrawVisualText(Canvas* canvas) override;

 private:
  friend class RenderTextTest;
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_RunDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_BreakRunsByUnicodeBlocks);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_SubglyphGraphemeCases);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_SubglyphGraphemePartition);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_NonExistentFont);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_UniscribeFallback);

  // Return the run index that contains the argument; or the length of the
  // |runs_| vector if argument exceeds the text length or width.
  size_t GetRunContainingCaret(const SelectionModel& caret) const;
  size_t GetRunContainingXCoord(int x, int* offset) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);
  SelectionModel LastSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);

  // Break the text into logical runs and populate the visual <-> logical maps.
  void ItemizeText();

  // Helper method for ShapeRun() that calls ShapeRunWithFont() with |run|,
  // |family|, and |render_params|, returning true if the family provides all
  // needed glyphs and false otherwise. Additionally updates |best_family|,
  // |best_render_params|, and |best_missing_glyphs| if |family| has fewer than
  // |best_missing_glyphs| missing glyphs.
  bool CompareFamily(internal::TextRunHarfBuzz* run,
                     const std::string& family,
                     const gfx::FontRenderParams& render_params,
                     std::string* best_family,
                     gfx::FontRenderParams* best_render_params,
                     size_t* best_missing_glyphs);

  // Shape the glyphs needed for the text |run|.
  void ShapeRun(internal::TextRunHarfBuzz* run);
  bool ShapeRunWithFont(internal::TextRunHarfBuzz* run,
                        const std::string& font_family,
                        const FontRenderParams& params);

  // Text runs in logical order.
  ScopedVector<internal::TextRunHarfBuzz> runs_;

  // Maps visual run indices to logical run indices and vice versa.
  std::vector<int32_t> visual_to_logical_;
  std::vector<int32_t> logical_to_visual_;

  bool needs_layout_;

  // ICU grapheme iterator for the layout text. Valid when |!needs_layout_|. Can
  // be NULL in case of an error.
  scoped_ptr<base::i18n::BreakIterator> grapheme_iterator_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextHarfBuzz);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_HARFBUZZ_H_
