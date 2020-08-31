// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_LISTENER_CONNECTION_ATTEMPT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_LISTENER_CONNECTION_ATTEMPT_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/services/secure_channel/connection_attempt_base.h"

namespace chromeos {

namespace secure_channel {

class BleConnectionManager;

// Attempts to connect to a remote device over BLE via the listener role.
class BleListenerConnectionAttempt
    : public ConnectionAttemptBase<BleListenerFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectionAttempt<BleListenerFailureType>> Create(
        BleConnectionManager* ble_connection_manager,
        ConnectionAttemptDelegate* delegate,
        const ConnectionAttemptDetails& connection_attempt_details);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ConnectionAttempt<BleListenerFailureType>>
    CreateInstance(
        BleConnectionManager* ble_connection_manager,
        ConnectionAttemptDelegate* delegate,
        const ConnectionAttemptDetails& connection_attempt_details) = 0;

   private:
    static Factory* test_factory_;
  };

  ~BleListenerConnectionAttempt() override;

 private:
  BleListenerConnectionAttempt(
      BleConnectionManager* ble_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details);

  // ConnectionAttemptBase<BleListenerFailureType>:
  std::unique_ptr<ConnectToDeviceOperation<BleListenerFailureType>>
  CreateConnectToDeviceOperation(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectToDeviceOperation<
          BleListenerFailureType>::ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<BleListenerFailureType>::
          ConnectionFailedCallback& failure_callback) override;

  // ConnectionAttempt<BleListenerFailureType>:
  void ProcessSuccessfulConnectionDuration(
      const base::TimeDelta& duration) override;

  BleConnectionManager* ble_connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(BleListenerConnectionAttempt);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_LISTENER_CONNECTION_ATTEMPT_H_
