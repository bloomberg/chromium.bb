// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_CONNECTION_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_CONNECTION_MANAGER_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_connection_manager.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

// Test BleConnectionManager implementation. Each of the overridden functions
// has an empty body.
class FakeBleConnectionManager : public BleConnectionManager {
 public:
  FakeBleConnectionManager();
  ~FakeBleConnectionManager() override;

  // Make public for testing.
  using BleConnectionManager::GetPriorityForAttempt;
  using BleConnectionManager::GetDetailsForRemoteDevice;
  using BleConnectionManager::DoesAttemptExist;
  using BleConnectionManager::NotifyBleInitiatorFailure;
  using BleConnectionManager::NotifyBleListenerFailure;
  using BleConnectionManager::NotifyConnectionSuccess;

 private:
  // BleConnectionManager:
  void PerformAttemptBleInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
  void PerformAttemptBleListenerConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleListenerConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;

  DISALLOW_COPY_AND_ASSIGN(FakeBleConnectionManager);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_CONNECTION_MANAGER_H_
