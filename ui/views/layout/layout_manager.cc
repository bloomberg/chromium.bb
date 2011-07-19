// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager.h"

#include "ui/views/view.h"

namespace ui {

int LayoutManager::GetPreferredHeightForWidth(View* host, int width) {
  return GetPreferredSize(host).height();
}

}  // namespace ui
