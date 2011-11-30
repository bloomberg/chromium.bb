// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/view_model_utils.h"

#include <algorithm>

#include "ui/aura_shell/launcher/view_model.h"
#include "ui/views/view.h"

namespace aura_shell {

// static
void ViewModelUtils::SetViewBoundsToIdealBounds(const ViewModel& model) {
  for (int i = 0; i < model.view_size(); ++i)
    model.view_at(i)->SetBoundsRect(model.ideal_bounds(i));
}

// static
int ViewModelUtils::DetermineMoveIndex(const ViewModel& model,
                                       views::View* view,
                                       int x) {
  int current_index = model.GetIndexOfView(view);
  DCHECK_NE(-1, current_index);
  for (int i = 0; i < current_index; ++i) {
    int mid_x = model.ideal_bounds(i).x() + model.ideal_bounds(i).width() / 2;
    if (x < mid_x)
      return i;
  }

  if (current_index + 1 == model.view_size())
    return current_index;

  // For indices after the current index ignore the bounds of the view being
  // dragged. This keeps the view from bouncing around as moved.
  int delta = model.ideal_bounds(current_index + 1).x() -
              model.ideal_bounds(current_index).x();
  for (int i = current_index + 1; i < model.view_size(); ++i) {
    const gfx::Rect& bounds(model.ideal_bounds(i));
    int mid_x = bounds.x() + bounds.width() / 2 - delta;
    if (x < mid_x)
      return i - 1;
  }
  return model.view_size() - 1;
}

}  // namespace aura_shell
