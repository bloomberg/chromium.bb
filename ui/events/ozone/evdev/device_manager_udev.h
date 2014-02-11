// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_DEVICE_MANAGER_UDEV_H_
#define UI_EVENTS_OZONE_EVDEV_DEVICE_MANAGER_UDEV_H_

#include "base/memory/scoped_ptr.h"

namespace ui {

class DeviceManagerEvdev;

// Constructor for DeviceManagerUdev.
scoped_ptr<DeviceManagerEvdev> CreateDeviceManagerUdev();

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_DEVICE_MANAGER_UDEV_H_
