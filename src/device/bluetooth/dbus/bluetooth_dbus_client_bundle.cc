// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_dbus_client_bundle.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_battery_client.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_battery_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"

namespace bluez {

BluetoothDBusClientBundle::BluetoothDBusClientBundle(bool use_fakes)
    : use_fakes_(use_fakes) {
  if (!use_fakes_) {
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    bluetooth_le_advertising_manager_client_.reset(
        BluetoothLEAdvertisingManagerClient::Create());
    bluetooth_agent_manager_client_.reset(
        BluetoothAgentManagerClient::Create());
    bluetooth_battery_client_.reset(BluetoothBatteryClient::Create());
    bluetooth_debug_manager_client_.reset(
        BluetoothDebugManagerClient::Create());
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
    bluetooth_input_client_.reset(BluetoothInputClient::Create());
    bluetooth_profile_manager_client_.reset(
        BluetoothProfileManagerClient::Create());
    bluetooth_gatt_characteristic_client_.reset(
        BluetoothGattCharacteristicClient::Create());
    bluetooth_gatt_descriptor_client_.reset(
        BluetoothGattDescriptorClient::Create());
    bluetooth_gatt_manager_client_.reset(BluetoothGattManagerClient::Create());
    bluetooth_gatt_service_client_.reset(BluetoothGattServiceClient::Create());

    alternate_bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    alternate_bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
  } else {
#if defined(USE_REAL_DBUS_CLIENTS)
    LOG(FATAL) << "Fakes are unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
    bluetooth_adapter_client_.reset(new FakeBluetoothAdapterClient);
    bluetooth_le_advertising_manager_client_.reset(
        new FakeBluetoothLEAdvertisingManagerClient);
    bluetooth_agent_manager_client_.reset(new FakeBluetoothAgentManagerClient);
    bluetooth_battery_client_.reset(new FakeBluetoothBatteryClient);
    bluetooth_debug_manager_client_.reset(new FakeBluetoothDebugManagerClient);
    bluetooth_device_client_.reset(new FakeBluetoothDeviceClient);
    bluetooth_input_client_.reset(new FakeBluetoothInputClient);
    bluetooth_profile_manager_client_.reset(
        new FakeBluetoothProfileManagerClient);
    bluetooth_gatt_characteristic_client_.reset(
        new FakeBluetoothGattCharacteristicClient);
    bluetooth_gatt_descriptor_client_.reset(
        new FakeBluetoothGattDescriptorClient);
    bluetooth_gatt_manager_client_.reset(new FakeBluetoothGattManagerClient);
    bluetooth_gatt_service_client_.reset(new FakeBluetoothGattServiceClient);

    alternate_bluetooth_adapter_client_.reset(new FakeBluetoothAdapterClient);
    alternate_bluetooth_device_client_.reset(new FakeBluetoothDeviceClient);
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  }
}

BluetoothDBusClientBundle::~BluetoothDBusClientBundle() = default;

}  // namespace bluez
