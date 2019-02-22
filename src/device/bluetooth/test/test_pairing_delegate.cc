// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/test_pairing_delegate.h"

namespace device {

TestPairingDelegate::TestPairingDelegate() = default;

TestPairingDelegate::~TestPairingDelegate() = default;

void TestPairingDelegate::RequestPinCode(BluetoothDevice* device) {
  ++call_count_;
  ++request_pincode_count_;
  QuitMessageLoop();
}

void TestPairingDelegate::RequestPasskey(BluetoothDevice* device) {
  ++call_count_;
  ++request_passkey_count_;
  QuitMessageLoop();
}

void TestPairingDelegate::DisplayPinCode(BluetoothDevice* device,
                                         const std::string& pincode) {
  ++call_count_;
  ++display_pincode_count_;
  last_pincode_ = pincode;
  QuitMessageLoop();
}

void TestPairingDelegate::DisplayPasskey(BluetoothDevice* device,
                                         uint32_t passkey) {
  ++call_count_;
  ++display_passkey_count_;
  last_passkey_ = passkey;
  QuitMessageLoop();
}

void TestPairingDelegate::KeysEntered(BluetoothDevice* device,
                                      uint32_t entered) {
  ++call_count_;
  ++keys_entered_count_;
  last_entered_ = entered;
  QuitMessageLoop();
}

void TestPairingDelegate::ConfirmPasskey(BluetoothDevice* device,
                                         uint32_t passkey) {
  ++call_count_;
  ++confirm_passkey_count_;
  last_passkey_ = passkey;
  QuitMessageLoop();
}

void TestPairingDelegate::AuthorizePairing(BluetoothDevice* device) {
  ++call_count_;
  ++authorize_pairing_count_;
  QuitMessageLoop();
}

void TestPairingDelegate::QuitMessageLoop() {
  if (base::RunLoop::IsRunningOnCurrentThread())
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace device
