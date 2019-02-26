// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/make_credential_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

namespace {

bool CheckIfAuthenticatorSelectionCriteriaAreSatisfied(
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  using UvAvailability =
      AuthenticatorSupportedOptions::UserVerificationAvailability;

  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  if ((authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kPlatform &&
       !opt_options->is_platform_device()) ||
      (authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kCrossPlatform &&
       opt_options->is_platform_device()))
    return false;

  if (authenticator_selection_criteria.require_resident_key() &&
      !opt_options->supports_resident_key())
    return false;

  return authenticator_selection_criteria.user_verification_requirement() !=
             UserVerificationRequirement::kRequired ||
         opt_options->user_verification_availability() ==
             UvAvailability::kSupportedAndConfigured;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  const auto attachment_type =
      authenticator_selection_criteria.authenticator_attachment();
  switch (attachment_type) {
    case AuthenticatorAttachment::kPlatform:
      return {FidoTransportProtocol::kInternal};
    case AuthenticatorAttachment::kCrossPlatform:
      // Cloud-assisted BLE is not yet supported for MakeCredential requests.
      return {FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy,
              FidoTransportProtocol::kNearFieldCommunication};
    case AuthenticatorAttachment::kAny:
      // Cloud-assisted BLE is not yet supported for MakeCredential requests.
      return {FidoTransportProtocol::kInternal,
              FidoTransportProtocol::kNearFieldCommunication,
              FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy};
  }

  NOTREACHED();
  return base::flat_set<FidoTransportProtocol>();
}

}  // namespace

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapMakeCredentialRequest request,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    CompletionCallback completion_callback)
    : FidoRequestHandler(
          connector,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedByRP(authenticator_selection_criteria)),
          std::move(completion_callback)),
      request_parameter_(std::move(request)),
      authenticator_selection_criteria_(
          std::move(authenticator_selection_criteria)),
      weak_factory_(this) {
  transport_availability_info().rp_id = request_parameter_.rp().rp_id();
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kMakeCredential;
  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  if (!CheckIfAuthenticatorSelectionCriteriaAreSatisfied(
          authenticator, authenticator_selection_criteria_))
    return;

  // Set the rk, uv and attachment fields, which were only initialized to
  // default values up to here.  TODO(martinkr): Initialize these fields earlier
  // (in AuthenticatorImpl) and get rid of the separate
  // AuthenticatorSelectionCriteriaParameter.
  request_parameter_.SetResidentKeyRequired(
      authenticator_selection_criteria_.require_resident_key());
  request_parameter_.SetUserVerification(
      authenticator_selection_criteria_.user_verification_requirement());
  request_parameter_.SetAuthenticatorAttachment(
      authenticator_selection_criteria_.authenticator_attachment());

  authenticator->MakeCredential(
      request_parameter_,
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  if (response_code != CtapDeviceResponseCode::kSuccess) {
    OnAuthenticatorResponse(authenticator, response_code, base::nullopt);
    return;
  }

  const auto rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(request_parameter_.rp().rp_id());

  if (!response || response->GetRpIdHash() != rp_id_hash) {
    OnAuthenticatorResponse(
        authenticator, CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  OnAuthenticatorResponse(authenticator, response_code, std::move(response));
}

void MakeCredentialRequestHandler::SetPlatformAuthenticatorOrMarkUnavailable(
    base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info) {
  if (platform_authenticator_info) {
    // TODO(crbug.com/873710): In the case of a request with
    // AuthenticatorAttachment::kAny and when there is no embedder-provided
    // transport selection UI, disable the platform authenticator to avoid the
    // Touch ID fingerprint prompt competing with external devices.
    const bool has_transport_selection_ui =
        observer() && observer()->EmbedderControlsAuthenticatorDispatch(
                          *platform_authenticator_info->authenticator);
    if (authenticator_selection_criteria_.authenticator_attachment() ==
            AuthenticatorAttachment::kAny &&
        !has_transport_selection_ui) {
      platform_authenticator_info = base::nullopt;
    }
  }

  FidoRequestHandlerBase::SetPlatformAuthenticatorOrMarkUnavailable(
      std::move(platform_authenticator_info));
}

}  // namespace device
