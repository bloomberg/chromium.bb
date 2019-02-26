// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager.h"

#include "ui/views/view.h"

namespace views {

LayoutManager::~LayoutManager() {
}

void LayoutManager::Installed(View* host) {
}

void LayoutManager::InvalidateLayout() {}

gfx::Size LayoutManager::GetMinimumSize(const View* host) const {
  // Fall back to using preferred size if no minimum size calculation is
  // available (e.g. legacy layout managers).
  //
  // Ideally we'd just call GetPreferredSize() on ourselves here, but because
  // some legacy views with layout managers override GetPreferredSize(), we need
  // to call GetPreferredSize() on the host view instead. The default
  // views::View behavior will be to call GetPreferredSize() on this layout
  // manager, so the fallback behavior in all other cases is as expected.
  return host->GetPreferredSize();
}

int LayoutManager::GetPreferredHeightForWidth(const View* host,
                                              int width) const {
  return GetPreferredSize(host).height();
}

void LayoutManager::ViewAdded(View* host, View* view) {
}

void LayoutManager::ViewRemoved(View* host, View* view) {
}

}  // namespace views
