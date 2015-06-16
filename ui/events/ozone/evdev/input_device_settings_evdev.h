// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_

namespace ui {

struct InputDeviceSettingsEvdev {
  InputDeviceSettingsEvdev();
  InputDeviceSettingsEvdev(const InputDeviceSettingsEvdev& other);
  ~InputDeviceSettingsEvdev();

  static const int kDefaultSensitivity = 3;

  // The initial settings are not critical since they will be shortly be changed
  // to the user's preferences or the application's own defaults.
  bool tap_to_click_enabled = true;
  bool three_finger_click_enabled = false;
  bool tap_dragging_enabled = false;
  bool natural_scroll_enabled = false;
  bool tap_to_click_paused = false;

  int touchpad_sensitivity = kDefaultSensitivity;
  int mouse_sensitivity = kDefaultSensitivity;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
