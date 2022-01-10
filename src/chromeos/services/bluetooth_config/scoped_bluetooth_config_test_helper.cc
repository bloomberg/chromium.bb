// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"

#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_device_status_notifier.h"
#include "chromeos/services/bluetooth_config/fake_bluetooth_power_controller.h"
#include "chromeos/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/services/bluetooth_config/fake_device_name_manager.h"
#include "chromeos/services/bluetooth_config/fake_device_operation_handler.h"
#include "chromeos/services/bluetooth_config/fake_discovery_session_manager.h"
#include "chromeos/services/bluetooth_config/in_process_instance.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace bluetooth_config {

ScopedBluetoothConfigTestHelper::ScopedBluetoothConfigTestHelper() {
  OverrideInProcessInstanceForTesting(/*initializer=*/this);
}

ScopedBluetoothConfigTestHelper::~ScopedBluetoothConfigTestHelper() {
  OverrideInProcessInstanceForTesting(/*initializer=*/nullptr);
}

std::unique_ptr<AdapterStateController>
ScopedBluetoothConfigTestHelper::CreateAdapterStateController(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_adapter_state_controller =
      std::make_unique<FakeAdapterStateController>();
  fake_adapter_state_controller_ = fake_adapter_state_controller.get();
  return fake_adapter_state_controller;
}

std::unique_ptr<BluetoothDeviceStatusNotifier>
ScopedBluetoothConfigTestHelper::CreateBluetoothDeviceStatusNotifier(
    DeviceCache* device_cache) {
  auto fake_bluetooth_device_status_notifier =
      std::make_unique<FakeBluetoothDeviceStatusNotifier>();
  fake_bluetooth_device_status_notifier_ =
      fake_bluetooth_device_status_notifier.get();
  return fake_bluetooth_device_status_notifier;
}

std::unique_ptr<BluetoothPowerController>
ScopedBluetoothConfigTestHelper::CreateBluetoothPowerController(
    AdapterStateController* adapter_state_controller) {
  auto fake_bluetooth_power_controller =
      std::make_unique<FakeBluetoothPowerController>(adapter_state_controller);
  fake_bluetooth_power_controller_ = fake_bluetooth_power_controller.get();
  return fake_bluetooth_power_controller;
}

std::unique_ptr<DeviceNameManager>
ScopedBluetoothConfigTestHelper::CreateDeviceNameManager(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_device_name_manager = std::make_unique<FakeDeviceNameManager>();
  fake_device_name_manager_ = fake_device_name_manager.get();
  return fake_device_name_manager;
}

std::unique_ptr<DeviceCache> ScopedBluetoothConfigTestHelper::CreateDeviceCache(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceNameManager* device_name_manager) {
  auto fake_device_cache =
      std::make_unique<FakeDeviceCache>(adapter_state_controller);
  fake_device_cache_ = fake_device_cache.get();
  return fake_device_cache;
}

std::unique_ptr<DiscoverySessionManager>
ScopedBluetoothConfigTestHelper::CreateDiscoverySessionManager(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    DeviceCache* device_cache) {
  auto fake_discovery_session_manager =
      std::make_unique<FakeDiscoverySessionManager>(adapter_state_controller,
                                                    device_cache);
  fake_discovery_session_manager_ = fake_discovery_session_manager.get();
  return fake_discovery_session_manager;
}

std::unique_ptr<DeviceOperationHandler>
ScopedBluetoothConfigTestHelper::CreateDeviceOperationHandler(
    AdapterStateController* adapter_state_controller,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  auto fake_device_operation_handler =
      std::make_unique<FakeDeviceOperationHandler>(adapter_state_controller);
  fake_device_operation_handler_ = fake_device_operation_handler.get();
  return fake_device_operation_handler;
}

}  // namespace bluetooth_config
}  // namespace chromeos
