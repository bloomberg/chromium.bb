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

void FakeWinWebAuthnApi::AuthenticatorMakeCredential(
    HWND h_wnd,
    GUID cancellation_id,
    PublicKeyCredentialRpEntity rp,
    PublicKeyCredentialUserEntity user,
    std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
        cose_credential_parameter_values,
    std::string client_data_json,
    std::vector<WEBAUTHN_EXTENSION> extensions,
    base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list,
    WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
    AuthenticatorMakeCredentialCallback callback) {
  DCHECK(is_available_);
}

void FakeWinWebAuthnApi::AuthenticatorGetAssertion(
    HWND h_wnd,
    GUID cancellation_id,
    base::string16 rp_id,
    base::Optional<base::string16> opt_app_id,
    std::string client_data_json,
    base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list,
    WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    AuthenticatorGetAssertionCallback callback) {
  DCHECK(is_available_);
}

HRESULT FakeWinWebAuthnApi::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(is_available_);
  return E_NOTIMPL;
}

const wchar_t* FakeWinWebAuthnApi::GetErrorName(HRESULT hr) {
  DCHECK(is_available_);
  return L"not implemented";
};

ScopedFakeWinWebAuthnApi::ScopedFakeWinWebAuthnApi() : FakeWinWebAuthnApi() {
  WinWebAuthnApi::SetDefaultForTesting(this);
}

ScopedFakeWinWebAuthnApi::~ScopedFakeWinWebAuthnApi() {
  WinWebAuthnApi::ClearDefaultForTesting();
}

}  // namespace device
