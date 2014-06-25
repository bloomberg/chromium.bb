// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views.h"

namespace views {

StaticSizedView::StaticSizedView(const gfx::Size& size) : size_(size) {}

StaticSizedView::~StaticSizedView() {}

gfx::Size StaticSizedView::GetPreferredSize() const {
  return size_;
}

ProportionallySizedView::ProportionallySizedView(int factor)
    : factor_(factor), preferred_width_(-1) {}

ProportionallySizedView::~ProportionallySizedView() {}

int ProportionallySizedView::GetHeightForWidth(int w) const {
  return w * factor_;
}

gfx::Size ProportionallySizedView::GetPreferredSize() const {
  if (preferred_width_ >= 0)
    return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
  return View::GetPreferredSize();
}

}  // namespace views
