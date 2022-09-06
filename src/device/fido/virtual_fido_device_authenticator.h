// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_AUTHENTICATOR_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_AUTHENTICATOR_H_

#include "device/fido/fido_device_authenticator.h"
#include "device/fido/virtual_fido_device.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualFidoDeviceAuthenticator
    : public FidoDeviceAuthenticator {
 public:
  explicit VirtualFidoDeviceAuthenticator(
      std::unique_ptr<VirtualFidoDevice> virtual_fido_device);

  VirtualFidoDeviceAuthenticator(const VirtualFidoDeviceAuthenticator&) =
      delete;
  VirtualFidoDeviceAuthenticator& operator=(
      const VirtualFidoDeviceAuthenticator&) = delete;

  ~VirtualFidoDeviceAuthenticator() override;

  // FidoDeviceAuthenticator:
  void GetCredentialInformationForRequest(
      const CtapGetAssertionRequest& request,
      GetCredentialInformationForRequestCallback callback) override;
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_AUTHENTICATOR_H_
