// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/fake_webauthn_api.h"

#include "base/logging.h"
#include "base/optional.h"

namespace device {

FakeWinWebAuthnApi::FakeWinWebAuthnApi() = default;
FakeWinWebAuthnApi::~FakeWinWebAuthnApi() = default;

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
  DCHECK(is_available_);
  return E_NOTIMPL;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorGetAssertion(
    HWND h_wnd,
    LPCWSTR rp_id,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    PWEBAUTHN_ASSERTION* assertion_ptr) {
  DCHECK(is_available_);
  return E_NOTIMPL;
}

HRESULT FakeWinWebAuthnApi::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(is_available_);
  return E_NOTIMPL;
}

PCWSTR FakeWinWebAuthnApi::GetErrorName(HRESULT hr) {
  DCHECK(is_available_);
  return L"not implemented";
}

void FakeWinWebAuthnApi::FreeCredentialAttestation(
    PWEBAUTHN_CREDENTIAL_ATTESTATION) {}

void FakeWinWebAuthnApi::FreeAssertion(PWEBAUTHN_ASSERTION pWebAuthNAssertion) {
}

ScopedFakeWinWebAuthnApi::ScopedFakeWinWebAuthnApi() : FakeWinWebAuthnApi() {
  WinWebAuthnApi::SetDefaultForTesting(this);
}

ScopedFakeWinWebAuthnApi::~ScopedFakeWinWebAuthnApi() {
  WinWebAuthnApi::ClearDefaultForTesting();
}

}  // namespace device
