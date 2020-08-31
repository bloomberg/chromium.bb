// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/connection_attempt.h"
#include "chromeos/services/secure_channel/connection_attempt_delegate.h"
#include "chromeos/services/secure_channel/connection_medium.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/pending_connection_manager.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {

namespace secure_channel {

class BleConnectionManager;

// Concrete PendingConnectionManager implementation. This class creates one
// ConnectionAttempt per ConnectionAttemptDetails requested; if more than one
// request shares the same ConnectionAttemptDetails, a single ConnectionAttempt
// attempts a connection for all associated requests.
//
// If a ConnectionAttempt successfully creates a channel, this class extracts
// client data from all requests to the same remote device and alerts its
// delegate, deleting all associated ConnectionAttempts when it is finished.
class PendingConnectionManagerImpl : public PendingConnectionManager,
                                     public ConnectionAttemptDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<PendingConnectionManager> Create(
        Delegate* delegate,
        BleConnectionManager* ble_connection_manager,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionManager> CreateInstance(
        Delegate* delegate,
        BleConnectionManager* ble_connection_manager,
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) = 0;

   private:
    static Factory* test_factory_;
  };

  ~PendingConnectionManagerImpl() override;

 private:
  PendingConnectionManagerImpl(
      Delegate* delegate,
      BleConnectionManager* ble_connection_manager,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);

  // PendingConnectionManager:
  void HandleConnectionRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority) override;

  // ConnectionAttemptDelegate:
  void OnConnectionAttemptSucceeded(
      const ConnectionDetails& connection_details,
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) override;
  void OnConnectionAttemptFinishedWithoutConnection(
      const ConnectionAttemptDetails& connection_attempt_details) override;

  void HandleBleInitiatorRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);
  void HandleBleListenerRequest(
      const ConnectionAttemptDetails& connection_attempt_details,
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority);

  void RemoveMapEntriesForFinishedConnectionAttempt(
      const ConnectionAttemptDetails& connection_attempt_details);

  base::flat_map<DeviceIdPair,
                 std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>>>
      id_pair_to_ble_initiator_connection_attempts_;

  base::flat_map<DeviceIdPair,
                 std::unique_ptr<ConnectionAttempt<BleListenerFailureType>>>
      id_pair_to_ble_listener_connection_attempts_;

  base::flat_map<ConnectionDetails, base::flat_set<ConnectionAttemptDetails>>
      details_to_attempt_details_map_;

  BleConnectionManager* ble_connection_manager_;
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  DISALLOW_COPY_AND_ASSIGN(PendingConnectionManagerImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_MANAGER_IMPL_H_
