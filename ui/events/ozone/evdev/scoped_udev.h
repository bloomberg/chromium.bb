// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_SCOPED_UDEV_H_
#define UI_EVENTS_OZONE_EVDEV_SCOPED_UDEV_H_

#include <libudev.h>

#include "base/memory/scoped_ptr.h"

// This file enables management of udev object references using scoped_ptr<> by
// associating a custom deleter that drops the reference rather than deleting.

namespace ui {

// Scoped struct udev.
struct UdevDeleter {
  void operator()(struct udev* udev) { udev_unref(udev); }
};
typedef scoped_ptr<struct udev, UdevDeleter> scoped_udev;

// Scoped struct udev_device.
struct UdevDeviceDeleter {
  void operator()(struct udev_device* udev_device) {
    udev_device_unref(udev_device);
  }
};
typedef scoped_ptr<struct udev_device, UdevDeviceDeleter> scoped_udev_device;

// Scoped struct udev_enumerate.
struct UdevEnumerateDeleter {
  void operator()(struct udev_enumerate* udev_enumerate) {
    udev_enumerate_unref(udev_enumerate);
  }
};
typedef scoped_ptr<struct udev_enumerate, UdevEnumerateDeleter>
    scoped_udev_enumerate;

// Scoped struct udev_monitor.
struct UdevMonitorDeleter {
  void operator()(struct udev_monitor* udev_monitor) {
    udev_monitor_unref(udev_monitor);
  }
};
typedef scoped_ptr<struct udev_monitor, UdevMonitorDeleter> scoped_udev_monitor;

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_SCOPED_UDEV_H_
