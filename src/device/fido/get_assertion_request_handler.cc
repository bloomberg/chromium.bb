// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/pin.h"

namespace device {

namespace {

// PublicKeyUserEntity field in GetAssertion response is optional with the
// following constraints:
// - If assertion has been made without user verification, user identifiable
//   information must not be included.
// - For resident key credentials, user id of the user entity is mandatory.
// - When multiple accounts exist for specified RP ID, user entity is
//   mandatory.
// TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
// published.
bool CheckRequirementsOnResponseUserEntity(
    const CtapGetAssertionRequest& request,
    const AuthenticatorGetAssertionResponse& response) {
  // If assertion has been made without user verification, user identifiable
  // information must not be included.
  const auto& user_entity = response.user_entity();
  const bool has_user_identifying_info =
      user_entity && (user_entity->user_display_name() ||
                      user_entity->user_name() || user_entity->user_icon_url());
  if (!response.auth_data().obtained_user_verification() &&
      has_user_identifying_info) {
    return false;
  }

  // For resident key credentials, user id of the user entity is mandatory.
  if ((!request.allow_list() || request.allow_list()->empty()) &&
      !user_entity) {
    return false;
  }

  // When multiple accounts exist for specified RP ID, user entity is mandatory.
  if (response.num_credentials().value_or(0u) > 1 && !user_entity) {
    return false;
  }

  return true;
}

// Checks whether credential ID returned from the authenticator and transport
// type used matches the transport type and credential ID defined in
// PublicKeyCredentialDescriptor of the allowed list. If the device has resident
// key support, returned credential ID may be resident credential. Thus,
// returned credential ID need not be in allowed list.
// TODO(hongjunchoi) : Add link to section of the CTAP spec once it is
// published.
bool CheckResponseCredentialIdMatchesRequestAllowList(
    const FidoAuthenticator& authenticator,
    const CtapGetAssertionRequest request,
    const AuthenticatorGetAssertionResponse& response) {
  const auto& allow_list = request.allow_list();
  if (!allow_list || allow_list->empty()) {
    // Allow list can't be empty for authenticators w/o resident key support.
    return !authenticator.Options() ||
           authenticator.Options()->supports_resident_key;
  }
  // Credential ID may be omitted if allow list has size 1. Otherwise, it needs
  // to match.
  const auto opt_transport_used = authenticator.AuthenticatorTransport();
  return (allow_list->size() == 1 && !response.credential()) ||
         std::any_of(allow_list->cbegin(), allow_list->cend(),
                     [&response, opt_transport_used](const auto& credential) {
                       return credential.id() == response.raw_credential_id() &&
                              (!opt_transport_used ||
                               base::ContainsKey(credential.transports(),
                                                 *opt_transport_used));
                     });
}

// When the response from the authenticator does not contain a credential and
// the allow list from the GetAssertion request only contains a single
// credential id, manually set credential id in the returned response.
void SetCredentialIdForResponseWithEmptyCredential(
    const CtapGetAssertionRequest& request,
    AuthenticatorGetAssertionResponse& response) {
  if (request.allow_list() && request.allow_list()->size() == 1 &&
      !response.credential()) {
    response.SetCredential(request.allow_list()->at(0));
  }
}

// Checks UserVerificationRequirement enum passed from the relying party is
// compatible with the authenticator.
bool CheckUserVerificationCompatible(FidoAuthenticator* authenticator,
                                     const CtapGetAssertionRequest& request,
                                     bool have_observer) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  const bool pin_support =
      base::FeatureList::IsEnabled(device::kWebAuthPINSupport) && have_observer;
  return request.user_verification() !=
             UserVerificationRequirement::kRequired ||
         opt_options->user_verification_availability ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured ||
         (pin_support && opt_options->client_pin_availability ==
                             AuthenticatorSupportedOptions::
                                 ClientPinAvailability::kSupportedAndPinSet);
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const CtapGetAssertionRequest& request) {
  const base::flat_set<FidoTransportProtocol> kAllTransports = {
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};

  // TODO(https://crbug.com/874479): |allowed_list| will |has_value| even if the
  // WebAuthn request has `allowCredential` undefined.
  const auto& allowed_list = request.allow_list();
  if (!allowed_list || allowed_list->empty()) {
    return kAllTransports;
  }

  base::flat_set<FidoTransportProtocol> transports;
  for (const auto credential : *allowed_list) {
    if (credential.transports().empty())
      return kAllTransports;
    transports.insert(credential.transports().begin(),
                      credential.transports().end());
  }

  return transports;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedAndConfiguredByRP(
    const CtapGetAssertionRequest& request) {
  auto transports = GetTransportsAllowedByRP(request);
  if (!request.cable_extension())
    transports.erase(FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  return transports;
}

}  // namespace

GetAssertionRequestHandler::GetAssertionRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapGetAssertionRequest request,
    CompletionCallback completion_callback)
    : FidoRequestHandler(
          connector,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedAndConfiguredByRP(request)),
          std::move(completion_callback)),
      request_(std::move(request)),
      weak_factory_(this) {
  transport_availability_info().rp_id = request_.rp_id();
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kGetAssertion;

  if (base::ContainsKey(
          transport_availability_info().available_transports,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy)) {
    DCHECK(request_.cable_extension());
    auto discovery =
        FidoDiscoveryFactory::CreateCable(*request_.cable_extension());
    discovery->set_observer(this);
    discoveries().push_back(std::move(discovery));
  }

  Start();
}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

static bool WillNeedPIN(const CtapGetAssertionRequest& request,
                        const FidoAuthenticator* authenticator) {
  const auto& opt_options = authenticator->Options();
  if (request.user_verification() ==
          UserVerificationRequirement::kDiscouraged ||
      !opt_options ||
      opt_options->user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured) {
    return false;
  }

  return opt_options->client_pin_availability ==
         AuthenticatorSupportedOptions::ClientPinAvailability::
             kSupportedAndPinSet;
}

void GetAssertionRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !CheckUserVerificationCompatible(authenticator, request_, observer())) {
    return;
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthPINSupport) &&
      WillNeedPIN(request_, authenticator)) {
    CtapGetAssertionRequest request(request_);
    // Set empty pinAuth to trigger just a touch.
    request.SetPinAuth({});
    authenticator->GetAssertion(
        request, base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                                weak_factory_.GetWeakPtr(), authenticator));
  } else {
    authenticator->GetAssertion(
        request_, base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                                 weak_factory_.GetWeakPtr(), authenticator));
  }
}

void GetAssertionRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN ||
        state_ == State::kWaitingForSecondTouch) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(FidoReturnCode::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, base::nullopt);
    }
  }
}

void GetAssertionRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForSecondTouch) {
    return;
  }

  if (state_ == State::kWaitingForTouch &&
      base::FeatureList::IsEnabled(device::kWebAuthPINSupport) &&
      WillNeedPIN(request_, authenticator)) {
    if (response_code != CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid &&
        response_code != CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
      VLOG(1) << "Expected invalid pinAuth error but got "
              << static_cast<int>(response_code);
      return;
    }
    DCHECK(observer());
    CancelActiveAuthenticators(authenticator->GetId());
    state_ = State::kGettingRetries;
    authenticator_ = authenticator;
    authenticator_->GetRetries(
        base::BindOnce(&GetAssertionRequestHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  state_ = State::kFinished;
  if (response_code != CtapDeviceResponseCode::kSuccess) {
    OnAuthenticatorResponse(authenticator, response_code, base::nullopt);
    return;
  }

  if (!response || !request_.CheckResponseRpIdHash(response->GetRpIdHash()) ||
      !CheckResponseCredentialIdMatchesRequestAllowList(*authenticator,
                                                        request_, *response) ||
      !CheckRequirementsOnResponseUserEntity(request_, *response)) {
    OnAuthenticatorResponse(
        authenticator, CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  SetCredentialIdForResponseWithEmptyCredential(request_, *response);
  OnAuthenticatorResponse(authenticator, response_code, std::move(response));
}

void GetAssertionRequestHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);

  if (status == CtapDeviceResponseCode::kSuccess && !response) {
    status = CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode ret = FidoReturnCode::kAuthenticatorResponseInvalid;
    if (status == CtapDeviceResponseCode::kCtap2ErrPinBlocked) {
      ret = FidoReturnCode::kHardPINBlock;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, base::nullopt);
    return;
  }

  state_ = State::kWaitingForPIN;
  observer()->CollectPIN(response->retries,
                         base::BindOnce(&GetAssertionRequestHandler::OnHavePIN,
                                        weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    DCHECK(!completion_callback_);
    return;
  }

  state_ = State::kGetEphemeralKey;
  authenticator_->GetEphemeralKey(
      base::BindOnce(&GetAssertionRequestHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void GetAssertionRequestHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(State::kGetEphemeralKey, state_);

  if (status == CtapDeviceResponseCode::kSuccess && !response) {
    status = CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid, base::nullopt,
             base::nullopt);
    return;
  }

  state_ = State::kRequestWithPIN;
  authenticator_->GetPINToken(
      std::move(pin), *response,
      base::BindOnce(&GetAssertionRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void GetAssertionRequestHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&GetAssertionRequestHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status == CtapDeviceResponseCode::kSuccess && !response) {
    status = CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode ret;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        ret = FidoReturnCode::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        ret = FidoReturnCode::kHardPINBlock;
        break;
      default:
        ret = FidoReturnCode::kAuthenticatorResponseInvalid;
        break;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, base::nullopt);
    return;
  }

  observer()->FinishCollectPIN();
  state_ = State::kWaitingForSecondTouch;
  request_.SetPinAuth(response->PinAuth(request_.client_data_hash()));
  request_.SetPinProtocol(pin::kProtocolVersion);

  authenticator_->GetAssertion(
      request_, base::BindOnce(&GetAssertionRequestHandler::HandleResponse,
                               weak_factory_.GetWeakPtr(), authenticator_));
}

}  // namespace device
