// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h>

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"

namespace device {

// static
base::WeakPtr<BluetoothLocalGattService> BluetoothLocalGattService::Create(
    BluetoothAdapter* adapter,
    const BluetoothUUID& uuid,
    bool is_primary,
    BluetoothLocalGattService* included_service,
    BluetoothLocalGattService::Delegate* delegate) {
  bluez::BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<bluez::BluetoothAdapterBlueZ*>(adapter);
  bluez::BluetoothLocalGattServiceBlueZ* service =
      new bluez::BluetoothLocalGattServiceBlueZ(adapter_bluez, uuid, is_primary,
                                                delegate);
  return service->weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device

namespace bluez {

BluetoothLocalGattServiceBlueZ::BluetoothLocalGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const device::BluetoothUUID& uuid,
    bool is_primary,
    device::BluetoothLocalGattService::Delegate* delegate)
    : BluetoothGattServiceBlueZ(
          adapter,
          AddGuidToObjectPath(adapter->GetApplicationObjectPath().value() +
                              "/service")),
      uuid_(uuid),
      is_primary_(is_primary),
      delegate_(delegate) {
  DVLOG(1) << "Creating local GATT service with identifier: "
           << GetIdentifier();
  adapter->AddLocalGattService(base::WrapUnique(this));
}

BluetoothLocalGattServiceBlueZ::~BluetoothLocalGattServiceBlueZ() = default;

device::BluetoothUUID BluetoothLocalGattServiceBlueZ::GetUUID() const {
  return uuid_;
}

bool BluetoothLocalGattServiceBlueZ::IsPrimary() const {
  return is_primary_;
}

void BluetoothLocalGattServiceBlueZ::Register(const base::Closure& callback,
                                              ErrorCallback error_callback) {
  GetAdapter()->RegisterGattService(this, callback, std::move(error_callback));
}

void BluetoothLocalGattServiceBlueZ::Unregister(const base::Closure& callback,
                                                ErrorCallback error_callback) {
  DCHECK(GetAdapter());
  GetAdapter()->UnregisterGattService(this, callback,
                                      std::move(error_callback));
}

bool BluetoothLocalGattServiceBlueZ::IsRegistered() {
  return GetAdapter()->IsGattServiceRegistered(this);
}

void BluetoothLocalGattServiceBlueZ::Delete() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetAdapter()->RemoveLocalGattService(this);
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattServiceBlueZ::GetCharacteristic(
    const std::string& identifier) {
  const auto& service = characteristics_.find(dbus::ObjectPath(identifier));
  return service == characteristics_.end() ? nullptr : service->second.get();
}

const std::map<dbus::ObjectPath,
               std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ>>&
BluetoothLocalGattServiceBlueZ::GetCharacteristics() const {
  return characteristics_;
}

// static
dbus::ObjectPath BluetoothLocalGattServiceBlueZ::AddGuidToObjectPath(
    const std::string& path) {
  std::string GuidString = base::GenerateGUID();
  base::RemoveChars(GuidString, "-", &GuidString);

  return dbus::ObjectPath(path + GuidString);
}

void BluetoothLocalGattServiceBlueZ::AddCharacteristic(
    std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ> characteristic) {
  characteristics_[characteristic->object_path()] = std::move(characteristic);
}

}  // namespace bluez
