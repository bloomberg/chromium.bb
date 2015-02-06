// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/keyboard_device.h"

#include <string>

#include "ui/events/devices/input_device.h"

namespace ui {

KeyboardDevice::KeyboardDevice(int id, InputDeviceType type)
    : InputDevice(id, type) {
}

}  // namespace ui
