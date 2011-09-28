// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SELECTION_MODEL_H_
#define UI_GFX_SELECTION_MODEL_H_
#pragma once

#include <stdlib.h>

#include "ui/base/ui_export.h"

namespace gfx {

// TODO(xji): publish bidi-editing guide line and replace the place holder.
// SelectionModel is used to represent the logical selection and visual
// position of cursor.
//
// For bi-directional text, the mapping between visual position and logical
// position is not one-to-one. For example, logical text "abcDEF" where capital
// letters stand for Hebrew, the visual display is "abcFED". According to the
// bidi editing guide (http://bidi-editing-guideline):
// 1. If pointing to the right half of the cell of a LTR character, the current
// position must be set after this character and the caret must be displayed
// after this character.
// 2. If pointing to the right half of the cell of a RTL character, the current
// position must be set before this character and the caret must be displayed
// before this character.
//
// Pointing to the right half of 'c' and pointing to the right half of 'D' both
// set the logical cursor position to 3. But the cursor displayed visually at
// different places:
// Pointing to the right half of 'c' displays the cursor right of 'c' as
// "abc|FED".
// Pointing to the right half of 'D' displays the cursor right of 'D' as
// "abcFED|".
// So, besides the logical selection start point and end point, we need extra
// information to specify to which character and on which edge of the character
// the visual cursor is bound to. For example, the visual cursor is bound to
// the trailing side of the 2nd character 'c' when pointing to right half of
// 'c'. And it is bound to the leading edge of the 3rd character 'D' when
// pointing to right of 'D'.
class UI_EXPORT SelectionModel {
 public:
  enum CaretPlacement {
    LEADING,
    TRAILING,
  };

  SelectionModel();
  explicit SelectionModel(size_t pos);
  SelectionModel(size_t start, size_t end);
  SelectionModel(size_t end, size_t pos, CaretPlacement status);
  SelectionModel(size_t start, size_t end, size_t pos, CaretPlacement status);

  virtual ~SelectionModel();

  size_t selection_start() const { return selection_start_; }
  void set_selection_start(size_t pos) { selection_start_ = pos; }

  size_t selection_end() const { return selection_end_; }
  void set_selection_end(size_t pos) { selection_end_ = pos; }

  size_t caret_pos() const { return caret_pos_; }
  void set_caret_pos(size_t pos) { caret_pos_ = pos; }

  CaretPlacement caret_placement() const { return caret_placement_; }
  void set_caret_placement(CaretPlacement placement) {
    caret_placement_ = placement;
  }

  bool Equals(const SelectionModel& sel) const;

 private:
  void Init(size_t start, size_t end, size_t pos, CaretPlacement status);

  // Logical selection start. If there is non-empty selection, if
  // selection_start_ is less than selection_end_, the selection starts visually
  // at the leading edge of the selection_start_. If selection_start_ is greater
  // than selection_end_, the selection starts visually at the trailing edge of
  // selection_start_'s previous grapheme. So, we do not need extra information
  // for visual bounding.
  size_t selection_start_;

  // The logical cursor position that next character will be inserted into.
  // It is also the end of the selection.
  size_t selection_end_;

  // The following two fields are used to guide cursor visual position.
  // The index of the character that cursor is visually attached to.
  size_t caret_pos_;
  // The visual placement of the cursor, relative to its associated character.
  CaretPlacement caret_placement_;
};

}  // namespace gfx

#endif  // UI_GFX_SELECTION_MODEL_H_
