// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_EVENT_OBSERVER_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_EVENT_OBSERVER_H_

#include "ui/events/devices/events_devices_export.h"

namespace ui {

// DeviceDataManager observer used to announce input hotplug events.
class EVENTS_DEVICES_EXPORT InputDeviceEventObserver {
 public:
  virtual ~InputDeviceEventObserver() {}

  virtual void OnKeyboardDeviceConfigurationChanged() = 0;
  virtual void OnTouchscreenDeviceConfigurationChanged() = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_EVENT_OBSERVER_H_
