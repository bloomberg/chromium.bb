// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_BOX_LAYOUT_H_
#define VIEWS_EXAMPLES_BOX_LAYOUT_H_

#include "views/layout_manager.h"

namespace examples {

// A layout manager that layouts child views vertically or horizontally.
class BoxLayout : public views::LayoutManager {
 public:
  enum Orientation {
    kHorizontal,
    kVertical,
  };

  BoxLayout(Orientation orientation, int margin);

  virtual ~BoxLayout() {}

  // Overridden from views::LayoutManager:
  virtual void Layout(views::View* host);

  virtual gfx::Size GetPreferredSize(views::View* host);

 private:
  const Orientation orientation_;

  // The pixel distance between children.
  const int margin_;

  DISALLOW_COPY_AND_ASSIGN(BoxLayout);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_BOX_LAYOUT_H_
