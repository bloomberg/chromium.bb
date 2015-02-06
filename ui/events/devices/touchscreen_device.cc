// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/touchscreen_device.h"

#include <string>

#include "ui/events/devices/input_device.h"

namespace ui {

TouchscreenDevice::TouchscreenDevice() : touch_points(0) {
}

TouchscreenDevice::TouchscreenDevice(unsigned int id,
                                     InputDeviceType type,
                                     const gfx::Size& size,
                                     int touch_points)
    : InputDevice(id, type), size(size), touch_points(touch_points) {
}

}  // namespace ui
