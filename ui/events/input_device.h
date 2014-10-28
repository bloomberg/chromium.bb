// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_INPUT_DEVICE_H_
#define UI_EVENTS_INPUT_DEVICE_H_

#include <string>

#include "ui/events/events_base_export.h"

namespace ui {

enum InputDeviceType {
  INPUT_DEVICE_INTERNAL,  // Internally connected input device.
  INPUT_DEVICE_EXTERNAL,  // Known externally connected input device.
  INPUT_DEVICE_UNKNOWN,   // Device that may or may not be an external device.
};

// Represents an input device state.
struct EVENTS_BASE_EXPORT InputDevice {
  static const unsigned int kInvalidId;

  // Creates an invalid input device.
  InputDevice();

  InputDevice(unsigned int id, InputDeviceType type, const std::string& name);
  virtual ~InputDevice();

  // ID of the device. This ID is unique between all input devices.
  unsigned int id;

  // The type of the input device.
  InputDeviceType type;

  // Name of the device.
  std::string name;
};

}  // namespace ui

#endif  // UI_EVENTS_INPUT_DEVICE_H_
