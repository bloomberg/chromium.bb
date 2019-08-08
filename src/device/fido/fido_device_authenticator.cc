// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/credential_management.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/features.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/get_assertion_task.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"

namespace device {

FidoDeviceAuthenticator::FidoDeviceAuthenticator(
    std::unique_ptr<FidoDevice> device)
    : device_(std::move(device)), weak_factory_(this) {}
FidoDeviceAuthenticator::~FidoDeviceAuthenticator() = default;

void FidoDeviceAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FidoDevice::DiscoverSupportedProtocolAndDeviceInfo,
          device()->GetWeakPtr(),
          base::BindOnce(&FidoDeviceAuthenticator::InitializeAuthenticatorDone,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void FidoDeviceAuthenticator::InitializeAuthenticatorDone(
    base::OnceClosure callback) {
  DCHECK(!options_);
  switch (device_->supported_protocol()) {
    case ProtocolVersion::kU2f:
      options_ = AuthenticatorSupportedOptions();
      break;
    case ProtocolVersion::kCtap2:
      DCHECK(device_->device_info()) << "uninitialized device";
      options_ = device_->device_info()->options;
      break;
    case ProtocolVersion::kUnknown:
      NOTREACHED() << "uninitialized device";
      options_ = AuthenticatorSupportedOptions();
  }
  std::move(callback).Run();
}

void FidoDeviceAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                             MakeCredentialCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  task_ = std::make_unique<MakeCredentialTask>(
      device_.get(), std::move(request), std::move(callback));
}

void FidoDeviceAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                           GetAssertionCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  task_ = std::make_unique<GetAssertionTask>(device_.get(), std::move(request),
                                             std::move(callback));
}

void FidoDeviceAuthenticator::GetNextAssertion(GetAssertionCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";

  operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetNextAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device_.get(), CtapGetNextAssertionRequest(), std::move(callback),
          base::BindOnce(&ReadCTAPGetAssertionResponse),
          GetAssertionTask::StringFixupPredicate);
  operation_->Start();
}

void FidoDeviceAuthenticator::GetTouch(base::OnceCallback<void()> callback) {
  MakeCredential(
      MakeCredentialTask::GetTouchRequest(device()),
      base::BindOnce(
          [](std::string authenticator_id, base::OnceCallback<void()> callback,
             CtapDeviceResponseCode status,
             base::Optional<AuthenticatorMakeCredentialResponse>) {
            // If the device didn't understand/process the request it may
            // fail immediately. Rather than count that as a touch, ignore
            // those cases completely.
            if (status == CtapDeviceResponseCode::kSuccess ||
                status == CtapDeviceResponseCode::kCtap2ErrPinNotSet ||
                status == CtapDeviceResponseCode::kCtap2ErrPinInvalid ||
                status == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid) {
              std::move(callback).Run();
              return;
            }
            FIDO_LOG(DEBUG) << "Ignoring status " << static_cast<int>(status)
                            << " from " << authenticator_id;
          },
          GetId(), std::move(callback)));
}

void FidoDeviceAuthenticator::GetRetries(GetRetriesCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::RetriesRequest, pin::RetriesResponse>>(
      device_.get(), pin::RetriesRequest(), std::move(callback),
      base::BindOnce(&pin::RetriesResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::GetEphemeralKey(
    GetEphemeralKeyCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ =
      std::make_unique<Ctap2DeviceOperation<pin::KeyAgreementRequest,
                                            pin::KeyAgreementResponse>>(
          device_.get(), pin::KeyAgreementRequest(), std::move(callback),
          base::BindOnce(&pin::KeyAgreementResponse::Parse),
          /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::GetPINToken(
    std::string pin,
    const pin::KeyAgreementResponse& peer_key,
    GetPINTokenCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  pin::TokenRequest request(pin, peer_key);
  std::array<uint8_t, 32> shared_key = request.shared_key();
  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::TokenRequest, pin::TokenResponse>>(
      device_.get(), std::move(request), std::move(callback),
      base::BindOnce(&pin::TokenResponse::Parse, std::move(shared_key)),
      /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::SetPIN(const std::string& pin,
                                     const pin::KeyAgreementResponse& peer_key,
                                     SetPINCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::SetRequest, pin::EmptyResponse>>(
      device_.get(), pin::SetRequest(pin, peer_key), std::move(callback),
      base::BindOnce(&pin::EmptyResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::ChangePIN(const std::string& old_pin,
                                        const std::string& new_pin,
                                        pin::KeyAgreementResponse& peer_key,
                                        SetPINCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";
  DCHECK(Options());
  DCHECK(Options()->client_pin_availability !=
         AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::ChangeRequest, pin::EmptyResponse>>(
      device_.get(), pin::ChangeRequest(old_pin, new_pin, peer_key),
      std::move(callback), base::BindOnce(&pin::EmptyResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

FidoAuthenticator::MakeCredentialPINDisposition
FidoDeviceAuthenticator::WillNeedPINToMakeCredential(
    const CtapMakeCredentialRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  using ClientPinAvailability =
      AuthenticatorSupportedOptions::ClientPinAvailability;

  // Authenticators with built-in UV can use that. (Fallback to PIN is not yet
  // implemented.)
  if (Options()->user_verification_availability ==
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured) {
    return MakeCredentialPINDisposition::kNoPIN;
  }

  const auto device_support = Options()->client_pin_availability;
  const bool can_collect_pin = observer && observer->SupportsPIN();

  // CTAP 2.0 requires a PIN for credential creation once a PIN has been set.
  // Thus, if fallback to U2F isn't possible, a PIN will be needed if set.
  const bool supports_u2f =
      device()->device_info() &&
      device()->device_info()->versions.contains(ProtocolVersion::kU2f);
  if (device_support == ClientPinAvailability::kSupportedAndPinSet &&
      !supports_u2f) {
    if (can_collect_pin) {
      return MakeCredentialPINDisposition::kUsePIN;
    } else {
      return MakeCredentialPINDisposition::kUnsatisfiable;
    }
  }

  // If a PIN cannot be collected, and UV is required, then this request cannot
  // be met.
  if (request.user_verification == UserVerificationRequirement::kRequired &&
      (!can_collect_pin ||
       device_support == ClientPinAvailability::kNotSupported)) {
    return MakeCredentialPINDisposition::kUnsatisfiable;
  }

  // If UV is required and a PIN can be set, set it during the MakeCredential
  // process.
  if (device_support == ClientPinAvailability::kSupportedButPinNotSet &&
      request.user_verification == UserVerificationRequirement::kRequired) {
    return MakeCredentialPINDisposition::kSetPIN;
  }

  // If discouraged, then either a PIN isn't set (thus we don't use one), or
  // else the device supports U2F (because the alternative was handled above)
  // and we'll use a U2F fallback to create a credential without a PIN.
  DCHECK(device_support != ClientPinAvailability::kSupportedAndPinSet ||
         supports_u2f);
  // TODO(agl): perhaps CTAP2 is indicated when, for example, hmac-secret is
  // requested?
  if (request.user_verification == UserVerificationRequirement::kDiscouraged) {
    return MakeCredentialPINDisposition::kNoPIN;
  }

  // Otherwise, a PIN will be used only if set.
  if (device_support == ClientPinAvailability::kSupportedAndPinSet &&
      can_collect_pin) {
    return MakeCredentialPINDisposition::kUsePIN;
  }

  return MakeCredentialPINDisposition::kNoPIN;
}

FidoAuthenticator::GetAssertionPINDisposition
FidoDeviceAuthenticator::WillNeedPINToGetAssertion(
    const CtapGetAssertionRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  // Authenticators with built-in UV can use that. (Fallback to PIN is not yet
  // implemented.)
  if (Options()->user_verification_availability ==
      AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured) {
    return GetAssertionPINDisposition::kNoPIN;
  }

  const bool can_use_pin = (Options()->client_pin_availability ==
                            AuthenticatorSupportedOptions::
                                ClientPinAvailability::kSupportedAndPinSet) &&
                           // The PIN is effectively unavailable if there's no
                           // UI support for collecting it.
                           observer && observer->SupportsPIN();
  const bool resident_key_request = request.allow_list.empty();

  if (resident_key_request) {
    if (can_use_pin) {
      return GetAssertionPINDisposition::kUsePIN;
    }
    return GetAssertionPINDisposition::kUnsatisfiable;
  }

  // If UV is required then the PIN must be used if set, or else this request
  // cannot be satisfied.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    if (can_use_pin) {
      return GetAssertionPINDisposition::kUsePIN;
    }
    return GetAssertionPINDisposition::kUnsatisfiable;
  }

  // If UV is preferred and a PIN is set, use it.
  if (request.user_verification == UserVerificationRequirement::kPreferred &&
      can_use_pin) {
    return GetAssertionPINDisposition::kUsePIN;
  }
  return GetAssertionPINDisposition::kNoPIN;
}

void FidoDeviceAuthenticator::GetCredentialsMetadata(
    base::span<const uint8_t> pin_token,
    GetCredentialsMetadataCallback callback) {
  DCHECK(Options()->supports_credential_management ||
         Options()->supports_credential_management_preview);

  operation_ =
      std::make_unique<Ctap2DeviceOperation<CredentialManagementRequest,
                                            CredentialsMetadataResponse>>(
          device_.get(),
          CredentialManagementRequest::ForGetCredsMetadata(
              Options()->supports_credential_management
                  ? CredentialManagementRequest::kDefault
                  : CredentialManagementRequest::kPreview,
              pin_token),
          std::move(callback),
          base::BindOnce(&CredentialsMetadataResponse::Parse),
          /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

struct FidoDeviceAuthenticator::EnumerateCredentialsState {
  EnumerateCredentialsState() = default;
  EnumerateCredentialsState(EnumerateCredentialsState&&) = default;
  EnumerateCredentialsState& operator=(EnumerateCredentialsState&&) = default;

  std::vector<uint8_t> pin_token;
  bool is_first_rp = true;
  bool is_first_credential = true;
  size_t rp_count;
  size_t current_rp_credential_count;

  FidoDeviceAuthenticator::EnumerateCredentialsCallback callback;
  std::vector<AggregatedEnumerateCredentialsResponse> responses;
};

void FidoDeviceAuthenticator::EnumerateCredentials(
    base::span<const uint8_t> pin_token,
    EnumerateCredentialsCallback callback) {
  DCHECK(Options()->supports_credential_management ||
         Options()->supports_credential_management_preview);

  EnumerateCredentialsState state;
  state.pin_token = fido_parsing_utils::Materialize(pin_token);
  state.callback = std::move(callback);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<CredentialManagementRequest, EnumerateRPsResponse>>(
      device_.get(),
      CredentialManagementRequest::ForEnumerateRPsBegin(
          Options()->supports_credential_management
              ? CredentialManagementRequest::kDefault
              : CredentialManagementRequest::kPreview,
          pin_token),
      base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateRPsDone,
                     weak_factory_.GetWeakPtr(), std::move(state)),
      base::BindOnce(&EnumerateRPsResponse::Parse, /*expect_rp_count=*/true),
      // TODO(martinkr): implement utf-8 fixup and add a test for it.
      /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::OnEnumerateRPsDone(
    EnumerateCredentialsState state,
    CtapDeviceResponseCode status,
    base::Optional<EnumerateRPsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(state.callback).Run(status, base::nullopt);
    return;
  }
  if (state.is_first_rp) {
    if (response->rp_count == 0) {
      std::move(state.callback).Run(status, std::move(state.responses));
      return;
    }
    state.rp_count = response->rp_count;
    state.is_first_rp = false;
  }
  DCHECK(response->rp);
  DCHECK(response->rp_id_hash);

  state.is_first_credential = true;
  state.responses.emplace_back(std::move(*response->rp));

  auto request = CredentialManagementRequest::ForEnumerateCredentialsBegin(
      Options()->supports_credential_management
          ? CredentialManagementRequest::kDefault
          : CredentialManagementRequest::kPreview,
      state.pin_token, std::move(*response->rp_id_hash));
  operation_ =
      std::make_unique<Ctap2DeviceOperation<CredentialManagementRequest,
                                            EnumerateCredentialsResponse>>(
          device_.get(), std::move(request),
          base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateCredentialsDone,
                         weak_factory_.GetWeakPtr(), std::move(state)),
          base::BindOnce(&EnumerateCredentialsResponse::Parse,
                         /*expect_credential_count=*/true),
          // TODO(martinkr): implement utf-8 fixup and add a test for it.
          /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::OnEnumerateCredentialsDone(
    EnumerateCredentialsState state,
    CtapDeviceResponseCode status,
    base::Optional<EnumerateCredentialsResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    std::move(state.callback).Run(status, base::nullopt);
    return;
  }
  if (state.is_first_credential) {
    state.current_rp_credential_count = response->credential_count;
    state.is_first_credential = false;
  }
  state.responses.back().credentials.emplace_back(std::move(*response));

  if (state.responses.back().credentials.size() <
      state.current_rp_credential_count) {
    operation_ =
        std::make_unique<Ctap2DeviceOperation<CredentialManagementRequest,
                                              EnumerateCredentialsResponse>>(
            device_.get(),
            CredentialManagementRequest::ForEnumerateCredentialsGetNext(
                Options()->supports_credential_management
                    ? CredentialManagementRequest::kDefault
                    : CredentialManagementRequest::kPreview),
            base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateCredentialsDone,
                           weak_factory_.GetWeakPtr(), std::move(state)),
            base::BindOnce(&EnumerateCredentialsResponse::Parse,
                           /*expect_credential_count=*/false),
            // TODO(martinkr): implement utf-8 fixup and add a test for it.
            /*string_fixup_predicate=*/nullptr);
    operation_->Start();
    return;
  }

  if (state.responses.size() < state.rp_count) {
    operation_ =
        std::make_unique<Ctap2DeviceOperation<CredentialManagementRequest,
                                              EnumerateRPsResponse>>(
            device_.get(),
            CredentialManagementRequest::ForEnumerateRPsGetNext(
                Options()->supports_credential_management
                    ? CredentialManagementRequest::kDefault
                    : CredentialManagementRequest::kPreview),
            base::BindOnce(&FidoDeviceAuthenticator::OnEnumerateRPsDone,
                           weak_factory_.GetWeakPtr(), std::move(state)),
            base::BindOnce(&EnumerateRPsResponse::Parse,
                           /*expect_rp_count=*/false),
            // TODO(martinkr): implement utf-8 fixup and add a test for it.
            /*string_fixup_predicate=*/nullptr);
    operation_->Start();
    return;
  }

  std::move(state.callback)
      .Run(CtapDeviceResponseCode::kSuccess, std::move(state.responses));
  return;
}

void FidoDeviceAuthenticator::DeleteCredential(
    base::span<const uint8_t> pin_token,
    base::span<const uint8_t> credential_id,
    DeleteCredentialCallback callback) {
  DCHECK(Options()->supports_credential_management ||
         Options()->supports_credential_management_preview);

  operation_ =
      std::make_unique<Ctap2DeviceOperation<CredentialManagementRequest,
                                            DeleteCredentialResponse>>(
          device_.get(),
          CredentialManagementRequest::ForDeleteCredential(
              Options()->supports_credential_management
                  ? CredentialManagementRequest::kDefault
                  : CredentialManagementRequest::kPreview,
              pin_token, fido_parsing_utils::Materialize(credential_id)),
          std::move(callback), base::BindOnce(&DeleteCredentialResponse::Parse),
          /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::GetModality(
    GetBioEnrollmentInfoCallback callback) {
  DCHECK(
      Options()->bio_enrollment_availability_preview !=
      AuthenticatorSupportedOptions::BioEnrollmentAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<BioEnrollmentRequest, BioEnrollmentResponse>>(
      device_.get(), BioEnrollmentRequest::ForGetModality(),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse),
      nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::GetSensorInfo(
    GetBioEnrollmentInfoCallback callback) {
  DCHECK(
      Options()->bio_enrollment_availability_preview !=
      AuthenticatorSupportedOptions::BioEnrollmentAvailability::kNotSupported);

  operation_ = std::make_unique<
      Ctap2DeviceOperation<BioEnrollmentRequest, BioEnrollmentResponse>>(
      device_.get(), BioEnrollmentRequest::ForGetSensorInfo(),
      std::move(callback), base::BindOnce(&BioEnrollmentResponse::Parse),
      nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::Reset(ResetCallback callback) {
  DCHECK(device_->SupportedProtocolIsInitialized())
      << "InitializeAuthenticator() must be called first.";

  operation_ = std::make_unique<
      Ctap2DeviceOperation<pin::ResetRequest, pin::ResetResponse>>(
      device_.get(), pin::ResetRequest(), std::move(callback),
      base::BindOnce(&pin::ResetResponse::Parse),
      /*string_fixup_predicate=*/nullptr);
  operation_->Start();
}

void FidoDeviceAuthenticator::Cancel() {
  if (operation_) {
    operation_->Cancel();
  }
  if (task_) {
    task_->Cancel();
  }
}

std::string FidoDeviceAuthenticator::GetId() const {
  return device_->GetId();
}

base::string16 FidoDeviceAuthenticator::GetDisplayName() const {
  return device_->GetDisplayName();
}

ProtocolVersion FidoDeviceAuthenticator::SupportedProtocol() const {
  DCHECK(device_->SupportedProtocolIsInitialized());
  return device_->supported_protocol();
}

const base::Optional<AuthenticatorSupportedOptions>&
FidoDeviceAuthenticator::Options() const {
  return options_;
}

base::Optional<FidoTransportProtocol>
FidoDeviceAuthenticator::AuthenticatorTransport() const {
  return device_->DeviceTransport();
}

bool FidoDeviceAuthenticator::IsInPairingMode() const {
  return device_->IsInPairingMode();
}

bool FidoDeviceAuthenticator::IsPaired() const {
  return device_->IsPaired();
}

#if defined(OS_WIN)
bool FidoDeviceAuthenticator::IsWinNativeApiAuthenticator() const {
  return false;
}
#endif  // defined(OS_WIN)

void FidoDeviceAuthenticator::SetTaskForTesting(
    std::unique_ptr<FidoTask> task) {
  task_ = std::move(task);
}

base::WeakPtr<FidoAuthenticator> FidoDeviceAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
