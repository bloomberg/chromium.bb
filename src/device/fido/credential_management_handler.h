// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_
#define DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/pin.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

// CredentialManagementHandler implements the authenticatorCredentialManagement
// protocol.
//
// Public methods on instances of this class may be called only after
// ReadyCallback has run, but not after FinishedCallback has run.
class COMPONENT_EXPORT(DEVICE_FIDO) CredentialManagementHandler
    : public FidoRequestHandlerBase {
 public:
  using DeleteCredentialCallback =
      base::OnceCallback<void(CtapDeviceResponseCode)>;
  using FinishedCallback = base::OnceCallback<void(FidoReturnCode)>;
  using GetCredentialsCallback = base::OnceCallback<void(
      CtapDeviceResponseCode status,
      base::Optional<std::vector<AggregatedEnumerateCredentialsResponse>>,
      base::Optional<size_t>)>;
  using GetPINCallback =
      base::RepeatingCallback<void(int64_t,
                                   base::OnceCallback<void(std::string)>)>;
  using ReadyCallback = base::OnceClosure;

  CredentialManagementHandler(
      service_manager::Connector* connector,
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      ReadyCallback ready_callback,
      GetPINCallback get_pin_callback,
      FinishedCallback finished_callback);
  ~CredentialManagementHandler() override;

  // GetCredentials invokes a series of commands to fetch all credentials stored
  // on the device. The supplied callback receives the status returned by the
  // device and, if successful, the resident credentials stored and remaining
  // capacity left on the chosen authenticator.
  void GetCredentials(GetCredentialsCallback callback);
  void DeleteCredential(base::span<const uint8_t> credential_id,
                        DeleteCredentialCallback callback);

 private:
  enum class State {
    kWaitingForTouch,
    kGettingRetries,
    kWaitingForPIN,
    kGettingEphemeralKey,
    kGettingPINToken,
    kReady,
    kGettingMetadata,
    kGettingRP,
    kGettingCredentials,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         base::Optional<pin::RetriesResponse> response);
  void OnHavePIN(std::string pin);
  void OnHaveEphemeralKey(std::string pin,
                          CtapDeviceResponseCode status,
                          base::Optional<pin::KeyAgreementResponse> response);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);
  void OnCredentialsMetadata(
      CtapDeviceResponseCode status,
      base::Optional<CredentialsMetadataResponse> response);
  void OnEnumerateCredentials(
      CredentialsMetadataResponse metadata_response,
      CtapDeviceResponseCode status,
      base::Optional<std::vector<AggregatedEnumerateCredentialsResponse>>
          responses);

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ = State::kWaitingForTouch;
  FidoAuthenticator* authenticator_ = nullptr;
  base::Optional<std::vector<uint8_t>> pin_token_;

  ReadyCallback ready_callback_;
  GetPINCallback get_pin_callback_;
  GetCredentialsCallback get_credentials_callback_;
  FinishedCallback finished_callback_;

  base::WeakPtrFactory<CredentialManagementHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagementHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_
