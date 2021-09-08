// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {

MockPlatformWindowDelegate::MockPlatformWindowDelegate() {}

MockPlatformWindowDelegate::~MockPlatformWindowDelegate() {}

bool operator==(const PlatformWindowDelegate::BoundsChange& bounds,
                const gfx::Rect& rect) {
  return bounds.bounds == rect;
}

}  // namespace ui
