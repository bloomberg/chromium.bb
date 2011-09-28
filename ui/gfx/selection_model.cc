// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/selection_model.h"

namespace gfx {

SelectionModel::SelectionModel() {
  Init(0, 0, 0, LEADING);
}

SelectionModel::SelectionModel(size_t pos) {
  Init(pos, pos, pos, LEADING);
}

SelectionModel::SelectionModel(size_t start, size_t end) {
  Init(start, end, end, LEADING);
}

SelectionModel::SelectionModel(size_t end,
                               size_t pos,
                               CaretPlacement placement) {
  Init(end, end, pos, placement);
}

SelectionModel::SelectionModel(size_t start,
                               size_t end,
                               size_t pos,
                               CaretPlacement placement) {
  Init(start, end, pos, placement);
}

SelectionModel::~SelectionModel() {
}

bool SelectionModel::Equals(const SelectionModel& sel) const {
  return selection_start_ == sel.selection_start() &&
         selection_end_ == sel.selection_end() &&
         caret_pos_ == sel.caret_pos() &&
         caret_placement_ == sel.caret_placement();
}

void SelectionModel::Init(size_t start,
                          size_t end,
                          size_t pos,
                          CaretPlacement placement) {
  selection_start_ = start;
  selection_end_ = end;
  caret_pos_ = pos;
  caret_placement_ = placement;
}

}  // namespace gfx
