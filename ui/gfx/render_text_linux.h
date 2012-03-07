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
  virtual int GetStringWidth() OVERRIDE;
  virtual SelectionModel FindCursorPosition(const Point& point) OVERRIDE;
  virtual Rect GetCursorBounds(const SelectionModel& position,
                               bool insert_mode) OVERRIDE;

 protected:
  // Overridden from RenderText:
  virtual SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) OVERRIDE;
  virtual SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) OVERRIDE;
  virtual SelectionModel EdgeSelectionModel(
      VisualCursorDirection direction) OVERRIDE;
  virtual std::vector<Rect> GetSubstringBounds(size_t from, size_t to) OVERRIDE;
  virtual void SetSelectionModel(const SelectionModel& model) OVERRIDE;
  virtual bool IsCursorablePosition(size_t position) OVERRIDE;
  virtual void UpdateLayout() OVERRIDE;
  virtual void EnsureLayout() OVERRIDE;
  virtual void DrawVisualText(Canvas* canvas) OVERRIDE;

 private:
  virtual size_t IndexOfAdjacentGrapheme(
      size_t index,
      LogicalCursorDirection direction) OVERRIDE;

  // Returns the run that contains |position|. Return NULL if not found.
  GSList* GetRunContainingPosition(size_t position) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(const PangoItem* run);
  SelectionModel LastSelectionModelInsideRun(const PangoItem* run);

  // Unref |layout_| and |pango_line_|. Set them to NULL.
  void ResetLayout();

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

  // Calculate the visual bounds containing the logical substring within |from|
  // to |to|.
  std::vector<Rect> CalculateSubstringBounds(size_t from, size_t to);

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
