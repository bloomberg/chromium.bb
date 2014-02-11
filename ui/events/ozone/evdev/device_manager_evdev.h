// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_DEVICE_MANAGER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_DEVICE_MANAGER_EVDEV_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace ui {

typedef base::Callback<void(const base::FilePath& file_path)>
    EvdevDeviceCallback;

// Interface for scanning & monitoring input devices.
class DeviceManagerEvdev {
 public:
  virtual ~DeviceManagerEvdev();

  // Enumerate devices & start watching for changes.
  // Must be balanced by a call to Stop().
  virtual void ScanAndStartMonitoring(
      const EvdevDeviceCallback& device_added,
      const EvdevDeviceCallback& device_removed) = 0;

  // Stop watching for device changes.
  virtual void Stop() = 0;
};

// Create simple DeviceManagerEvdev that scans /dev/input manually.
scoped_ptr<DeviceManagerEvdev> CreateDeviceManagerManual();

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_DEVICE_MANAGER_EVDEV_H_
