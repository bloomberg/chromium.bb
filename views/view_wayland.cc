// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include "base/logging.h"

namespace views {

gfx::NativeViewAccessible View::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

int View::GetHorizontalDragThreshold() {
  return 8;
}

int View::GetVerticalDragThreshold() {
  return GetHorizontalDragThreshold();
}

}  // namespace views
