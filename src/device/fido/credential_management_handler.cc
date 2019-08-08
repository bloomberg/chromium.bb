// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

CredentialManagementHandler::CredentialManagementHandler(
    service_manager::Connector* connector,
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    ReadyCallback ready_callback,
    GetPINCallback get_pin_callback,
    FinishedCallback finished_callback)
    : FidoRequestHandlerBase(connector,
                             fido_discovery_factory,
                             supported_transports),
      ready_callback_(std::move(ready_callback)),
      get_pin_callback_(std::move(get_pin_callback)),
      finished_callback_(std::move(finished_callback)),
      weak_factory_(this) {
  Start();
}

CredentialManagementHandler::~CredentialManagementHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CredentialManagementHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForTouch);
  authenticator->GetTouch(base::BindOnce(&CredentialManagementHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void CredentialManagementHandler::OnTouch(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  state_ = State::kGettingRetries;
  CancelActiveAuthenticators(authenticator->GetId());

  if (authenticator->SupportedProtocol() != ProtocolVersion::kCtap2 ||
      !authenticator->Options() ||
      !(authenticator->Options()->supports_credential_management ||
        authenticator->Options()->supports_credential_management_preview)) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(FidoReturnCode::kAuthenticatorMissingCredentialManagement);
    return;
  }

  if (authenticator->Options()->client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet) {
    // The authenticator doesn't have a PIN/UV set up or doesn't support PINs.
    // We should implement in-flow PIN setting, but for now just tell the user
    // to set a PIN themselves.
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(FidoReturnCode::kAuthenticatorMissingUserVerification);
    return;
  }

  authenticator_ = authenticator;
  authenticator_->GetRetries(
      base::BindOnce(&CredentialManagementHandler::OnRetriesResponse,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }
  if (response->retries == 0) {
    state_ = State::kFinished;
    std::move(finished_callback_).Run(FidoReturnCode::kHardPINBlock);
    return;
  }
  state_ = State::kWaitingForPIN;
  get_pin_callback_.Run(response->retries,
                        base::BindOnce(&CredentialManagementHandler::OnHavePIN,
                                       weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    return;
  }

  state_ = State::kGettingEphemeralKey;
  authenticator_->GetEphemeralKey(
      base::BindOnce(&CredentialManagementHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void CredentialManagementHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kGettingEphemeralKey, state_);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }

  state_ = State::kGettingPINToken;
  authenticator_->GetPINToken(
      std::move(pin), *response,
      base::BindOnce(&CredentialManagementHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingPINToken);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&CredentialManagementHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode error;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        error = FidoReturnCode::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        error = FidoReturnCode::kHardPINBlock;
        break;
      default:
        error = FidoReturnCode::kAuthenticatorResponseInvalid;
        break;
    }
    std::move(finished_callback_).Run(error);
    return;
  }

  state_ = State::kReady;
  pin_token_ = response->token();
  std::move(ready_callback_).Run();
}

void CredentialManagementHandler::GetCredentials(
    GetCredentialsCallback callback) {
  DCHECK(state_ == State::kReady && !get_credentials_callback_);
  if (!authenticator_) {
    // AuthenticatorRemoved() may have been called, but the observer would have
    // seen a FidoAuthenticatorRemoved() call.
    NOTREACHED();
    return;
  }
  get_credentials_callback_ = std::move(callback);
  state_ = State::kGettingCredentials;
  authenticator_->GetCredentialsMetadata(
      *pin_token_,
      base::BindOnce(&CredentialManagementHandler::OnCredentialsMetadata,
                     weak_factory_.GetWeakPtr()));
}

static void OnDeleteCredential(
    CredentialManagementHandler::DeleteCredentialCallback callback,
    CtapDeviceResponseCode status,
    base::Optional<DeleteCredentialResponse> response) {
  std::move(callback).Run(status);
}

void CredentialManagementHandler::DeleteCredential(
    base::span<const uint8_t> credential_id,
    DeleteCredentialCallback callback) {
  DCHECK(state_ == State::kReady && !get_credentials_callback_);
  if (!authenticator_) {
    // AuthenticatorRemoved() may have been called, but the observer would have
    // seen a FidoAuthenticatorRemoved() call.
    NOTREACHED();
    return;
  }
  DCHECK(pin_token_);
  authenticator_->DeleteCredential(
      *pin_token_, credential_id,
      base::BindOnce(&OnDeleteCredential, std::move(callback)));
}

void CredentialManagementHandler::OnCredentialsMetadata(
    CtapDeviceResponseCode status,
    base::Optional<CredentialsMetadataResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(get_credentials_callback_)
        .Run(status, base::nullopt, base::nullopt);
    return;
  }
  authenticator_->EnumerateCredentials(
      *pin_token_,
      base::BindOnce(&CredentialManagementHandler::OnEnumerateCredentials,
                     weak_factory_.GetWeakPtr(), std::move(*response)));
}

void CredentialManagementHandler::OnEnumerateCredentials(
    CredentialsMetadataResponse metadata_response,
    CtapDeviceResponseCode status,
    base::Optional<std::vector<AggregatedEnumerateCredentialsResponse>>
        responses) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(get_credentials_callback_)
        .Run(status, base::nullopt, base::nullopt);
    return;
  }
  state_ = State::kReady;
  std::move(get_credentials_callback_)
      .Run(status, std::move(responses),
           metadata_response.num_estimated_remaining_credentials);
}

void CredentialManagementHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
  if (authenticator != authenticator_) {
    return;
  }

  authenticator_ = nullptr;
  if (state_ == State::kWaitingForPIN) {
    state_ = State::kFinished;
    std::move(finished_callback_)
        .Run(FidoReturnCode::kAuthenticatorRemovedDuringPINEntry);
    return;
  }
  if (state_ == State::kReady) {
    std::move(finished_callback_).Run(FidoReturnCode::kSuccess);
    return;
  }
}

}  // namespace device
