// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_editing_controller.h"

namespace ui {

namespace {
TouchSelectionControllerFactory* g_shared_instance = NULL;
}  // namespace

SelectionBound::SelectionBound()
    : type(ui::SelectionBound::EMPTY) {
}

SelectionBound::~SelectionBound() {
}

int SelectionBound::GetHeight() const {
  return edge_bottom.y() - edge_top.y();
}

bool operator==(const SelectionBound& lhs, const SelectionBound& rhs) {
  return lhs.type == rhs.type && lhs.edge_top == rhs.edge_top &&
         lhs.edge_bottom == rhs.edge_bottom;
}

bool operator!=(const SelectionBound& lhs, const SelectionBound& rhs) {
  return !(lhs == rhs);
}

gfx::Rect RectBetweenSelectionBounds(const SelectionBound& b1,
                                     const SelectionBound& b2) {
  int all_x[] ={
      b1.edge_top.x(), b2.edge_top.x(), b1.edge_bottom.x(), b2.edge_bottom.x()
  };
  int all_y[] = {
      b1.edge_top.y(), b2.edge_top.y(), b1.edge_bottom.y(), b2.edge_bottom.y()
  };
  const int num_elements = arraysize(all_x);
  COMPILE_ASSERT(arraysize(all_y) == num_elements, array_size_mismatch);

  int left = *std::min_element(all_x, all_x + num_elements);
  int top = *std::min_element(all_y, all_y + num_elements);
  int right = *std::max_element(all_x, all_x + num_elements);
  int bottom = *std::max_element(all_y, all_y + num_elements);

  return gfx::Rect(left, top, right - left, bottom - top);
}

TouchSelectionController* TouchSelectionController::create(
    TouchEditable* client_view) {
  if (g_shared_instance)
    return g_shared_instance->create(client_view);
  return NULL;
}

// static
void TouchSelectionControllerFactory::SetInstance(
    TouchSelectionControllerFactory* instance) {
  g_shared_instance = instance;
}

}  // namespace ui
