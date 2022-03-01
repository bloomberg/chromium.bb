// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_PAIRING_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_PAIRING_FLOSS_H_

#include <stdint.h>

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"

namespace floss {

// The BluetoothPairingFloss class represents an ongoing pairing process with a
// remote device.
class BluetoothPairingFloss {
 public:
  enum PairingExpectation {
    kNone,
    kPinCode,
    kPasskey,
    kConfirmation,
  };

  explicit BluetoothPairingFloss(
      device::BluetoothDevice::PairingDelegate* pairing_delegate);

  BluetoothPairingFloss(const BluetoothPairingFloss&) = delete;
  BluetoothPairingFloss& operator=(const BluetoothPairingFloss&) = delete;

  ~BluetoothPairingFloss();

  // Returns the pairing delegate being used by this pairing object.
  device::BluetoothDevice::PairingDelegate* pairing_delegate() const {
    return pairing_delegate_;
  }

  // Returns the type of the expected response from the local user.
  PairingExpectation pairing_expectation() const {
    return pairing_expectation_;
  }

  // Sets the type of the expected response from the local user.
  void SetPairingExpectation(PairingExpectation expectation) {
    pairing_expectation_ = expectation;
  }

 private:
  // UI Pairing Delegate to make method calls on, this must live as long as
  // the object capturing the PairingContext.
  device::BluetoothDevice::PairingDelegate* pairing_delegate_;

  // If pending user interaction, what is expected from the user.
  PairingExpectation pairing_expectation_ = PairingExpectation::kNone;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_PAIRING_FLOSS_H_
