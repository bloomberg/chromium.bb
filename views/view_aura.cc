// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

namespace views {

gfx::NativeViewAccessible View::GetNativeViewAccessible() {
  return NULL;
}

int View::GetHorizontalDragThreshold() {
  return 0;
}

int View::GetVerticalDragThreshold() {
  return 0;
}

}  // namespace views
