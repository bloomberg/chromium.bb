// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/touchscreen_device.h"

#include <string>

#include "ui/events/devices/input_device.h"

namespace ui {

TouchscreenDevice::TouchscreenDevice() = default;

TouchscreenDevice::TouchscreenDevice(int id,
                                     InputDeviceType type,
                                     const std::string& name,
                                     const gfx::Size& size,
                                     int touch_points,
                                     bool has_stylus)
    : InputDevice(id, type, name),
      size(size),
      touch_points(touch_points),
      has_stylus(has_stylus) {}

TouchscreenDevice::TouchscreenDevice(const InputDevice& input_device,
                                     const gfx::Size& size,
                                     int touch_points,
                                     bool has_stylus)
    : InputDevice(input_device),
      size(size),
      touch_points(touch_points),
      has_stylus(has_stylus) {}

TouchscreenDevice::TouchscreenDevice(const TouchscreenDevice& other) = default;

TouchscreenDevice::~TouchscreenDevice() = default;

}  // namespace ui
