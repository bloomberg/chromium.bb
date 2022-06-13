// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_application_service_provider.h"

namespace bluez {

// The BluetoothAdvertisementMonitorManagerClient simulates the behavior of the
// Bluetooth daemon's Advertisement Monitor Manager object and is used in
// test case.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAdvertisementMonitorManagerClient
    : public BluetoothAdvertisementMonitorManagerClient {
 public:
  FakeBluetoothAdvertisementMonitorManagerClient();
  ~FakeBluetoothAdvertisementMonitorManagerClient() override;
  FakeBluetoothAdvertisementMonitorManagerClient(
      const FakeBluetoothAdvertisementMonitorManagerClient&) = delete;
  FakeBluetoothAdvertisementMonitorManagerClient& operator=(
      const FakeBluetoothAdvertisementMonitorManagerClient&) = delete;

  // DBusClient override:
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;

  // BluetoothAdvertisementMonitorManagerClient override.
  void RegisterMonitor(const dbus::ObjectPath& application,
                       const dbus::ObjectPath& adapter,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  void UnregisterMonitor(const dbus::ObjectPath& application,
                         const dbus::ObjectPath& adapter,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;

  void RegisterApplicationServiceProvider(
      FakeBluetoothAdvertisementMonitorApplicationServiceProvider* provider);

  // This allows tests to control whether GetProperties() will return nullptr or
  // a valid object.
  void InitializeProperties();
  void RemoveProperties();

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
  application_provider() {
    return application_provider_;
  }

 private:
  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
      application_provider_ = nullptr;
  std::unique_ptr<Properties> properties_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
