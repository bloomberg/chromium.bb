// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/device_manager_evdev.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"

namespace ui {

namespace {

class DeviceManagerManual : public DeviceManagerEvdev {
 public:
  DeviceManagerManual() {}
  virtual ~DeviceManagerManual() {}

  // Enumerate existing devices & start watching for device changes.
  virtual void ScanAndStartMonitoring(const EvdevDeviceCallback& device_added,
                                      const EvdevDeviceCallback& device_removed)
      OVERRIDE {
    base::FileEnumerator file_enum(base::FilePath("/dev/input"),
                                   false,
                                   base::FileEnumerator::FILES,
                                   "event*[0-9]");
    for (base::FilePath path = file_enum.Next(); !path.empty();
         path = file_enum.Next())
      device_added.Run(path);
  }

  virtual void Stop() OVERRIDE {}
};

}  // namespace

DeviceManagerEvdev::~DeviceManagerEvdev() {}

scoped_ptr<DeviceManagerEvdev> CreateDeviceManagerManual() {
  return make_scoped_ptr<DeviceManagerEvdev>(new DeviceManagerManual);
}

}  // namespace ui
