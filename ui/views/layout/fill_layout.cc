// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/fill_layout.h"

#include "base/logging.h"

namespace views {

FillLayout::FillLayout() {}

FillLayout::~FillLayout() {}

void FillLayout::Layout(View* host) {
  if (!host->has_children())
    return;

  View* frame_view = host->child_at(0);
  frame_view->SetBoundsRect(host->GetContentsBounds());
}

gfx::Size FillLayout::GetPreferredSize(View* host) {
  DCHECK_EQ(1, host->child_count());
  gfx::Rect rect(host->child_at(0)->GetPreferredSize());
  rect.Inset(-host->GetInsets());
  return rect.size();
}

int FillLayout::GetPreferredHeightForWidth(View* host, int width) {
  DCHECK_EQ(1, host->child_count());
  const gfx::Insets insets = host->GetInsets();
  return host->child_at(0)->GetHeightForWidth(width - insets.width()) +
      insets.height();
}

}  // namespace views
