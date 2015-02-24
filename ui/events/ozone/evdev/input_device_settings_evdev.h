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

  bool tap_to_click_enabled;
  bool three_finger_click_enabled;
  bool tap_dragging_enabled;
  bool natural_scroll_enabled;
  bool tap_to_click_paused;

  int touchpad_sensitivity;
  int mouse_sensitivity;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
