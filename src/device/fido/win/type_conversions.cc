// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/type_conversions.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/opaque_attestation_statement.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

absl::optional<AuthenticatorMakeCredentialResponse>
ToAuthenticatorMakeCredentialResponse(
    const WEBAUTHN_CREDENTIAL_ATTESTATION& credential_attestation) {
  auto authenticator_data = AuthenticatorData::DecodeAuthenticatorData(
      base::span<const uint8_t>(credential_attestation.pbAuthenticatorData,
                                credential_attestation.cbAuthenticatorData));
  if (!authenticator_data) {
    DLOG(ERROR) << "DecodeAuthenticatorData failed: "
                << base::HexEncode(credential_attestation.pbAuthenticatorData,
                                   credential_attestation.cbAuthenticatorData);
    return absl::nullopt;
  }
  absl::optional<cbor::Value> cbor_attestation_statement = cbor::Reader::Read(
      base::span<const uint8_t>(credential_attestation.pbAttestation,
                                credential_attestation.cbAttestation));
  if (!cbor_attestation_statement || !cbor_attestation_statement->is_map()) {
    DLOG(ERROR) << "CBOR decoding attestation statement failed: "
                << base::HexEncode(credential_attestation.pbAttestation,
                                   credential_attestation.cbAttestation);
    return absl::nullopt;
  }

  absl::optional<FidoTransportProtocol> transport_used;
  if (credential_attestation.dwVersion >=
      WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3) {
    // dwUsedTransport should have exactly one of the
    // WEBAUTHN_CTAP_TRANSPORT_* values set.
    switch (credential_attestation.dwUsedTransport) {
      case WEBAUTHN_CTAP_TRANSPORT_USB:
        transport_used = FidoTransportProtocol::kUsbHumanInterfaceDevice;
        break;
      case WEBAUTHN_CTAP_TRANSPORT_NFC:
        transport_used = FidoTransportProtocol::kNearFieldCommunication;
        break;
      case WEBAUTHN_CTAP_TRANSPORT_BLE:
        transport_used = FidoTransportProtocol::kBluetoothLowEnergy;
        break;
      case WEBAUTHN_CTAP_TRANSPORT_INTERNAL:
        transport_used = FidoTransportProtocol::kInternal;
        break;
      default:
        // Ignore _TEST and possibly future others.
        break;
    }
  }

  return AuthenticatorMakeCredentialResponse(
      transport_used,
      AttestationObject(
          std::move(*authenticator_data),
          std::make_unique<OpaqueAttestationStatement>(
              base::WideToUTF8(credential_attestation.pwszFormatType),
              std::move(*cbor_attestation_statement))));
}

absl::optional<AuthenticatorGetAssertionResponse>
ToAuthenticatorGetAssertionResponse(
    const WEBAUTHN_ASSERTION& assertion,
    const std::vector<PublicKeyCredentialDescriptor>& allow_list) {
  auto authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(base::span<const uint8_t>(
          assertion.pbAuthenticatorData, assertion.cbAuthenticatorData));
  if (!authenticator_data) {
    DLOG(ERROR) << "DecodeAuthenticatorData failed: "
                << base::HexEncode(assertion.pbAuthenticatorData,
                                   assertion.cbAuthenticatorData);
    return absl::nullopt;
  }
  AuthenticatorGetAssertionResponse response(
      std::move(*authenticator_data),
      std::vector<uint8_t>(assertion.pbSignature,
                           assertion.pbSignature + assertion.cbSignature));
  response.credential = PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      std::vector<uint8_t>(
          assertion.Credential.pbId,
          assertion.Credential.pbId + assertion.Credential.cbId));
  if (assertion.cbUserId > 0) {
    response.user_entity = PublicKeyCredentialUserEntity(std::vector<uint8_t>(
        assertion.pbUserId, assertion.pbUserId + assertion.cbUserId));
  }
  return response;
}

uint32_t ToWinUserVerificationRequirement(
    UserVerificationRequirement user_verification_requirement) {
  switch (user_verification_requirement) {
    case UserVerificationRequirement::kRequired:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
    case UserVerificationRequirement::kPreferred:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
    case UserVerificationRequirement::kDiscouraged:
      return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
  }
  NOTREACHED();
  return WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
}

uint32_t ToWinAuthenticatorAttachment(
    AuthenticatorAttachment authenticator_attachment) {
  switch (authenticator_attachment) {
    case AuthenticatorAttachment::kAny:
      return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
    case AuthenticatorAttachment::kPlatform:
      return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
    case AuthenticatorAttachment::kCrossPlatform:
      return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  }
  NOTREACHED();
  return WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
}

static uint32_t ToWinTransportsMask(
    const base::flat_set<FidoTransportProtocol>& transports) {
  uint32_t result = 0;
  for (const FidoTransportProtocol transport : transports) {
    switch (transport) {
      case FidoTransportProtocol::kUsbHumanInterfaceDevice:
        result |= WEBAUTHN_CTAP_TRANSPORT_USB;
        break;
      case FidoTransportProtocol::kNearFieldCommunication:
        result |= WEBAUTHN_CTAP_TRANSPORT_NFC;
        break;
      case FidoTransportProtocol::kBluetoothLowEnergy:
        result |= WEBAUTHN_CTAP_TRANSPORT_BLE;
        break;
      case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      case FidoTransportProtocol::kAndroidAccessory:
        // caBLE is unsupported by the Windows API.
        break;
      case FidoTransportProtocol::kInternal:
        result |= WEBAUTHN_CTAP_TRANSPORT_INTERNAL;
        break;
    }
  }
  return result;
}

std::vector<WEBAUTHN_CREDENTIAL> ToWinCredentialVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials) {
  std::vector<WEBAUTHN_CREDENTIAL> result;
  for (const auto& credential : *credentials) {
    if (credential.credential_type() != CredentialType::kPublicKey) {
      continue;
    }
    result.push_back(WEBAUTHN_CREDENTIAL{
        WEBAUTHN_CREDENTIAL_CURRENT_VERSION,
        base::checked_cast<DWORD>(credential.id().size()),
        const_cast<unsigned char*>(credential.id().data()),
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY,
    });
  }
  return result;
}

std::vector<WEBAUTHN_CREDENTIAL_EX> ToWinCredentialExVector(
    const std::vector<PublicKeyCredentialDescriptor>* credentials) {
  std::vector<WEBAUTHN_CREDENTIAL_EX> result;
  for (const auto& credential : *credentials) {
    if (credential.credential_type() != CredentialType::kPublicKey) {
      continue;
    }
    result.push_back(WEBAUTHN_CREDENTIAL_EX{
        WEBAUTHN_CREDENTIAL_EX_CURRENT_VERSION,
        base::checked_cast<DWORD>(credential.id().size()),
        const_cast<unsigned char*>(credential.id().data()),
        WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY,
        ToWinTransportsMask(credential.transports())});
  }
  return result;
}

CtapDeviceResponseCode WinErrorNameToCtapDeviceResponseCode(
    const std::u16string& error_name) {
  // See WebAuthNGetErrorName in <webauthn.h> for these string literals.
  //
  // Note that the set of errors that browser are allowed to return in a
  // response to a WebAuthn call is much narrower than what the Windows
  // WebAuthn API returns.  According to the WebAuthn spec, the only
  // permissible errors are "InvalidStateError" (aka CREDENTIAL_EXCLUDED in
  // Chromium code) and "NotAllowedError". Hence, we can collapse the set of
  // Windows errors to a smaller set of CtapDeviceResponseCodes.
  static base::flat_map<std::u16string, CtapDeviceResponseCode>
      kResponseCodeMap({
          {u"Success", CtapDeviceResponseCode::kSuccess},
          {u"InvalidStateError",
           CtapDeviceResponseCode::kCtap2ErrCredentialExcluded},
          {u"ConstraintError",
           CtapDeviceResponseCode::kCtap2ErrOperationDenied},
          {u"NotSupportedError",
           CtapDeviceResponseCode::kCtap2ErrOperationDenied},
          {u"NotAllowedError",
           CtapDeviceResponseCode::kCtap2ErrOperationDenied},
          {u"UnknownError", CtapDeviceResponseCode::kCtap2ErrOperationDenied},
      });
  if (!base::Contains(kResponseCodeMap, error_name)) {
    FIDO_LOG(ERROR) << "Unexpected error name: " << error_name;
    return CtapDeviceResponseCode::kCtap2ErrOperationDenied;
  }
  return kResponseCodeMap[error_name];
}

COMPONENT_EXPORT(DEVICE_FIDO)
MakeCredentialStatus WinCtapDeviceResponseCodeToMakeCredentialStatus(
    CtapDeviceResponseCode status) {
  switch (status) {
    case CtapDeviceResponseCode::kSuccess:
      return MakeCredentialStatus::kSuccess;
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      return MakeCredentialStatus::kWinInvalidStateError;
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return MakeCredentialStatus::kWinNotAllowedError;
    default:
      NOTREACHED() << "Must only be called with a status returned from "
                      "WinErrorNameToCtapDeviceResponseCode().";
      FIDO_LOG(ERROR) << "Unexpected CtapDeviceResponseCode: "
                      << static_cast<int>(status);
      return MakeCredentialStatus::kWinNotAllowedError;
  }
}

COMPONENT_EXPORT(DEVICE_FIDO)
GetAssertionStatus WinCtapDeviceResponseCodeToGetAssertionStatus(
    CtapDeviceResponseCode status) {
  switch (status) {
    case CtapDeviceResponseCode::kSuccess:
      return GetAssertionStatus::kSuccess;
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return GetAssertionStatus::kWinNotAllowedError;
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      // The API should never return InvalidStateError for GetAssertion.
      FIDO_LOG(ERROR) << "Unexpected CtapDeviceResponseCode: "
                      << static_cast<int>(status);
      return GetAssertionStatus::kWinNotAllowedError;
    default:
      NOTREACHED() << "Must only be called with a status returned from "
                      "WinErrorNameToCtapDeviceResponseCode().";
      FIDO_LOG(ERROR) << "Unexpected CtapDeviceResponseCode: "
                      << static_cast<int>(status);
      return GetAssertionStatus::kWinNotAllowedError;
  }
}

uint32_t ToWinAttestationConveyancePreference(
    const AttestationConveyancePreference& value) {
  switch (value) {
    case AttestationConveyancePreference::kNone:
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
    case AttestationConveyancePreference::kIndirect:
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
    case AttestationConveyancePreference::kDirect:
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
    case AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator:
    case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      // Windows does not support enterprise attestation.
      return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
  }
  NOTREACHED();
  return WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE;
}

}  // namespace device
