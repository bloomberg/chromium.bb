// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/fake_webauthn_api.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/values.h"
#include "crypto/sha2.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"

namespace device {

struct FakeWinWebAuthnApi::WebAuthnAssertionEx {
  std::vector<uint8_t> credential_id;
  std::vector<uint8_t> authenticator_data;
  std::vector<uint8_t> signature;
  WEBAUTHN_ASSERTION assertion;
};

FakeWinWebAuthnApi::FakeWinWebAuthnApi() = default;
FakeWinWebAuthnApi::~FakeWinWebAuthnApi() {
  // Ensure callers free unmanaged pointers returned by the real Windows API.
  DCHECK(returned_attestations_.empty());
  DCHECK(returned_assertions_.empty());
}

bool FakeWinWebAuthnApi::InjectNonDiscoverableCredential(
    base::span<const uint8_t> credential_id,
    const std::string& rp_id) {
  bool was_inserted;
  std::tie(std::ignore, was_inserted) = registrations_.insert(
      {fido_parsing_utils::Materialize(credential_id),
       RegistrationData(VirtualFidoDevice::PrivateKey::FreshP256Key(),
                        fido_parsing_utils::CreateSHA256Hash(rp_id),
                        /*counter=*/0)});
  return was_inserted;
}

bool FakeWinWebAuthnApi::InjectDiscoverableCredential(
    base::span<const uint8_t> credential_id,
    device::PublicKeyCredentialRpEntity rp,
    device::PublicKeyCredentialUserEntity user) {
  RegistrationData registration(VirtualFidoDevice::PrivateKey::FreshP256Key(),
                                fido_parsing_utils::CreateSHA256Hash(rp.id),
                                /*counter=*/0);
  registration.is_resident = true;
  registration.user = std::move(user);
  registration.rp = std::move(rp);

  bool was_inserted;
  std::tie(std::ignore, was_inserted) =
      registrations_.insert({fido_parsing_utils::Materialize(credential_id),
                             std::move(registration)});
  return was_inserted;
}

bool FakeWinWebAuthnApi::IsAvailable() const {
  return is_available_;
}
HRESULT FakeWinWebAuthnApi::IsUserVerifyingPlatformAuthenticatorAvailable(
    BOOL* result) {
  DCHECK(is_available_);
  *result = is_uvpaa_;
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorMakeCredential(
    HWND h_wnd,
    PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
    PCWEBAUTHN_USER_ENTITY_INFORMATION user,
    PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
    PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) {
  // TODO(martinkr): Implement to create a credential in |registrations_|.
  DCHECK(is_available_);
  if (result_override_ != S_OK) {
    return result_override_;
  }

  returned_attestations_.push_back(FakeAttestation());
  *credential_attestation_ptr = &returned_attestations_.back();
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorGetAssertion(
    HWND h_wnd,
    LPCWSTR rp_id,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    PWEBAUTHN_ASSERTION* assertion_ptr) {
  // TODO(martinkr): support AppID extension
  DCHECK(is_available_);

  if (result_override_ != S_OK) {
    return result_override_;
  }

  const auto rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(base::WideToUTF8(rp_id));

  RegistrationData* registration = nullptr;
  base::span<const uint8_t> credential_id;
  PCWEBAUTHN_CREDENTIAL_LIST allow_credentials = options->pAllowCredentialList;

  // Find a matching resident credential if allow list is empty. Windows
  // provides its own account selector, so only one credential gets returned.
  // Pretend the user selected the first match.
  if (allow_credentials->cCredentials == 0) {
    for (auto& registration_pair : registrations_) {
      if (!registration_pair.second.is_resident ||
          registration_pair.second.application_parameter != rp_id_hash) {
        continue;
      }
      credential_id = registration_pair.first;
      registration = &registration_pair.second;
      break;
    }
  }

  for (size_t i = 0; i < allow_credentials->cCredentials; i++) {
    PWEBAUTHN_CREDENTIAL_EX credential = allow_credentials->ppCredentials[i];
    base::span<const uint8_t> allow_credential_id(credential->pbId,
                                                  credential->cbId);
    auto it = registrations_.find(allow_credential_id);
    if (it == registrations_.end() ||
        it->second.application_parameter != rp_id_hash) {
      continue;
    }
    credential_id = it->first;
    registration = &it->second;
    break;
  }

  if (!registration) {
    return NTE_NOT_FOUND;
  }
  DCHECK(!credential_id.empty());

  WebAuthnAssertionEx result;
  result.credential_id = fido_parsing_utils::Materialize(credential_id);
  result.authenticator_data =
      AuthenticatorData(
          registration->application_parameter,
          /*user_present=*/true,
          /*user_verified=*/options->dwUserVerificationRequirement !=
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED,
          registration->counter++,
          /*attested_credential_data=*/base::nullopt,
          /*extensions=*/base::nullopt)
          .SerializeToByteArray();

  // Create the assertion signature.
  std::vector<uint8_t> sign_data;
  fido_parsing_utils::Append(&sign_data, result.authenticator_data);
  fido_parsing_utils::Append(
      &sign_data, crypto::SHA256Hash({client_data->pbClientDataJSON,
                                      client_data->cbClientDataJSON}));
  result.signature =
      registration->private_key->Sign({sign_data.data(), sign_data.size()});

  // Fill in the WEBAUTHN_ASSERTION struct returned to the caller.
  result.assertion = {};
  result.assertion.dwVersion = 1;
  result.assertion.cbAuthenticatorData = result.authenticator_data.size();
  result.assertion.pbAuthenticatorData = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(result.authenticator_data.data()));

  result.assertion.cbSignature = result.signature.size();
  result.assertion.pbSignature = result.signature.data();
  result.assertion.Credential = {};
  result.assertion.Credential.dwVersion = 1;
  result.assertion.Credential.cbId = result.credential_id.size();
  result.assertion.Credential.pbId = result.credential_id.data();
  result.assertion.Credential.pwszCredentialType =
      WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
  // TODO(martinkr): Return a user entity for requests with empty allow lists.
  // (Though the CTAP2.0 spec allows that to be omitted if only a single
  // credential matched.)
  result.assertion.pbUserId = nullptr;
  result.assertion.cbUserId = 0;

  // The real API hands out results in naked pointers and asks callers
  // to call FreeAssertion() when they're done. We maintain ownership
  // of the pointees in |returned_assertions_|.
  returned_assertions_.push_back(std::move(result));
  *assertion_ptr = &returned_assertions_.back().assertion;

  return S_OK;
}

HRESULT FakeWinWebAuthnApi::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(is_available_);
  NOTREACHED() << "not implemented";
  return E_NOTIMPL;
}

PCWSTR FakeWinWebAuthnApi::GetErrorName(HRESULT hr) {
  DCHECK(is_available_);
  // See the comment for WebAuthNGetErrorName() in <webauthn.h>.
  switch (hr) {
    case S_OK:
      return L"Success";
    case NTE_EXISTS:
      return L"InvalidStateError";
    case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):
    case NTE_NOT_SUPPORTED:
    case NTE_TOKEN_KEYSET_STORAGE_FULL:
      return L"ConstraintError";
    case NTE_INVALID_PARAMETER:
      return L"NotSupportedError";
    case NTE_DEVICE_NOT_FOUND:
    case NTE_NOT_FOUND:
    case HRESULT_FROM_WIN32(ERROR_CANCELLED):
    case NTE_USER_CANCELLED:
    case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
      return L"NotAllowedError";
    default:
      return L"UnknownError";
  }
}

void FakeWinWebAuthnApi::FreeCredentialAttestation(
    PWEBAUTHN_CREDENTIAL_ATTESTATION credential_attestation) {
  for (auto it = returned_attestations_.begin();
       it != returned_attestations_.end(); ++it) {
    if (credential_attestation != &*it) {
      continue;
    }
    returned_attestations_.erase(it);
    return;
  }
  NOTREACHED();
}

void FakeWinWebAuthnApi::FreeAssertion(PWEBAUTHN_ASSERTION assertion) {
  for (auto it = returned_assertions_.begin(); it != returned_assertions_.end();
       ++it) {
    if (assertion != &it->assertion) {
      continue;
    }
    returned_assertions_.erase(it);
    return;
  }
  NOTREACHED();
}

int FakeWinWebAuthnApi::Version() {
  return version_;
}

// static
WEBAUTHN_CREDENTIAL_ATTESTATION FakeWinWebAuthnApi::FakeAttestation() {
  WEBAUTHN_CREDENTIAL_ATTESTATION attestation = {};
  attestation.dwVersion = WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_1;
  attestation.cbAuthenticatorData =
      sizeof(test_data::kCtap2MakeCredentialAuthData);
  attestation.pbAuthenticatorData = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(device::test_data::kCtap2MakeCredentialAuthData));
  attestation.cbAttestation =
      sizeof(test_data::kPackedAttestationStatementCBOR);
  attestation.pbAttestation = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(device::test_data::kPackedAttestationStatementCBOR));
  attestation.cbAttestationObject = 0;
  attestation.cbCredentialId = 0;
  attestation.pwszFormatType = L"packed";
  attestation.dwAttestationDecodeType = 0;
  return attestation;
}

}  // namespace device
