// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_LAYOUT_BOX_LAYOUT_H_
#define VIEWS_LAYOUT_BOX_LAYOUT_H_
#pragma once

#include "base/basictypes.h"
#include "views/layout_manager.h"

namespace views {

// A Layout manager that arranges child views vertically or horizontally in a
// side-by-side fashion with spacing around and between the child views. The
// child views are always sized according to their preferred size. If the
// host's bounds provide insufficient space, child views will be clamped.
// Excess space will not be distributed.
class BoxLayout : public LayoutManager {
 public:
  enum Orientation {
    kHorizontal,
    kVertical,
  };

  // Use |inside_border_horizontal_spacing| and
  // |inside_border_vertical_spacing| to add additional space between the child
  // view area and the host view border. |between_child_spacing| controls the
  // space in between child views.
  BoxLayout(Orientation orientation,
            int inside_border_horizontal_spacing,
            int inside_border_vertical_spacing,
            int between_child_spacing);
  virtual ~BoxLayout() {}

  // Overridden from views::LayoutManager:
  virtual void Layout(View* host);
  virtual gfx::Size GetPreferredSize(View* host);

 private:
  const Orientation orientation_;

  // Spacing between child views and host view border.
  const int inside_border_horizontal_spacing_;
  const int inside_border_vertical_spacing_;

  // Spacing to put in between child views.
  const int between_child_spacing_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BoxLayout);
};

} // namespace views

#endif // VIEWS_LAYOUT_BOX_LAYOUT_H_
