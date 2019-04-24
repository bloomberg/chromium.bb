// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_CONNECTION_REQUEST_BASE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_CONNECTION_REQUEST_BASE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chromeos/services/secure_channel/pending_connection_request_base.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {

namespace secure_channel {

template <typename BleFailureDetailType>
class PendingBleConnectionRequestBase
    : public PendingConnectionRequestBase<BleFailureDetailType>,
      public device::BluetoothAdapter::Observer {
 public:
  ~PendingBleConnectionRequestBase() override {
    bluetooth_adapter_->RemoveObserver(this);
  }

 protected:
  PendingBleConnectionRequestBase(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      const std::string& readable_request_type_for_logging,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
      : PendingConnectionRequestBase<BleFailureDetailType>(
            std::move(client_connection_parameters),
            connection_priority,
            readable_request_type_for_logging,
            delegate),
        bluetooth_adapter_(std::move(bluetooth_adapter)) {
    bluetooth_adapter_->AddObserver(this);
  }

 private:
  friend class SecureChannelPendingBleConnectionRequestBaseTest;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override {
    DCHECK_EQ(bluetooth_adapter_, adapter);
    if (powered)
      return;

    this->StopRequestDueToConnectionFailures(
        mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED);
  }

  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override {
    DCHECK_EQ(bluetooth_adapter_, adapter);
    if (present)
      return;

    this->StopRequestDueToConnectionFailures(
        mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT);
  }

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  DISALLOW_COPY_AND_ASSIGN(PendingBleConnectionRequestBase);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_CONNECTION_REQUEST_BASE_H_
