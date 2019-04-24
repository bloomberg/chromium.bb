// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

using ClientPinAvailability =
    AuthenticatorSupportedOptions::ClientPinAvailability;

namespace {

bool CheckIfAuthenticatorSelectionCriteriaAreSatisfied(
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria,
    bool have_observer) {
  using UvAvailability =
      AuthenticatorSupportedOptions::UserVerificationAvailability;
  const bool pin_support =
      base::FeatureList::IsEnabled(device::kWebAuthPINSupport) && have_observer;

  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  if ((authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kPlatform &&
       !opt_options->is_platform_device) ||
      (authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kCrossPlatform &&
       opt_options->is_platform_device)) {
    return false;
  }

  if (authenticator_selection_criteria.require_resident_key() &&
      !opt_options->supports_resident_key) {
    return false;
  }

  if (authenticator_selection_criteria.user_verification_requirement() ==
          UserVerificationRequirement::kRequired &&
      opt_options->user_verification_availability !=
          UvAvailability::kSupportedAndConfigured &&
      (!pin_support || opt_options->client_pin_availability ==
                           ClientPinAvailability::kNotSupported)) {
    return false;
  }

  return true;
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

// WillNeedPIN returns what type of PIN intervention will be needed to serve the
// given request on the given authenticator.
//   |kNotSupported|: no PIN involved.
//   |kSupportedButPinNotSet|: will need to set a new PIN.
//   |kSupportedAndPinSet|: will need to prompt for an existing PIN.
static ClientPinAvailability WillNeedPIN(
    const CtapMakeCredentialRequest& request,
    const FidoAuthenticator* authenticator) {
  const auto& opt_options = authenticator->Options();
  if (request.user_verification() != UserVerificationRequirement::kRequired ||
      !opt_options ||
      opt_options->user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured) {
    return ClientPinAvailability::kNotSupported;
  }

  return opt_options->client_pin_availability;
}

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !CheckIfAuthenticatorSelectionCriteriaAreSatisfied(
          authenticator, authenticator_selection_criteria_, observer())) {
    return;
  }

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

  if (base::FeatureList::IsEnabled(device::kWebAuthPINSupport) &&
      WillNeedPIN(request_parameter_, authenticator) !=
          ClientPinAvailability::kNotSupported) {
    // Set an empty pinAuth parameter to wait for a touch and then report an
    // error.
    CtapMakeCredentialRequest request(request_parameter_);
    request.SetPinAuth({});
    authenticator->MakeCredential(
        request, base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                                weak_factory_.GetWeakPtr(), authenticator));
  } else {
    authenticator->MakeCredential(
        request_parameter_,
        base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                       weak_factory_.GetWeakPtr(), authenticator));
  }
}

void MakeCredentialRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN || state_ == State::kWaitingForNewPIN ||
        state_ == State::kWaitingForSecondTouch) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(FidoReturnCode::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, base::nullopt);
    }
  }
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForSecondTouch) {
    return;
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthPINSupport) &&
      state_ == State::kWaitingForTouch) {
    switch (WillNeedPIN(request_parameter_, authenticator)) {
      case ClientPinAvailability::kSupportedAndPinSet:
        // Will need to get PIN to handle this request.
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
            base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                           weak_factory_.GetWeakPtr()));
        return;

      case ClientPinAvailability::kSupportedButPinNotSet:
        // Will need to set a PIN to handle this request.
        if (response_code != CtapDeviceResponseCode::kCtap2ErrPinNotSet) {
          VLOG(1) << "Expected PIN-not-set error but got "
                  << static_cast<int>(response_code);
          return;
        }
        DCHECK(observer());
        CancelActiveAuthenticators(authenticator->GetId());
        state_ = State::kWaitingForNewPIN;
        authenticator_ = authenticator;
        observer()->CollectPIN(
            base::nullopt,
            base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                           weak_factory_.GetWeakPtr()));
        return;

      case ClientPinAvailability::kNotSupported:
        // No PIN needed for this request.
        break;
    }
  }

  state_ = State::kFinished;
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

void MakeCredentialRequestHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(state_ == State::kWaitingForPIN || state_ == State::kWaitingForNewPIN);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    DCHECK(!completion_callback_);
    return;
  }

  if (state_ == State::kWaitingForPIN) {
    state_ = State::kGetEphemeralKey;
  } else {
    DCHECK_EQ(state_, State::kWaitingForNewPIN);
    state_ = State::kGetEphemeralKeyForNewPIN;
  }

  authenticator_->GetEphemeralKey(
      base::BindOnce(&MakeCredentialRequestHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void MakeCredentialRequestHandler::OnRetriesResponse(
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
  observer()->CollectPIN(
      response->retries,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialRequestHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(state_ == State::kGetEphemeralKey ||
         state_ == State::kGetEphemeralKeyForNewPIN);

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

  if (state_ == State::kGetEphemeralKey) {
    state_ = State::kRequestWithPIN;
    authenticator_->GetPINToken(
        std::move(pin), *response,
        base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                       weak_factory_.GetWeakPtr()));
  } else {
    DCHECK_EQ(state_, State::kGetEphemeralKeyForNewPIN);
    state_ = State::kSettingPIN;
    authenticator_->SetPIN(
        pin, *response,
        base::BindOnce(&MakeCredentialRequestHandler::OnHaveSetPIN,
                       weak_factory_.GetWeakPtr(), pin, *response));
  }
}

void MakeCredentialRequestHandler::OnHaveSetPIN(
    std::string pin,
    pin::KeyAgreementResponse key_agreement,
    CtapDeviceResponseCode status,
    base::Optional<pin::EmptyResponse> response) {
  DCHECK_EQ(state_, State::kSettingPIN);

  // Having just set the PIN, we need to immediately turn around and use it to
  // get a PIN token.
  state_ = State::kRequestWithPIN;
  authenticator_->GetPINToken(
      std::move(pin), key_agreement,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialRequestHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
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
  request_parameter_.SetPinAuth(
      response->PinAuth(request_parameter_.client_data_hash()));
  request_parameter_.SetPinProtocol(pin::kProtocolVersion);

  authenticator_->MakeCredential(
      request_parameter_,
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator_));
}

void MakeCredentialRequestHandler::SetPlatformAuthenticatorOrMarkUnavailable(
    base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

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
