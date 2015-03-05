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

class Range;
class RangeF;

namespace internal {

// Applies std::round to the start and end values of the given RangeF.
GFX_EXPORT Range RoundRangeF(const RangeF& range_f);

struct GFX_EXPORT TextRunHarfBuzz {
  TextRunHarfBuzz();
  ~TextRunHarfBuzz();

  // Returns the index of the first glyph that corresponds to the character at
  // |pos|.
  size_t CharToGlyph(size_t pos) const;

  // Returns the corresponding glyph range of the given character range.
  // |range| is in text-space (0 corresponds to |GetDisplayText()[0]|). Returned
  // value is in run-space (0 corresponds to the first glyph in the run).
  Range CharRangeToGlyphRange(const Range& range) const;

  // Returns the number of missing glyphs in the shaped text run.
  size_t CountMissingGlyphs() const;

  // Writes the character and glyph ranges of the cluster containing |pos|.
  void GetClusterAt(size_t pos, Range* chars, Range* glyphs) const;

  // Returns the grapheme bounds at |text_index|. Handles multi-grapheme glyphs.
  RangeF GetGraphemeBounds(base::i18n::BreakIterator* grapheme_iterator,
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

// Manages the list of TextRunHarfBuzz and its logical <-> visual index mapping.
class TextRunList {
 public:
  TextRunList();
  ~TextRunList();

  size_t size() const { return runs_.size(); }

  // Converts the index between logical and visual index.
  size_t visual_to_logical(size_t index) const {
    return visual_to_logical_[index];
  }
  size_t logical_to_visual(size_t index) const {
    return logical_to_visual_[index];
  }

  const ScopedVector<TextRunHarfBuzz>& runs() const { return runs_; }

  // Adds the new |run| to the run list.
  void add(TextRunHarfBuzz* run) { runs_.push_back(run); }

  // Reset the run list.
  void Reset();

  // Initialize the index mapping.
  void InitIndexMap();

  // Precomputes the offsets for all runs.
  void ComputePrecedingRunWidths();

  // Get the total width of runs, as if they were shown on one line.
  // Do not use this when multiline is enabled.
  float width() const { return width_; }

 private:
  // Text runs in logical order.
  ScopedVector<TextRunHarfBuzz> runs_;

  // Maps visual run indices to logical run indices and vice versa.
  std::vector<int32_t> visual_to_logical_;
  std::vector<int32_t> logical_to_visual_;

  float width_;

  DISALLOW_COPY_AND_ASSIGN(TextRunList);
};

}  // namespace internal

class GFX_EXPORT RenderTextHarfBuzz : public RenderText {
 public:
  RenderTextHarfBuzz();
  ~RenderTextHarfBuzz() override;

  // RenderText:
  scoped_ptr<RenderText> CreateInstanceOfSameType() const override;
  const base::string16& GetDisplayText() override;
  Size GetStringSize() override;
  SizeF GetStringSizeF() override;
  SelectionModel FindCursorPosition(const Point& point) override;
  std::vector<FontSpan> GetFontSpansForTesting() override;
  Range GetGlyphBounds(size_t index) override;

 protected:
  // RenderText:
  int GetDisplayTextBaseline() override;
  SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) override;
  std::vector<Rect> GetSubstringBounds(const Range& range) override;
  size_t TextIndexToDisplayIndex(size_t index) override;
  size_t DisplayIndexToTextIndex(size_t index) override;
  bool IsValidCursorIndex(size_t index) override;
  void OnLayoutTextAttributeChanged(bool text_changed) override;
  void OnDisplayTextAttributeChanged() override;
  void EnsureLayout() override;
  void DrawVisualText(Canvas* canvas) override;

 private:
  friend class RenderTextTest;
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_HorizontalAlignment);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_NormalWidth);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_RunDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_HorizontalPositions);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest,
                           HarfBuzz_TextPositionWithFractionalSize);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_BreakRunsByUnicodeBlocks);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_BreakRunsByEmoji);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_SubglyphGraphemeCases);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_SubglyphGraphemePartition);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_NonExistentFont);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_UniscribeFallback);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_UnicodeFallback);

  // Specify the width of a glyph for test. The width of glyphs is very
  // platform-dependent and environment-dependent. Otherwise multiline test
  // will become really flaky.
  void set_glyph_width_for_test(float test_width) {
    glyph_width_for_test_ = test_width;
  }

  // The actual implementation of the text drawing.
  void DrawVisualTextInternal(internal::SkiaTextRenderer* renderer);

  // Return the run index that contains the argument; or the length of the
  // |runs_| vector if argument exceeds the text length or width.
  size_t GetRunContainingCaret(const SelectionModel& caret);
  size_t GetRunContainingXCoord(float x, float* offset) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);
  SelectionModel LastSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);

  // Break the text into logical runs and populate the visual <-> logical maps
  // into |run_list_out|.
  void ItemizeTextToRuns(const base::string16& string,
                         internal::TextRunList* run_list_out);

  // Helper method for ShapeRun() that calls ShapeRunWithFont() with |text|,
  // |run|, |family|, and |render_params|, returning true if the family provides
  // all the glyphs needed for |run|, and false otherwise. Additionally updates
  // |best_family|, |best_render_params|, and |best_missing_glyphs| if |family|
  // has fewer than |best_missing_glyphs| missing glyphs.
  bool CompareFamily(const base::string16& text,
                     const std::string& family,
                     const gfx::FontRenderParams& render_params,
                     internal::TextRunHarfBuzz* run,
                     std::string* best_family,
                     gfx::FontRenderParams* best_render_params,
                     size_t* best_missing_glyphs);

  // Shape the glyphs of all runs in |run_list| using |text|.
  void ShapeRunList(const base::string16& text,
                    internal::TextRunList* run_list);

  // Shape the glyphs needed for the |run| within the |text|.
  void ShapeRun(const base::string16& text,
                internal::TextRunHarfBuzz* run);
  bool ShapeRunWithFont(const base::string16& text,
                        const std::string& font_family,
                        const FontRenderParams& params,
                        internal::TextRunHarfBuzz* run);

  // Makes sure that text runs for layout text are shaped.
  void EnsureLayoutRunList();

  // ICU grapheme iterator for the layout text. Can be NULL in case of an error.
  base::i18n::BreakIterator* GetGraphemeIterator();

  // Convert an index in |text_| to the index in |given_text|. The
  // |given_text| should be either |display_text_| or |layout_text_|
  // depending on the elide state.
  size_t TextIndexToGivenTextIndex(const base::string16& given_text,
                                   size_t index);

  // Returns the current run list, |display_run_list_| if the text is
  // elided, or |layout_run_list_| otherwise.
  internal::TextRunList* GetRunList();
  const internal::TextRunList* GetRunList() const;

  // Text run list for |layout_text_| and |display_text_|.
  // |display_run_list_| is created only when the text is elided.
  internal::TextRunList layout_run_list_;
  scoped_ptr<internal::TextRunList> display_run_list_;

  bool update_layout_run_list_ : 1;
  bool update_display_run_list_ : 1;
  bool update_grapheme_iterator_ : 1;
  bool update_display_text_ : 1;

  // ICU grapheme iterator for the layout text. Use GetGraphemeIterator()
  // to access the iterator.
  scoped_ptr<base::i18n::BreakIterator> grapheme_iterator_;

  // The total size of the layouted text.
  SizeF total_size_;

  // Fixed width of glyphs. This should only be set in test environments.
  float glyph_width_for_test_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextHarfBuzz);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_HARFBUZZ_H_
