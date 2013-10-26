// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

namespace ui {

bool IsTouchDevicePresent() {
  return true;
}

// Looks like the best we can do here is detect 1, 2+, or 5+ by
// feature detecting:
// FEATURE_TOUCHSCREEN (1),
// FEATURE_TOUCHSCREEN_MULTITOUCH (2),
// FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT (2+), or
// FEATURE_TOUCHSCREEN_MULTITOUCH_JAZZHANDS (5+)
//
// Probably start from the biggest and detect down the list until we
// find one that's supported and return its value.
int MaxTouchPoints() {
  return kMaxTouchPointsUnknown;
}

}  // namespace ui
