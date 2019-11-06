// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_ble_connection_manager.h"

namespace chromeos {

namespace secure_channel {

FakeBleConnectionManager::FakeBleConnectionManager() = default;

FakeBleConnectionManager::~FakeBleConnectionManager() = default;

void FakeBleConnectionManager::PerformAttemptBleInitiatorConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {}

void FakeBleConnectionManager::PerformUpdateBleInitiatorConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {}

void FakeBleConnectionManager::PerformCancelBleInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {}

void FakeBleConnectionManager::PerformAttemptBleListenerConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {}

void FakeBleConnectionManager::PerformUpdateBleListenerConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {}

void FakeBleConnectionManager::PerformCancelBleListenerConnectionAttempt(
    const DeviceIdPair& device_id_pair) {}

}  // namespace secure_channel

}  // namespace chromeos
