// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_WIN_H_
#define UI_GFX_RENDER_TEXT_WIN_H_
#pragma once

#include <usp10.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/render_text.h"

namespace gfx {

namespace internal {

struct TextRun {
  TextRun();
  ~TextRun();

  ui::Range range;
  Font font;
  // TODO(msw): Disambiguate color, strike, etc. from TextRuns.
  //            Otherwise, this breaks the glyph shaping process.
  //            See the example at: http://www.catch22.net/tuts/neatpad/12.
  SkColor foreground;
  bool strike;
  bool underline;

  int width;
  // The cumulative widths of preceding runs.
  int preceding_run_widths;

  SCRIPT_ANALYSIS script_analysis;

  scoped_array<WORD> glyphs;
  scoped_array<WORD> logical_clusters;
  scoped_array<SCRIPT_VISATTR> visible_attributes;
  int glyph_count;

  scoped_array<int> advance_widths;
  scoped_array<GOFFSET> offsets;
  ABC abc_widths;
  SCRIPT_CACHE script_cache;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextRun);
};

}  // namespace internal

// RenderTextWin is the Windows implementation of RenderText using Uniscribe.
class RenderTextWin : public RenderText {
 public:
  RenderTextWin();
  virtual ~RenderTextWin();

  // Overridden from RenderText:
  virtual int GetStringWidth() OVERRIDE;
  virtual void Draw(Canvas* canvas) OVERRIDE;
  virtual SelectionModel FindCursorPosition(const Point& point) OVERRIDE;
  virtual Rect GetCursorBounds(const SelectionModel& selection,
                               bool insert_mode) OVERRIDE;

 protected:
  // Overridden from RenderText:
  virtual SelectionModel GetLeftSelectionModel(const SelectionModel& current,
                                               BreakType break_type) OVERRIDE;
  virtual SelectionModel GetRightSelectionModel(const SelectionModel& current,
                                                BreakType break_type) OVERRIDE;
  virtual SelectionModel LeftEndSelectionModel() OVERRIDE;
  virtual SelectionModel RightEndSelectionModel() OVERRIDE;
  virtual std::vector<Rect> GetSubstringBounds(size_t from, size_t to) OVERRIDE;
  virtual bool IsCursorablePosition(size_t position) OVERRIDE;
  virtual void UpdateLayout() OVERRIDE;

 private:
  virtual size_t IndexOfAdjacentGrapheme(size_t index, bool next) OVERRIDE;

  void EnsureLayout();
  void ItemizeLogicalText();
  void LayoutVisualText();

  // Return the run index that contains the argument; or the length of the
  // |runs_| vector if argument exceeds the text length or width.
  size_t GetRunContainingPosition(size_t position) const;
  size_t GetRunContainingPoint(const Point& point) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(internal::TextRun* run);
  SelectionModel LastSelectionModelInsideRun(internal::TextRun* run);

  // Get the selection model visually left/right of |selection| by one grapheme.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel LeftSelectionModel(const SelectionModel& selection);
  SelectionModel RightSelectionModel(const SelectionModel& selection);

  // Draw the text, cursor, and selection.
  void DrawSelection(Canvas* canvas);
  void DrawVisualText(Canvas* canvas);
  void DrawCursor(Canvas* canvas);

  // National Language Support native digit and digit substitution settings.
  SCRIPT_DIGITSUBSTITUTE digit_substitute_;

  SCRIPT_CONTROL script_control_;
  SCRIPT_STATE script_state_;

  std::vector<internal::TextRun*> runs_;
  int string_width_;

  scoped_array<int> visual_to_logical_;
  scoped_array<int> logical_to_visual_;

  bool needs_layout_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextWin);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_WIN_H_
