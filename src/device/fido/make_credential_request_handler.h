// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class FidoAuthenticator;
class AuthenticatorMakeCredentialResponse;

class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialRequestHandler
    : public FidoRequestHandler<AuthenticatorMakeCredentialResponse> {
 public:
  MakeCredentialRequestHandler(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapMakeCredentialRequest request_parameter,
      AuthenticatorSelectionCriteria authenticator_criteria,
      CompletionCallback completion_callback);
  ~MakeCredentialRequestHandler() override;

  // FidoRequestHandlerBase:
  void SetPlatformAuthenticatorOrMarkUnavailable(
      base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info)
      override;

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForSecondTouch,
    kGettingRetries,
    kWaitingForPIN,
    kWaitingForNewPIN,
    kGetEphemeralKey,
    kGetEphemeralKeyForNewPIN,
    kSettingPIN,
    kRequestWithPIN,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void HandleResponse(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response);
  void HandleTouch(FidoAuthenticator* authenticator);
  void HandleInapplicableAuthenticator(FidoAuthenticator* authenticator);
  void OnHavePIN(std::string pin);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         base::Optional<pin::RetriesResponse> response);
  void OnHaveEphemeralKey(std::string pin,
                          CtapDeviceResponseCode status,
                          base::Optional<pin::KeyAgreementResponse> response);
  void OnHaveSetPIN(std::string pin,
                    pin::KeyAgreementResponse key_agreement,
                    CtapDeviceResponseCode status,
                    base::Optional<pin::EmptyResponse> response);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);

  State state_ = State::kWaitingForTouch;
  CtapMakeCredentialRequest request_;
  AuthenticatorSelectionCriteria authenticator_selection_criteria_;
  // authenticator_ points to the authenticator that will be used for this
  // operation. It's only set after the user touches an authenticator to select
  // it, after which point that authenticator will be used exclusively through
  // requesting PIN etc. The object is owned by the underlying discovery object
  // and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* authenticator_ = nullptr;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<MakeCredentialRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
