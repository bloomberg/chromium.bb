// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_MANAGER_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothGattManagerClient is used to communicate with the GATT Service
// manager object of the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattManagerClient
    : public BluezDBusClient {
 public:
  // Options used to register a GATT service hierarchy.
  struct DEVICE_BLUETOOTH_EXPORT Options {
    // TODO(armansito): This parameter is not yet clearly defined. Add fields
    // later as we know more about how this will be used.
  };

  ~BluetoothGattManagerClient() override;

  // The ErrorCallback is used by GATT manager methods to indicate failure. It
  // receives two arguments: the name of the error in |error_name| and an
  // optional message in |error_message|.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)> ErrorCallback;

  // Registers a GATT service implementation within the local process at the
  // D-Bus object path |service_path| with the remote GATT manager. The local
  // service must implement the GattService1 interface. Characteristic objects
  // must be hierarchical to their service and must use the interface
  // GattCharacteristic1. Similarly, characteristic descriptor objects must
  // implement the GattDescriptor1 interface and must be hierarchical to their
  // characteristic. In a successful invocation of RegisterService, the
  // Bluetooth daemon will discover all objects in the registered hierarchy by
  // using D-Bus Object Manager. Hence, the object paths and the interfaces in
  // the registered hierarchy must comply with the BlueZ GATT D-Bus
  // specification.
  virtual void RegisterApplication(const dbus::ObjectPath& adapter_object_path,
                                   const dbus::ObjectPath& application_path,
                                   const Options& options,
                                   const base::Closure& callback,
                                   const ErrorCallback& error_callback) = 0;

  // Unregisters the GATT service with the D-Bus object path |service_path| from
  // the remote GATT manager.
  virtual void UnregisterApplication(
      const dbus::ObjectPath& adapter_object_path,
      const dbus::ObjectPath& application_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Creates the instance.
  static BluetoothGattManagerClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

 protected:
  BluetoothGattManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattManagerClient);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_MANAGER_CLIENT_H_
