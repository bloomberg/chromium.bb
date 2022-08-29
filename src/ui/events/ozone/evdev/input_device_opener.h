// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_OPENER_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_OPENER_H_

#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#endif

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;

struct OpenInputDeviceParams {
  // Unique identifier for the new device.
  int id;

  // Device path to open.
  base::FilePath path;

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* dispatcher;

  // State shared between devices.
  CursorDelegateEvdev* cursor;
#if defined(USE_EVDEV_GESTURES)
  GesturePropertyProvider* gesture_property_provider;
#endif
  SharedPalmDetectionFilterState* shared_palm_state;
};

class COMPONENT_EXPORT(EVDEV) InputDeviceOpener {
 public:
  virtual ~InputDeviceOpener() = default;

  virtual std::unique_ptr<EventConverterEvdev> OpenInputDevice(
      const OpenInputDeviceParams& params) = 0;
};
}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_OPENER_H_
