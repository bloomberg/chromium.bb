// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

class CtapGetAssertionRequest;
class CtapMakeCredentialRequest;
class FidoDevice;
class FidoTask;

namespace pin {
struct RetriesRequest;
struct RetriesResponse;
}  // namespace pin

// Adaptor class from a |FidoDevice| to the |FidoAuthenticator| interface.
// Responsible for translating WebAuthn-level requests into serializations that
// can be passed to the device for transport.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDeviceAuthenticator
    : public FidoAuthenticator {
 public:
  FidoDeviceAuthenticator(std::unique_ptr<FidoDevice> device);
  ~FidoDeviceAuthenticator() override;

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    GetAssertionCallback callback) override;
  void GetNextAssertion(GetAssertionCallback callback) override;
  void GetTouch(base::OnceCallback<void()> callback) override;
  void GetRetries(GetRetriesCallback callback) override;
  void GetEphemeralKey(GetEphemeralKeyCallback callback) override;
  void GetPINToken(std::string pin,
                   const pin::KeyAgreementResponse& peer_key,
                   GetPINTokenCallback callback) override;
  void SetPIN(const std::string& pin,
              const pin::KeyAgreementResponse& peer_key,
              SetPINCallback callback) override;
  void ChangePIN(const std::string& old_pin,
                 const std::string& new_pin,
                 pin::KeyAgreementResponse& peer_key,
                 SetPINCallback callback) override;
  MakeCredentialPINDisposition WillNeedPINToMakeCredential(
      const CtapMakeCredentialRequest& request,
      const FidoRequestHandlerBase::Observer* observer) override;

  // WillNeedPINToGetAssertion returns whether a PIN prompt will be needed to
  // serve the given request on this authenticator.
  GetAssertionPINDisposition WillNeedPINToGetAssertion(
      const CtapGetAssertionRequest& request,
      const FidoRequestHandlerBase::Observer* observer) override;

  void Reset(ResetCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
  ProtocolVersion SupportedProtocol() const override;
  const base::Optional<AuthenticatorSupportedOptions>& Options() const override;
  base::Optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  bool IsInPairingMode() const override;
  bool IsPaired() const override;
#if defined(OS_WIN)
  bool IsWinNativeApiAuthenticator() const override;
#endif  // defined(OS_WIN)
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  FidoDevice* device() { return device_.get(); }
  void SetTaskForTesting(std::unique_ptr<FidoTask> task);

 protected:
  void OnCtapMakeCredentialResponseReceived(
      MakeCredentialCallback callback,
      base::Optional<std::vector<uint8_t>> response_data);
  void OnCtapGetAssertionResponseReceived(
      GetAssertionCallback callback,
      base::Optional<std::vector<uint8_t>> response_data);

 private:
  void InitializeAuthenticatorDone(base::OnceClosure callback);

  const std::unique_ptr<FidoDevice> device_;
  base::Optional<AuthenticatorSupportedOptions> options_;
  std::unique_ptr<FidoTask> task_;
  std::unique_ptr<GenericDeviceOperation> operation_;
  base::WeakPtrFactory<FidoDeviceAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoDeviceAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
