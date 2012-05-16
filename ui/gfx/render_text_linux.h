// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_LINUX_H_
#define UI_GFX_RENDER_TEXT_LINUX_H_
#pragma once

#include <pango/pango.h>
#include <vector>

#include "ui/gfx/render_text.h"

namespace gfx {

// RenderTextLinux is the Linux implementation of RenderText using Pango.
class RenderTextLinux : public RenderText {
 public:
  RenderTextLinux();
  virtual ~RenderTextLinux();

  // Overridden from RenderText:
  virtual base::i18n::TextDirection GetTextDirection() OVERRIDE;
  virtual Size GetStringSize() OVERRIDE;
  virtual SelectionModel FindCursorPosition(const Point& point) OVERRIDE;
  virtual std::vector<FontSpan> GetFontSpansForTesting() OVERRIDE;

 protected:
  // Overridden from RenderText:
  virtual SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) OVERRIDE;
  virtual SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) OVERRIDE;
  virtual void SetSelectionModel(const SelectionModel& model) OVERRIDE;
  virtual void GetGlyphBounds(size_t index,
                              ui::Range* xspan,
                              int* height) OVERRIDE;
  virtual std::vector<Rect> GetSubstringBounds(ui::Range range) OVERRIDE;
  virtual bool IsCursorablePosition(size_t position) OVERRIDE;
  virtual void ResetLayout() OVERRIDE;
  virtual void EnsureLayout() OVERRIDE;
  virtual void DrawVisualText(Canvas* canvas) OVERRIDE;

 private:
  // Returns the run that contains the character attached to the caret in the
  // given selection model. Return NULL if not found.
  GSList* GetRunContainingCaret(const SelectionModel& caret) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(const PangoItem* run);
  SelectionModel LastSelectionModelInsideRun(const PangoItem* run);

  // Setup pango attribute: foreground, background, font, strike.
  void SetupPangoAttributes(PangoLayout* layout);

  // Append one pango attribute |pango_attr| into pango attribute list |attrs|.
  void AppendPangoAttribute(size_t start,
                            size_t end,
                            PangoAttribute* pango_attr,
                            PangoAttrList* attrs);

  // Convert between indices into text() and indices into |layout_text_|.
  size_t TextIndexToLayoutIndex(size_t index) const;
  size_t LayoutIndexToTextIndex(size_t index) const;

  // Calculate the visual bounds containing the logical substring within the
  // given range.
  std::vector<Rect> CalculateSubstringBounds(ui::Range range);

  // Get the visual bounds of the logical selection.
  std::vector<Rect> GetSelectionBounds();

  // Pango Layout.
  PangoLayout* layout_;
  // A single line layout resulting from laying out via |layout_|.
  PangoLayoutLine* current_line_;

  // Information about character attributes.
  PangoLogAttr* log_attrs_;
  // Number of attributes in |log_attrs_|.
  int num_log_attrs_;

  // Vector of the visual bounds containing the logical substring of selection.
  std::vector<Rect> selection_visual_bounds_;

  // The text in the |layout_|.
  const char* layout_text_;
  // The text length.
  size_t layout_text_len_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextLinux);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_LINUX_H_
