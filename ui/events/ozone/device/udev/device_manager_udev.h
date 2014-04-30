// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_UDEV_DEVICE_MANAGER_UDEV_H_
#define UI_EVENTS_OZONE_DEVICE_UDEV_DEVICE_MANAGER_UDEV_H_

#include "base/message_loop/message_pump_libevent.h"
#include "base/observer_list.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/device/udev/scoped_udev.h"

namespace ui {

class DeviceEvent;
class DeviceEventObserver;

class DeviceManagerUdev
    : public DeviceManager, base::MessagePumpLibevent::Watcher {
 public:
  DeviceManagerUdev();
  virtual ~DeviceManagerUdev();

 private:
  scoped_ptr<DeviceEvent> ProcessMessage(udev_device* device);

  // DeviceManager overrides:
  virtual void ScanDevices(DeviceEventObserver* observer) OVERRIDE;
  virtual void AddObserver(DeviceEventObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DeviceEventObserver* observer) OVERRIDE;

  // base::MessagePumpLibevent::Watcher overrides:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  scoped_udev udev_;
  scoped_udev_monitor monitor_;

  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  ObserverList<DeviceEventObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerUdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_UDEV_DEVICE_MANAGER_UDEV_H_
