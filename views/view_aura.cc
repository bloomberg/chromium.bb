// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

namespace {

// Default horizontal drag threshold in pixels.
// Use the default horizontal drag threshold on gtk for now.
const int kDefaultHorizontalDragThreshold = 8;

}  // namespace

namespace views {

gfx::NativeViewAccessible View::GetNativeViewAccessible() {
  return NULL;
}

int View::GetHorizontalDragThreshold() {
  // TODO(jennyz): We may need to adjust this value for different platforms.
  return kDefaultHorizontalDragThreshold;
}

int View::GetVerticalDragThreshold() {
  return GetHorizontalDragThreshold();
}

}  // namespace views
