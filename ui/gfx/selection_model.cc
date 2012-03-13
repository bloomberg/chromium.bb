// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/selection_model.h"

#include <ostream>

namespace gfx {

SelectionModel::SelectionModel()
  : selection_(0), caret_affinity_(CURSOR_BACKWARD) {}

SelectionModel::SelectionModel(size_t position, LogicalCursorDirection affinity)
  : selection_(position), caret_affinity_(affinity) {}

SelectionModel::SelectionModel(ui::Range selection,
                               LogicalCursorDirection affinity)
  : selection_(selection), caret_affinity_(affinity) {}

bool SelectionModel::operator==(const SelectionModel& sel) const {
  return selection_ == sel.selection() &&
         caret_affinity_ == sel.caret_affinity();
}

std::ostream& operator<<(std::ostream& out, const SelectionModel& sel) {
  out << '{';
  if (sel.selection().is_empty())
    out << sel.caret_pos();
  else
    out << sel.selection();
  bool backward = sel.caret_affinity() == CURSOR_BACKWARD;
  return out << (backward ? ",BACKWARD}" : ",FORWARD}");
}

}  // namespace gfx
