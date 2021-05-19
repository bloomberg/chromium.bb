// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/filter.h"
#include "device/fido/make_credential_task.h"

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/type_conversions.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "device/fido/cros/authenticator.h"
#endif

namespace device {

using PINUVDisposition = FidoAuthenticator::PINUVDisposition;
using BioEnrollmentAvailability =
    AuthenticatorSupportedOptions::BioEnrollmentAvailability;

namespace {

// Permissions requested for PinUvAuthToken. GetAssertion is needed for silent
// probing of credentials.
const std::set<pin::Permissions> GetMakeCredentialRequestPermissions(
    FidoAuthenticator* authenticator) {
  std::set<pin::Permissions> permissions = {pin::Permissions::kMakeCredential,
                                            pin::Permissions::kGetAssertion};
  if (authenticator->Options() &&
      authenticator->Options()->bio_enrollment_availability ==
          BioEnrollmentAvailability::kSupportedButUnprovisioned) {
    permissions.insert(pin::Permissions::kBioEnrollment);
  }
  return permissions;
}

base::Optional<MakeCredentialStatus> ConvertDeviceResponseCode(
    CtapDeviceResponseCode device_response_code) {
  switch (device_response_code) {
    case CtapDeviceResponseCode::kSuccess:
      return MakeCredentialStatus::kSuccess;

    // Only returned after the user interacted with the authenticator.
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      return MakeCredentialStatus::kUserConsentButCredentialExcluded;

    // The user explicitly denied the operation. Touch ID returns this error
    // when the user cancels the macOS prompt. External authenticators may
    // return it e.g. after the user fails fingerprint verification.
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return MakeCredentialStatus::kUserConsentDenied;

    // External authenticators may return this error if internal user
    // verification fails for a make credential request or if the pin token is
    // not valid.
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
      return MakeCredentialStatus::kUserConsentDenied;

    case CtapDeviceResponseCode::kCtap2ErrKeyStoreFull:
      return MakeCredentialStatus::kStorageFull;

    // For all other errors, the authenticator will be dropped, and other
    // authenticators may continue.
    default:
      return base::nullopt;
  }
}

// IsCandidateAuthenticatorPreTouch returns true if the given authenticator
// should even blink for a request.
bool IsCandidateAuthenticatorPreTouch(
    FidoAuthenticator* authenticator,
    AuthenticatorAttachment requested_attachment,
    bool allow_platform_authenticator_for_make_credential_request) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  if ((requested_attachment == AuthenticatorAttachment::kPlatform &&
       !opt_options->is_platform_device) ||
      (requested_attachment == AuthenticatorAttachment::kCrossPlatform &&
       opt_options->is_platform_device &&
       !allow_platform_authenticator_for_make_credential_request)) {
    return false;
  }

  return true;
}

// IsCandidateAuthenticatorPostTouch returns a value other than |kSuccess| if
// the given authenticator cannot handle a request.
MakeCredentialStatus IsCandidateAuthenticatorPostTouch(
    const CtapMakeCredentialRequest& request,
    FidoAuthenticator* authenticator,
    const MakeCredentialRequestHandler::Options& options,
    const FidoRequestHandlerBase::Observer* observer) {
  if (options.cred_protect_request && options.cred_protect_request->second &&
      !authenticator->SupportsCredProtectExtension()) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  const base::Optional<AuthenticatorSupportedOptions>& auth_options =
      authenticator->Options();
  if (!auth_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return MakeCredentialStatus::kSuccess;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Allow dispatch of UP-only cross-platform requests to the platform
  // authenticator to ensure backwards compatibility with the legacy
  // DeviceSecondFactorAuthentication enterprise policy.
  if (options.authenticator_attachment ==
          AuthenticatorAttachment::kCrossPlatform &&
      auth_options->is_platform_device) {
    if (options.resident_key == ResidentKeyRequirement::kRequired) {
      return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
    }
    if (options.user_verification == UserVerificationRequirement::kRequired) {
      return MakeCredentialStatus::kAuthenticatorMissingUserVerification;
    }
    return MakeCredentialStatus::kSuccess;
  }
#endif

  if (options.resident_key == ResidentKeyRequirement::kRequired &&
      !auth_options->supports_resident_key) {
    return MakeCredentialStatus::kAuthenticatorMissingResidentKeys;
  }

  if (authenticator->PINUVDispositionForMakeCredential(request, observer) ==
      PINUVDisposition::kUnsatisfiable) {
    return MakeCredentialStatus::kAuthenticatorMissingUserVerification;
  }

  // The largeBlobs extension only works for resident credentials on CTAP 2.1
  // authenticators.
  if (options.large_blob_support == LargeBlobSupport::kRequired &&
      (!auth_options->supports_large_blobs || !request.resident_key_required)) {
    return MakeCredentialStatus::kAuthenticatorMissingLargeBlob;
  }

  base::Optional<base::span<const int32_t>> supported_algorithms(
      authenticator->GetAlgorithms());
  if (supported_algorithms) {
    // Substitution of defaults should have happened by this point.
    DCHECK(!request.public_key_credential_params.public_key_credential_params()
                .empty());

    bool at_least_one_common_algorithm = false;
    for (const auto& algo :
         request.public_key_credential_params.public_key_credential_params()) {
      if (algo.type != CredentialType::kPublicKey) {
        continue;
      }

      if (base::Contains(*supported_algorithms, algo.algorithm)) {
        at_least_one_common_algorithm = true;
        break;
      }
    }

    if (!at_least_one_common_algorithm) {
      return MakeCredentialStatus::kNoCommonAlgorithms;
    }
  }

  return MakeCredentialStatus::kSuccess;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    AuthenticatorAttachment authenticator_attachment) {
  switch (authenticator_attachment) {
    case AuthenticatorAttachment::kPlatform:
      return {FidoTransportProtocol::kInternal};
    case AuthenticatorAttachment::kCrossPlatform:
      return {
          FidoTransportProtocol::kUsbHumanInterfaceDevice,
          FidoTransportProtocol::kBluetoothLowEnergy,
          FidoTransportProtocol::kNearFieldCommunication,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
          FidoTransportProtocol::kAndroidAccessory,
      };
    case AuthenticatorAttachment::kAny:
      return {
          FidoTransportProtocol::kInternal,
          FidoTransportProtocol::kNearFieldCommunication,
          FidoTransportProtocol::kUsbHumanInterfaceDevice,
          FidoTransportProtocol::kBluetoothLowEnergy,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
          FidoTransportProtocol::kAndroidAccessory,
      };
  }

  NOTREACHED();
  return base::flat_set<FidoTransportProtocol>();
}

void ReportMakeCredentialRequestTransport(FidoAuthenticator* authenticator) {
  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MakeCredentialRequestTransport",
        *authenticator->AuthenticatorTransport());
  }
}

// CredProtectForAuthenticator translates a |CredProtectRequest| to a
// |CredProtect| value given the capabilities of a specific authenticator.
CredProtect CredProtectForAuthenticator(
    CredProtectRequest request,
    const FidoAuthenticator& authenticator) {
  switch (request) {
    case CredProtectRequest::kUVOptional:
      return CredProtect::kUVOptional;
    case CredProtectRequest::kUVOrCredIDRequired:
      return CredProtect::kUVOrCredIDRequired;
    case CredProtectRequest::kUVRequired:
      return CredProtect::kUVRequired;
    case CredProtectRequest::kUVOrCredIDRequiredOrBetter:
      if (authenticator.Options() &&
          authenticator.Options()->default_cred_protect ==
              CredProtect::kUVRequired) {
        return CredProtect::kUVRequired;
      }
      return CredProtect::kUVOrCredIDRequired;
  }
}

// ValidateResponseExtensions returns true iff |extensions| is valid as a
// response to |request| from an authenticator that reports that it supports
// |options|.
bool ValidateResponseExtensions(
    const CtapMakeCredentialRequest& request,
    const MakeCredentialRequestHandler::Options& options,
    const FidoAuthenticator& authenticator,
    const cbor::Value& extensions) {
  if (!extensions.is_map()) {
    return false;
  }

  for (const auto& it : extensions.GetMap()) {
    if (!it.first.is_string()) {
      return false;
    }
    const std::string& ext_name = it.first.GetString();

    if (ext_name == kExtensionCredProtect) {
      if (!authenticator.SupportsCredProtectExtension() ||
          !it.second.is_integer()) {
        return false;
      }

      // The authenticator can return any valid credProtect value that is
      // equal to, or greater than, what was requested, including when
      // nothing was requested.
      const int64_t requested_level =
          options.cred_protect_request
              ? static_cast<int64_t>(CredProtectForAuthenticator(
                    options.cred_protect_request->first, authenticator))
              : 1;
      const int64_t returned_level = it.second.GetInteger();

      if (returned_level < requested_level ||
          returned_level >
              base::strict_cast<int64_t>(CredProtect::kUVRequired)) {
        FIDO_LOG(ERROR) << "Returned credProtect level (" << returned_level
                        << ") is invalid or less than the requested level ("
                        << requested_level << ")";
        return false;
      }
    } else if (ext_name == kExtensionHmacSecret) {
      if (!request.hmac_secret || !it.second.is_bool()) {
        return false;
      }
    } else {
      // Authenticators may not return unknown extensions.
      return false;
    }
  }

  return true;
}

// ResponseValid returns whether |response| is permissible for the given
// |authenticator| and |request|.
bool ResponseValid(const FidoAuthenticator& authenticator,
                   const CtapMakeCredentialRequest& request,
                   const AuthenticatorMakeCredentialResponse& response,
                   const MakeCredentialRequestHandler::Options& options) {
  if (response.GetRpIdHash() !=
      fido_parsing_utils::CreateSHA256Hash(request.rp.id)) {
    FIDO_LOG(ERROR) << "Invalid RP ID hash";
    return false;
  }

  const base::Optional<cbor::Value>& extensions =
      response.attestation_object().authenticator_data().extensions();
  if (extensions && !ValidateResponseExtensions(request, options, authenticator,
                                                *extensions)) {
    FIDO_LOG(ERROR) << "Invalid extensions block: "
                    << cbor::DiagnosticWriter::Write(*extensions);
    return false;
  }

  if (response.enterprise_attestation_returned &&
      (request.attestation_preference !=
           AttestationConveyancePreference::
               kEnterpriseIfRPListedOnAuthenticator &&
       request.attestation_preference !=
           AttestationConveyancePreference::kEnterpriseApprovedByBrowser)) {
    FIDO_LOG(ERROR) << "Enterprise attestation returned but not requested.";
    return false;
  }

  if (request.large_blob_key && !response.large_blob_key()) {
    FIDO_LOG(ERROR) << "Large blob key requested but not returned";
    return false;
  }

  return true;
}
}  // namespace

MakeCredentialRequestHandler::Options::Options() = default;
MakeCredentialRequestHandler::Options::~Options() = default;
MakeCredentialRequestHandler::Options::Options(const Options&) = default;
MakeCredentialRequestHandler::Options::Options(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria)
    : authenticator_attachment(
          authenticator_selection_criteria.authenticator_attachment()),
      resident_key(authenticator_selection_criteria.resident_key()),
      user_verification(
          authenticator_selection_criteria.user_verification_requirement()) {}
MakeCredentialRequestHandler::Options::Options(Options&&) = default;
MakeCredentialRequestHandler::Options&
MakeCredentialRequestHandler::Options::operator=(const Options&) = default;
MakeCredentialRequestHandler::Options&
MakeCredentialRequestHandler::Options::operator=(Options&&) = default;

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapMakeCredentialRequest request,
    const Options& options,
    CompletionCallback completion_callback)
    : completion_callback_(std::move(completion_callback)),
      request_(std::move(request)),
      options_(options) {
  // These parts of the request should be filled in by
  // |SpecializeRequestForAuthenticator|.
  DCHECK_EQ(request_.authenticator_attachment, AuthenticatorAttachment::kAny);
  DCHECK(!request_.resident_key_required);
  DCHECK(!request_.cred_protect);
  DCHECK(!request_.cred_protect_enforce);

  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kMakeCredential;
  transport_availability_info().is_off_the_record_context =
      request_.is_off_the_record_context;
  transport_availability_info().resident_key_requirement =
      options_.resident_key;

  base::flat_set<FidoTransportProtocol> allowed_transports =
      GetTransportsAllowedByRP(options.authenticator_attachment);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Attempt to instantiate the ChromeOS platform authenticator for
  // power-button-only requests for compatibility with the legacy
  // DeviceSecondFactorAuthentication policy, if that policy is enabled.
  if (!request_.is_u2f_only && options_.authenticator_attachment ==
                                   AuthenticatorAttachment::kCrossPlatform) {
    allow_platform_authenticator_for_cross_platform_request_ = true;
    fido_discovery_factory->set_require_legacy_cros_authenticator(true);
    allowed_transports.insert(FidoTransportProtocol::kInternal);
  }
#endif

  InitDiscoveries(
      fido_discovery_factory,
      base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
          supported_transports, allowed_transports));
  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !IsCandidateAuthenticatorPreTouch(
          authenticator, options_.authenticator_attachment,
          allow_platform_authenticator_for_cross_platform_request_)) {
    return;
  }

  const std::string authenticator_name = authenticator->GetDisplayName();
  switch (fido_filter::Evaluate(
      fido_filter::Operation::MAKE_CREDENTIAL, request_.rp.id,
      authenticator_name,
      std::pair<fido_filter::IDType, base::span<const uint8_t>>(
          fido_filter::IDType::USER_ID, request_.user.id))) {
    case fido_filter::Action::ALLOW:
      break;
    case fido_filter::Action::NO_ATTESTATION:
      suppress_attestation_ = true;
      break;
    case fido_filter::Action::BLOCK:
      FIDO_LOG(DEBUG) << "Filtered request to device " << authenticator_name;
      return;
  }

  for (const auto& cred : request_.exclude_list) {
    if (fido_filter::Evaluate(
            fido_filter::Operation::MAKE_CREDENTIAL, request_.rp.id,
            authenticator_name,
            std::pair<fido_filter::IDType, base::span<const uint8_t>>(
                fido_filter::IDType::CREDENTIAL_ID, cred.id())) ==
        fido_filter::Action::BLOCK) {
      FIDO_LOG(DEBUG) << "Filtered request to device " << authenticator_name
                      << " for credential ID " << base::HexEncode(cred.id());
      return;
    }
  }

  std::unique_ptr<CtapMakeCredentialRequest> request(
      new CtapMakeCredentialRequest(request_));
  SpecializeRequestForAuthenticator(request.get(), authenticator);

  if (IsCandidateAuthenticatorPostTouch(*request.get(), authenticator, options_,
                                        observer()) !=
      MakeCredentialStatus::kSuccess) {
#if defined(OS_WIN)
    // If the Windows API cannot handle a request, just reject the request
    // outright. There are no other authenticators to attempt, so calling
    // GetTouch() would not make sense.
    if (authenticator->IsWinNativeApiAuthenticator()) {
      HandleInapplicableAuthenticator(authenticator, std::move(request));
      return;
    }
#endif  // defined(OS_WIN)

    if (authenticator->Options() &&
        authenticator->Options()->is_platform_device) {
      HandleInapplicableAuthenticator(authenticator, std::move(request));
      return;
    }

    // This authenticator does not meet requirements, but make it flash anyway
    // so the user understands that it's functional. A descriptive error message
    // will be shown if the user selects it.
    authenticator->GetTouch(base::BindOnce(
        &MakeCredentialRequestHandler::HandleInapplicableAuthenticator,
        weak_factory_.GetWeakPtr(), authenticator, std::move(request)));
    return;
  }

  const bool skip_pin_touch =
      active_authenticators().size() == 1 && options_.allow_skipping_pin_touch;

  auto uv_disposition = authenticator->PINUVDispositionForMakeCredential(
      *request.get(), observer());
  switch (uv_disposition) {
    case PINUVDisposition::kNoUV:
    case PINUVDisposition::kNoTokenInternalUV:
    case PINUVDisposition::kNoTokenInternalUVPINFallback:
      break;
    case PINUVDisposition::kGetToken:
      ObtainPINUVAuthToken(authenticator, skip_pin_touch,
                           /*internal_uv_locked=*/false);
      return;
    case PINUVDisposition::kUnsatisfiable:
      // |IsCandidateAuthenticatorPostTouch| should have handled this case.
      NOTREACHED();
      return;
  }

  ReportMakeCredentialRequestTransport(authenticator);

  auto request_copy(*request.get());  // can't copy and move in the same stmt.
  authenticator->MakeCredential(
      std::move(request_copy),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request), base::ElapsedTimer()));
}

void MakeCredentialRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  auth_token_requester_map_.erase(authenticator);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == selected_authenticator_for_pin_uv_auth_token_) {
    selected_authenticator_for_pin_uv_auth_token_ = nullptr;
    // Authenticator could have been removed during PIN entry, PIN fallback
    // after failed internal UV, or bio enrollment. Bail and show an error.
    if (state_ != State::kFinished) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, nullptr);
    }
  }
}

void MakeCredentialRequestHandler::AuthenticatorSelectedForPINUVAuthToken(
    FidoAuthenticator* authenticator) {
  DCHECK_EQ(state_, State::kWaitingForTouch);
  state_ = State::kWaitingForToken;
  selected_authenticator_for_pin_uv_auth_token_ = authenticator;

  base::EraseIf(auth_token_requester_map_, [authenticator](auto& entry) {
    return entry.first != authenticator;
  });
  CancelActiveAuthenticators(authenticator->GetId());
}

void MakeCredentialRequestHandler::CollectPIN(
    pin::PINEntryReason reason,
    pin::PINEntryError error,
    uint32_t min_pin_length,
    int attempts,
    ProvidePINCallback provide_pin_cb) {
  DCHECK_EQ(state_, State::kWaitingForToken);
  observer()->CollectPIN({.reason = reason,
                          .error = error,
                          .min_pin_length = min_pin_length,
                          .attempts = attempts},
                         std::move(provide_pin_cb));
}

void MakeCredentialRequestHandler::PromptForInternalUVRetry(int attempts) {
  DCHECK(state_ == State::kWaitingForTouch ||
         state_ == State::kWaitingForToken);
  observer()->OnRetryUserVerification(attempts);
}

void MakeCredentialRequestHandler::HavePINUVAuthTokenResultForAuthenticator(
    FidoAuthenticator* authenticator,
    AuthTokenRequester::Result result,
    base::Optional<pin::TokenResponse> token_response) {
  base::Optional<MakeCredentialStatus> error;
  switch (result) {
    case AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest:
    case AuthTokenRequester::Result::kPreTouchAuthenticatorResponseInvalid:
      FIDO_LOG(ERROR) << "Ignoring MakeCredentialStatus="
                      << static_cast<int>(result) << " from "
                      << authenticator->GetId();
      return;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorInternalUVLock:
      error = MakeCredentialStatus::kAuthenticatorMissingUserVerification;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorResponseInvalid:
      error = MakeCredentialStatus::kAuthenticatorResponseInvalid;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorOperationDenied:
      error = MakeCredentialStatus::kUserConsentDenied;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorPINSoftLock:
      error = MakeCredentialStatus::kSoftPINBlock;
      break;
    case AuthTokenRequester::Result::kPostTouchAuthenticatorPINHardLock:
      error = MakeCredentialStatus::kHardPINBlock;
      break;
    case AuthTokenRequester::Result::kSuccess:
      break;
  }

  // Pre touch events should be handled above.
  DCHECK_EQ(state_, State::kWaitingForToken);
  DCHECK_EQ(selected_authenticator_for_pin_uv_auth_token_, authenticator);
  if (error) {
    state_ = State::kFinished;
    std::move(completion_callback_).Run(*error, base::nullopt, authenticator);
    return;
  }

  DCHECK_EQ(result, AuthTokenRequester::Result::kSuccess);

  auto request = std::make_unique<CtapMakeCredentialRequest>(request_);
  SpecializeRequestForAuthenticator(request.get(), authenticator);

  // If the authenticator supports biometric enrollment but is not enrolled,
  // offer enrollment with the request.
  if (authenticator->Options()->bio_enrollment_availability ==
          BioEnrollmentAvailability::kSupportedButUnprovisioned ||
      authenticator->Options()->bio_enrollment_availability_preview ==
          BioEnrollmentAvailability::kSupportedButUnprovisioned) {
    state_ = State::kBioEnrollment;
    bio_enroller_ =
        std::make_unique<BioEnroller>(this, authenticator, *token_response);
    bio_enrollment_complete_barrier_.emplace(base::BarrierClosure(
        2, base::BindOnce(&MakeCredentialRequestHandler::OnEnrollmentComplete,
                          weak_factory_.GetWeakPtr(), std::move(request))));
    observer()->StartBioEnrollment(
        base::BindOnce(&MakeCredentialRequestHandler::OnEnrollmentDismissed,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  DispatchRequestWithToken(authenticator, std::move(request),
                           std::move(*token_response));
}

void MakeCredentialRequestHandler::ObtainPINUVAuthToken(
    FidoAuthenticator* authenticator,
    bool skip_pin_touch,
    bool internal_uv_locked) {
  AuthTokenRequester::Options options;
  options.token_permissions =
      GetMakeCredentialRequestPermissions(authenticator);
  options.rp_id = request_.rp.id;
  options.skip_pin_touch = skip_pin_touch;
  options.internal_uv_locked = internal_uv_locked;

  auth_token_requester_map_.insert(
      {authenticator, std::make_unique<AuthTokenRequester>(
                          this, authenticator, std::move(options))});
  auth_token_requester_map_.at(authenticator)->ObtainPINUVAuthToken();
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request,
    base::ElapsedTimer request_timer,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForResponseWithToken) {
    return;
  }

#if defined(OS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator()) {
    state_ = State::kFinished;
    if (status != CtapDeviceResponseCode::kSuccess) {
      std::move(completion_callback_)
          .Run(WinCtapDeviceResponseCodeToMakeCredentialStatus(status),
               base::nullopt, authenticator);
      return;
    }
    if (!response ||
        !ResponseValid(*authenticator, *request, *response, options_)) {
      FIDO_LOG(ERROR)
          << "Failing make credential request due to bad response from "
          << authenticator->GetDisplayName();
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kWinNotAllowedError, base::nullopt,
               authenticator);
      return;
    }
    CancelActiveAuthenticators(authenticator->GetId());
    response->attestation_should_be_filtered = suppress_attestation_;
    std::move(completion_callback_)
        .Run(WinCtapDeviceResponseCodeToMakeCredentialStatus(status),
             std::move(*response), authenticator);
    return;
  }
#endif

  // If we requested UV from an authentiator without uvToken support, UV failed,
  // and the authenticator supports PIN, fall back to that.
  if (request->user_verification != UserVerificationRequirement::kDiscouraged &&
      !request->pin_auth &&
      (status == CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid ||
       status == CtapDeviceResponseCode::kCtap2ErrPinRequired) &&
      authenticator->PINUVDispositionForMakeCredential(*request, observer()) ==
          PINUVDisposition::kNoTokenInternalUVPINFallback) {
    // Authenticators without uvToken support will return this error immediately
    // without user interaction when internal UV is locked.
    const base::TimeDelta response_time = request_timer.Elapsed();
    if (response_time < kMinExpectedAuthenticatorResponseTime) {
      FIDO_LOG(DEBUG) << "Authenticator is probably locked, response_time="
                      << response_time;
      ObtainPINUVAuthToken(authenticator, /*skip_pin_touch=*/false,
                           /*internal_uv_locked=*/true);
      return;
    }
    ObtainPINUVAuthToken(authenticator, /*skip_pin_touch=*/true,
                         /*internal_uv_locked=*/true);
    return;
  }

  if (options_.resident_key == ResidentKeyRequirement::kPreferred &&
      request->resident_key_required &&
      status == CtapDeviceResponseCode::kCtap2ErrKeyStoreFull) {
    FIDO_LOG(DEBUG) << "Downgrading rk=preferred to non-resident credential "
                       "because key storage is full";
    request->resident_key_required = false;
    CtapMakeCredentialRequest request_copy(*request);
    authenticator->MakeCredential(
        std::move(request_copy),
        base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                       weak_factory_.GetWeakPtr(), authenticator,
                       std::move(request), base::ElapsedTimer()));
    return;
  }

  const base::Optional<MakeCredentialStatus> maybe_result =
      ConvertDeviceResponseCode(status);
  if (!maybe_result) {
    if (state_ == State::kWaitingForResponseWithToken) {
      std::move(completion_callback_)
          .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid,
               base::nullopt, authenticator);
    } else {
      FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                      << " from " << authenticator->GetDisplayName();
    }
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing make credential request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(*maybe_result, base::nullopt, authenticator);
    return;
  }

  if (!response ||
      !ResponseValid(*authenticator, *request, *response, options_)) {
    FIDO_LOG(ERROR)
        << "Failing make credential request due to bad response from "
        << authenticator->GetDisplayName();
    std::move(completion_callback_)
        .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
             authenticator);
    return;
  }

  if (authenticator->AuthenticatorTransport()) {
    base::UmaHistogramEnumeration(
        "WebAuthentication.MakeCredentialResponseTransport",
        *authenticator->AuthenticatorTransport());
  }

  response->attestation_should_be_filtered = suppress_attestation_;
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kSuccess, std::move(*response), authenticator);
}

void MakeCredentialRequestHandler::HandleInapplicableAuthenticator(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  // User touched an authenticator that cannot handle this request.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  const MakeCredentialStatus capability_error =
      IsCandidateAuthenticatorPostTouch(*request.get(), authenticator, options_,
                                        observer());
  DCHECK_NE(capability_error, MakeCredentialStatus::kSuccess);
  std::move(completion_callback_).Run(capability_error, base::nullopt, nullptr);
}

void MakeCredentialRequestHandler::OnSampleCollected(
    BioEnrollmentSampleStatus status,
    int samples_remaining) {
  observer()->OnSampleCollected(samples_remaining);
}

void MakeCredentialRequestHandler::OnEnrollmentDone(
    base::Optional<std::vector<uint8_t>> template_id) {
  state_ = State::kBioEnrollmentDone;

  bio_enrollment_complete_barrier_->Run();
}

void MakeCredentialRequestHandler::OnEnrollmentError(
    CtapDeviceResponseCode status) {
  bio_enroller_.reset();
  state_ = State::kFinished;
  std::move(completion_callback_)
      .Run(MakeCredentialStatus::kAuthenticatorResponseInvalid, base::nullopt,
           nullptr);
}

void MakeCredentialRequestHandler::OnEnrollmentDismissed() {
  if (state_ != State::kBioEnrollmentDone) {
    // There is still an inflight enrollment request. Cancel it.
    bio_enroller_->Cancel();
  }

  bio_enrollment_complete_barrier_->Run();
}

void MakeCredentialRequestHandler::OnEnrollmentComplete(
    std::unique_ptr<CtapMakeCredentialRequest> request) {
  DCHECK(state_ == State::kBioEnrollmentDone);

  bio_enrollment_complete_barrier_.reset();
  auto token = bio_enroller_->token();
  FidoAuthenticator* authenticator = bio_enroller_->authenticator();
  DCHECK_EQ(authenticator, selected_authenticator_for_pin_uv_auth_token_);
  bio_enroller_.reset();
  DispatchRequestWithToken(authenticator, std::move(request), std::move(token));
}

void MakeCredentialRequestHandler::DispatchRequestWithToken(
    FidoAuthenticator* authenticator,
    std::unique_ptr<CtapMakeCredentialRequest> request,
    pin::TokenResponse token) {
  observer()->FinishCollectToken();
  state_ = State::kWaitingForResponseWithToken;
  std::tie(request->pin_protocol, request->pin_auth) =
      token.PinAuth(request->client_data_hash);
  request->pin_token_for_exclude_list_probing = std::move(token);

  ReportMakeCredentialRequestTransport(authenticator);

  auto request_copy(*request.get());  // can't copy and move in the same stmt.
  authenticator->MakeCredential(
      std::move(request_copy),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator,
                     std::move(request), base::ElapsedTimer()));
}

void MakeCredentialRequestHandler::SpecializeRequestForAuthenticator(
    CtapMakeCredentialRequest* request,
    const FidoAuthenticator* authenticator) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (authenticator->AuthenticatorTransport() ==
          FidoTransportProtocol::kInternal &&
      options_.authenticator_attachment ==
          AuthenticatorAttachment::kCrossPlatform) {
    request->resident_key_required = false;
    request->user_verification = UserVerificationRequirement::kDiscouraged;
    // None of the other options below are applicable.
    return;
  }
#endif

  // Only Windows cares about |authenticator_attachment| on the request.
  request->authenticator_attachment = options_.authenticator_attachment;

  const base::Optional<AuthenticatorSupportedOptions>& auth_options =
      authenticator->Options();
  switch (options_.resident_key) {
    case ResidentKeyRequirement::kRequired:
      request->resident_key_required = true;
      break;
    case ResidentKeyRequirement::kPreferred: {
      // Create a resident key if the authenticator supports it, has sufficient
      // storage space for another credential, and we can obtain UV via client
      // PIN or an internal modality.
      request->resident_key_required =
#if defined(OS_WIN)
          // Windows does not yet support rk=preferred.
          !authenticator->IsWinNativeApiAuthenticator() &&
#endif
          auth_options && auth_options->supports_resident_key &&
          !authenticator->DiscoverableCredentialStorageFull() &&
          (observer()->SupportsPIN() ||
           auth_options->user_verification_availability ==
               AuthenticatorSupportedOptions::UserVerificationAvailability::
                   kSupportedAndConfigured);
      break;
    }
    case ResidentKeyRequirement::kDiscouraged:
      request->resident_key_required = false;
      break;
  }

  switch (options_.large_blob_support) {
    case LargeBlobSupport::kRequired:
      request->large_blob_key = true;
      break;
    case LargeBlobSupport::kPreferred:
      request->large_blob_key = auth_options &&
                                auth_options->supports_large_blobs &&
                                request->resident_key_required;
      break;
    case LargeBlobSupport::kNotRequested:
      request->large_blob_key = false;
      break;
  }

  if (!request->is_u2f_only && (request->resident_key_required ||
                                (auth_options && auth_options->always_uv))) {
    request->user_verification = UserVerificationRequirement::kRequired;
  } else {
    request->user_verification = options_.user_verification;
  }

  if (options_.cred_protect_request &&
      authenticator->SupportsCredProtectExtension()) {
    request->cred_protect = CredProtectForAuthenticator(
        options_.cred_protect_request->first, *authenticator);
    request->cred_protect_enforce = options_.cred_protect_request->second;
  }

  if (request->hmac_secret && !authenticator->SupportsHMACSecretExtension()) {
    request->hmac_secret = false;
  }

  if (request->large_blob_key &&
      !authenticator->Options()->supports_large_blobs) {
    request->large_blob_key = false;
  }

  if (!authenticator->SupportsEnterpriseAttestation()) {
    switch (request->attestation_preference) {
      case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
        // If enterprise attestation is approved by policy then downgrade to
        // "direct" if not supported. Otherwise we have the strange behaviour
        // that kEnterpriseApprovedByBrowser turns into "none" on Windows
        // without EP support, or macOS/Chrome OS platform authenticators, but
        // "direct" elsewhere.
        request->attestation_preference =
            AttestationConveyancePreference::kDirect;
        break;
      case AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator:
        request->attestation_preference =
            AttestationConveyancePreference::kNone;
        break;
      default:
        break;
    }
  }
}

}  // namespace device
