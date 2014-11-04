// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_HOTPLUG_EVENT_OBSERVER_H_
#define UI_EVENTS_DEVICES_DEVICE_HOTPLUG_EVENT_OBSERVER_H_

#include <vector>

#include "ui/events/devices/events_devices_export.h"

namespace ui {

struct KeyboardDevice;
struct TouchscreenDevice;

// Listener for specific input device hotplug events.
class EVENTS_DEVICES_EXPORT DeviceHotplugEventObserver {
 public:
  virtual ~DeviceHotplugEventObserver() {}

  // On a hotplug event this is called with the list of available touchscreen
  // devices. The set of touchscreen devices may not necessarily have changed.
  virtual void OnTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) = 0;

  // On a hotplug event this is called with the list of available keyboard
  // devices. The set of keyboard devices may not necessarily have changed.
  virtual void OnKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_HOTPLUG_EVENT_OBSERVER_H_
