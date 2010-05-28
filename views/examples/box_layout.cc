// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/box_layout.h"

namespace examples {

BoxLayout::BoxLayout(Orientation orientation, int margin)
    : orientation_(orientation),
      margin_(margin) {
}

void BoxLayout::Layout(views::View* host) {
  int height = host->height();
  int width = host->width();
  int count = host->GetChildViewCount();

  int z = 0;
  for (int i = 0; i < count; ++i) {
    views::View* child = host->GetChildViewAt(i);

    if (orientation_ == kVertical) {
      child->SetBounds(0, z, width, height / count);
      z = (height + margin_) * (i + 1) / count;
    } else if (orientation_ == kHorizontal) {
      child->SetBounds(z, 0, width / count, height);
      z = (width + margin_) * (i + 1) / count;
    }
  }
}

gfx::Size BoxLayout::GetPreferredSize(views::View* host) {
  return gfx::Size();
}

}  // namespace examples
