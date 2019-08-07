// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_pairing_requested_event_args_winrt.h"

#include <windows.foundation.h>

#include <utility>

#include "base/win/scoped_hstring.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds_ProvidePin;
using ABI::Windows::Foundation::IDeferral;
using Microsoft::WRL::Make;
using Microsoft::WRL::ComPtr;

class FakeDeferral
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Foundation::IDeferral> {
 public:
  explicit FakeDeferral(
      ComPtr<FakeDevicePairingRequestedEventArgsWinrt> pairing_requested)
      : pairing_requested_(std::move(pairing_requested)) {}
  ~FakeDeferral() override = default;

  // IDeferral:
  IFACEMETHODIMP Complete() override {
    pairing_requested_->Complete();
    return S_OK;
  }

 private:
  ComPtr<FakeDevicePairingRequestedEventArgsWinrt> pairing_requested_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeferral);
};

}  // namespace

FakeDevicePairingRequestedEventArgsWinrt::
    FakeDevicePairingRequestedEventArgsWinrt(
        ComPtr<FakeDeviceInformationCustomPairingWinrt> custom_pairing)
    : custom_pairing_(std::move(custom_pairing)) {}

FakeDevicePairingRequestedEventArgsWinrt::
    ~FakeDevicePairingRequestedEventArgsWinrt() = default;

HRESULT FakeDevicePairingRequestedEventArgsWinrt::get_DeviceInformation(
    IDeviceInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::get_PairingKind(
    DevicePairingKinds* value) {
  *value = DevicePairingKinds_ProvidePin;
  return S_OK;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::get_Pin(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::Accept() {
  return E_NOTIMPL;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::AcceptWithPin(HSTRING pin) {
  custom_pairing_->AcceptWithPin(base::win::ScopedHString(pin).GetAsUTF8());
  return S_OK;
}

HRESULT FakeDevicePairingRequestedEventArgsWinrt::GetDeferral(
    IDeferral** result) {
  return Make<FakeDeferral>(this).CopyTo(result);
}

void FakeDevicePairingRequestedEventArgsWinrt::Complete() {
  custom_pairing_->Complete();
}

}  // namespace device
