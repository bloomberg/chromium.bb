// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device.h"

#include <string>

namespace ui {

// static
const unsigned int InputDevice::kInvalidId = 0;

InputDevice::InputDevice()
    : id(kInvalidId), type(InputDeviceType::INPUT_DEVICE_UNKNOWN) {
}

InputDevice::InputDevice(unsigned int id, InputDeviceType type)
    : id(id), type(type) {
}

InputDevice::~InputDevice() {
}

}  // namespace ui
