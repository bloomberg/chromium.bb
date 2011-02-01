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
  if (host->child_count() == 0)
    return;

  View* child = host->GetChildViewAt(0);
  child->SetBounds(0, 0, host->width(), host->height());
}

gfx::Size FillLayout::GetPreferredSize(View* host) {
  DCHECK(host->child_count() == 1);
  return host->GetChildViewAt(0)->GetPreferredSize();
}

}  // namespace ui
