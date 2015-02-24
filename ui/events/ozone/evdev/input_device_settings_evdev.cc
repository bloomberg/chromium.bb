// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

namespace ui {

const int kDefaultSensitivity = 3;

// The initial settings are not critical since they will be shortly be changed
// to the user's preferences or the application's own defaults.

InputDeviceSettingsEvdev::InputDeviceSettingsEvdev()
    : tap_to_click_enabled(true),
      three_finger_click_enabled(false),
      tap_dragging_enabled(false),
      natural_scroll_enabled(false),
      tap_to_click_paused(false),
      touchpad_sensitivity(kDefaultSensitivity),
      mouse_sensitivity(kDefaultSensitivity) {
}

InputDeviceSettingsEvdev::InputDeviceSettingsEvdev(
    const InputDeviceSettingsEvdev& other) = default;

InputDeviceSettingsEvdev::~InputDeviceSettingsEvdev() {
}

}  // namespace ui
