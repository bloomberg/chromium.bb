// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TOUCH_TOUCH_DEVICE_H_
#define UI_BASE_TOUCH_TOUCH_DEVICE_H_

#include "ui/base/ui_export.h"

namespace ui {

// TODO(sblom): This is non-standard, and should be removed before
// RuntimeEnabledFlags::PointerEventsMaxTouchPoints is marked stable.
// Tracked by: http://crbug.com/308649
const int kMaxTouchPointsUnknown = -1;

// Returns true if a touch device is available.
UI_EXPORT bool IsTouchDevicePresent();

// Returns the maximum number of simultaneous touch contacts supported
// by the device. In the case of devices with multiple digitizers (e.g.
// multiple touchscreens), the value MUST be the maximum of the set of
// maximum supported contacts by each individual digitizer.
// For example, suppose a device has 3 touchscreens, which support 2, 5,
// and 10 simultaneous touch contacts, respectively. This returns 10.
// http://www.w3.org/TR/pointerevents/#widl-Navigator-maxTouchPoints
UI_EXPORT int MaxTouchPoints();

}  // namespace ui

#endif  // UI_BASE_TOUCH_TOUCH_DEVICE_H_
