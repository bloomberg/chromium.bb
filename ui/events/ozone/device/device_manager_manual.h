// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_DEVICE_MANAGER_MANUAL_H_
#define UI_EVENTS_OZONE_DEVICE_DEVICE_MANAGER_MANUAL_H_

#include "base/macros.h"
#include "ui/events/ozone/device/device_manager.h"

namespace ui {

class DeviceManagerManual : public DeviceManager {
 public:
  DeviceManagerManual();
  ~DeviceManagerManual() override;

 private:
  // DeviceManager overrides:
  void ScanDevices(DeviceEventObserver* observer) override;
  void AddObserver(DeviceEventObserver* observer) override;
  void RemoveObserver(DeviceEventObserver* observer) override;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerManual);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_DEVICE_MANAGER_MANUAL_H_
