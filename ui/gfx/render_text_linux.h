// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  virtual SelectionModel GetLeftSelectionModel(const SelectionModel& current,
                                               BreakType break_type) OVERRIDE;
  virtual SelectionModel GetRightSelectionModel(const SelectionModel& current,
                                                BreakType break_type) OVERRIDE;
  virtual SelectionModel LeftEndSelectionModel() OVERRIDE;
  virtual SelectionModel RightEndSelectionModel() OVERRIDE;
  virtual void GetSubstringBounds(size_t from,
                                  size_t to,
                                  std::vector<Rect>* bounds) OVERRIDE;
  virtual void SetSelectionModel(const SelectionModel& model) OVERRIDE;
  virtual bool IsCursorablePosition(size_t position) OVERRIDE;
  virtual void UpdateLayout() OVERRIDE;
  virtual void EnsureLayout() OVERRIDE;
  virtual void DrawVisualText(Canvas* canvas) OVERRIDE;

 private:
  virtual size_t IndexOfAdjacentGrapheme(size_t index, bool next) OVERRIDE;

  // Returns the run that contains |position|. Return NULL if not found.
  GSList* GetRunContainingPosition(size_t position) const;

  // Given |utf8_index_of_current_grapheme|, returns the UTF8 or UTF16 index of
  // next grapheme in the text if |next| is true, otherwise, returns the index
  // of previous grapheme. Returns 0 if there is no previous grapheme, and
  // returns the |text_| length if there is no next grapheme.
  size_t Utf8IndexOfAdjacentGrapheme(size_t utf8_index_of_current_grapheme,
                                     bool next) const;
  size_t Utf16IndexOfAdjacentGrapheme(size_t utf8_index_of_current_grapheme,
                                      bool next) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(const PangoItem* run) const;
  SelectionModel LastSelectionModelInsideRun(const PangoItem* run) const;

  // Get the selection model visually left or right of |current| by one
  // grapheme.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel LeftSelectionModel(const SelectionModel& current);
  SelectionModel RightSelectionModel(const SelectionModel& current);

  // Get the selection model visually left or right of |current| by one word.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel LeftSelectionModelByWord(const SelectionModel& current);
  SelectionModel RightSelectionModelByWord(const SelectionModel& current);

  // Unref |layout_| and |pango_line_|. Set them to NULL.
  void ResetLayout();

  // Setup pango attribute: foreground, background, font, strike.
  void SetupPangoAttributes(PangoLayout* layout);

  // Append one pango attribute |pango_attr| into pango attribute list |attrs|.
  void AppendPangoAttribute(size_t start,
                            size_t end,
                            PangoAttribute* pango_attr,
                            PangoAttrList* attrs);

  // Returns |run|'s visually previous run.
  // The complexity is O(n) since it is a single-linked list.
  PangoLayoutRun* GetPreviousRun(PangoLayoutRun* run) const;

  // Returns the last run in |current_line_|.
  // The complexity is O(n) since it is a single-linked list.
  PangoLayoutRun* GetLastRun() const;

  size_t Utf16IndexToUtf8Index(size_t index) const;
  size_t Utf8IndexToUtf16Index(size_t index) const;

  // Calculate the visual bounds containing the logical substring within |from|
  // to |to| into |bounds|.
  void CalculateSubstringBounds(size_t from,
                                size_t to,
                                std::vector<Rect>* bounds);

  // Save the visual bounds of logical selection into |bounds|.
  void GetSelectionBounds(std::vector<Rect>* bounds);

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
