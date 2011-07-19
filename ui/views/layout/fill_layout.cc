// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/fill_layout.h"

#include "base/logging.h"
#include "ui/views/view.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// FillLayout, public:

FillLayout::FillLayout() {
}

FillLayout::~FillLayout() {
}

////////////////////////////////////////////////////////////////////////////////
// FillLayout, LayoutManager implementation:

void FillLayout::Layout(View* host) {
  if (host->children_empty())
    return;

  View* child = host->child_at(0);
  child->SetBounds(gfx::Rect(gfx::Point(), host->size()));
}

gfx::Size FillLayout::GetPreferredSize(View* host) {
  DCHECK_EQ(1U, host->children_size());
  return host->child_at(0)->GetPreferredSize();
}

}  // namespace ui
