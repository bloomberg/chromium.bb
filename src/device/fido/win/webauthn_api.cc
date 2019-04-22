// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/webauthn_api.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "device/fido/win/type_conversions.h"

namespace device {

namespace {
base::string16 OptionalGURLToUTF16(const base::Optional<GURL>& in) {
  return in ? base::UTF8ToUTF16(in->spec()) : base::string16();
}
}  // namespace

// Time out all Windows API requests after 5 minutes. We maintain our own
// timeout and cancel the operation when it expires, so this value simply needs
// to be larger than the largest internal request timeout.
constexpr uint32_t kWinWebAuthnTimeoutMilliseconds = 1000 * 60 * 5;

// We do not integrate with older API versions of webauthn.dll because they
// don't support BLE and direct device access to USB and BLE FIDO devices is
// not yet blocked on those platforms.
constexpr uint32_t kMinWinWebAuthnApiVersion = WEBAUTHN_API_VERSION_1;

class WinWebAuthnApiImpl : public WinWebAuthnApi {
 public:
  WinWebAuthnApiImpl() : WinWebAuthnApi(), is_bound_(false) {
    webauthn_dll_ =
        LoadLibraryExA("webauthn.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!webauthn_dll_) {
      return;
    }

#define BIND_FN(fn_pointer, lib_handle, fn_name)       \
  DCHECK(!fn_pointer);                                 \
  fn_pointer = reinterpret_cast<decltype(fn_pointer)>( \
      GetProcAddress(lib_handle, fn_name));

#define BIND_FN_OR_RETURN(fn_pointer, lib_handle, fn_name) \
  BIND_FN(fn_pointer, lib_handle, fn_name);                \
  if (!fn_pointer) {                                       \
    DLOG(ERROR) << "failed to bind " << fn_name;           \
    return;                                                \
  }

    BIND_FN_OR_RETURN(is_user_verifying_platform_authenticator_available_,
                      webauthn_dll_,
                      "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable");
    BIND_FN_OR_RETURN(authenticator_make_credential_, webauthn_dll_,
                      "WebAuthNAuthenticatorMakeCredential");
    BIND_FN_OR_RETURN(authenticator_get_assertion_, webauthn_dll_,
                      "WebAuthNAuthenticatorGetAssertion");
    BIND_FN_OR_RETURN(cancel_current_operation_, webauthn_dll_,
                      "WebAuthNCancelCurrentOperation");
    BIND_FN_OR_RETURN(get_error_name_, webauthn_dll_, "WebAuthNGetErrorName");
    BIND_FN_OR_RETURN(free_credential_attestation_, webauthn_dll_,
                      "WebAuthNFreeCredentialAttestation");
    BIND_FN_OR_RETURN(free_assertion_, webauthn_dll_, "WebAuthNFreeAssertion");

    is_bound_ = true;

    // Determine the API version of webauthn.dll. There is a version currently
    // shipping with Windows RS5 from before WebAuthNGetApiVersionNumber was
    // added (i.e., before WEBAUTHN_API_VERSION_1). Therefore we allow this
    // function to be missing.
    BIND_FN(get_api_version_number_, webauthn_dll_,
            "WebAuthNGetApiVersionNumber");
    api_version_ = get_api_version_number_ ? get_api_version_number_() : 0;
  }

  // WinWebAuthnApi:
  bool IsAvailable() const override {
    return is_bound_ && (api_version_ >= kMinWinWebAuthnApiVersion);
  }

  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) override {
    DCHECK(is_bound_);
    return is_user_verifying_platform_authenticator_available_(available);
  }

  HRESULT AuthenticatorMakeCredential(
      HWND h_wnd,
      PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
      PCWEBAUTHN_USER_ENTITY_INFORMATION user,
      PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) override {
    DCHECK(is_bound_);
    return authenticator_make_credential_(
        h_wnd, rp, user, cose_credential_parameters, client_data, options,
        credential_attestation_ptr);
  }

  HRESULT AuthenticatorGetAssertion(
      HWND h_wnd,
      LPCWSTR rp_id,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      PWEBAUTHN_ASSERTION* assertion_ptr) override {
    DCHECK(is_bound_);
    return authenticator_get_assertion_(h_wnd, rp_id, client_data, options,
                                        assertion_ptr);
  }

  HRESULT CancelCurrentOperation(GUID* cancellation_id) override {
    DCHECK(is_bound_);
    return cancel_current_operation_(cancellation_id);
  }

  PCWSTR GetErrorName(HRESULT hr) override {
    DCHECK(is_bound_);
    return get_error_name_(hr);
  }

  void FreeCredentialAttestation(
      PWEBAUTHN_CREDENTIAL_ATTESTATION attestation_ptr) override {
    DCHECK(is_bound_);
    return free_credential_attestation_(attestation_ptr);
  }
  void FreeAssertion(PWEBAUTHN_ASSERTION assertion_ptr) override {
    DCHECK(is_bound_);
    return free_assertion_(assertion_ptr);
  }

  ~WinWebAuthnApiImpl() override {}

 private:
  bool is_bound_ = false;
  uint32_t api_version_ = 0;
  HMODULE webauthn_dll_;

  decltype(&WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable)
      is_user_verifying_platform_authenticator_available_ = nullptr;
  decltype(
      &WebAuthNAuthenticatorMakeCredential) authenticator_make_credential_ =
      nullptr;
  decltype(&WebAuthNAuthenticatorGetAssertion) authenticator_get_assertion_ =
      nullptr;
  decltype(&WebAuthNCancelCurrentOperation) cancel_current_operation_ = nullptr;
  decltype(&WebAuthNGetErrorName) get_error_name_ = nullptr;
  decltype(&WebAuthNFreeCredentialAttestation) free_credential_attestation_ =
      nullptr;
  decltype(&WebAuthNFreeAssertion) free_assertion_ = nullptr;

  // This method is not available in all versions of webauthn.dll.
  decltype(&WebAuthNGetApiVersionNumber) get_api_version_number_ = nullptr;
};

static WinWebAuthnApi* kDefaultForTesting = nullptr;

// static
WinWebAuthnApi* WinWebAuthnApi::GetDefault() {
  if (kDefaultForTesting) {
    return kDefaultForTesting;
  }

  static base::NoDestructor<WinWebAuthnApiImpl> api;
  return api.get();
}

// static
void WinWebAuthnApi::SetDefaultForTesting(WinWebAuthnApi* api) {
  DCHECK(!kDefaultForTesting);
  kDefaultForTesting = api;
}

// static
void WinWebAuthnApi::ClearDefaultForTesting() {
  DCHECK(kDefaultForTesting);
  kDefaultForTesting = nullptr;
}

WinWebAuthnApi::WinWebAuthnApi() = default;

WinWebAuthnApi::~WinWebAuthnApi() = default;

std::pair<CtapDeviceResponseCode,
          base::Optional<AuthenticatorMakeCredentialResponse>>
AuthenticatorMakeCredentialBlocking(WinWebAuthnApi* webauthn_api,
                                    HWND h_wnd,
                                    GUID cancellation_id,
                                    CtapMakeCredentialRequest request) {
  DCHECK(webauthn_api->IsAvailable());

  base::string16 rp_id = base::UTF8ToUTF16(request.rp().rp_id());
  base::string16 rp_name =
      base::UTF8ToUTF16(request.rp().rp_name().value_or(""));
  base::string16 rp_icon_url = OptionalGURLToUTF16(request.rp().rp_icon_url());
  WEBAUTHN_RP_ENTITY_INFORMATION rp_info{
      WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION, rp_id.c_str(),
      rp_name.c_str(), rp_icon_url.c_str()};

  base::string16 user_name =
      base::UTF8ToUTF16(request.user().name.value_or(""));
  base::string16 user_icon_url = OptionalGURLToUTF16(request.user().icon_url);
  base::string16 user_display_name =
      base::UTF8ToUTF16(request.user().display_name.value_or(""));
  std::vector<uint8_t> user_id = request.user().id;
  WEBAUTHN_USER_ENTITY_INFORMATION user_info{
      WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
      user_id.size(),
      const_cast<unsigned char*>(user_id.data()),
      user_name.c_str(),
      user_icon_url.c_str(),
      user_display_name.c_str(),  // This appears to be ignored.
  };

  std::vector<WEBAUTHN_COSE_CREDENTIAL_PARAMETER>
      cose_credential_parameter_values;
  for (const PublicKeyCredentialParams::CredentialInfo& credential_info :
       request.public_key_credential_params().public_key_credential_params()) {
    if (credential_info.type != CredentialType::kPublicKey) {
      continue;
    }
    cose_credential_parameter_values.push_back(
        {WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION,
         WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY, credential_info.algorithm});
  }
  WEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters{
      cose_credential_parameter_values.size(),
      cose_credential_parameter_values.data()};

  std::string client_data_json = request.client_data_json();
  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, client_data_json.size(),
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(client_data_json.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  std::vector<WEBAUTHN_EXTENSION> extensions;
  if (request.hmac_secret()) {
    static BOOL kHMACSecretTrue = TRUE;
    extensions.emplace_back(
        WEBAUTHN_EXTENSION{WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET,
                           sizeof(BOOL), static_cast<void*>(&kHMACSecretTrue)});
  }

  uint32_t authenticator_attachment;
  if (request.is_u2f_only()) {
    authenticator_attachment =
        WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2;
  } else if (request.is_incognito_mode()) {
    // Disable all platform authenticators in incognito mode. We are going to
    // revisit this in crbug/908622.
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  } else {
    authenticator_attachment =
        ToWinAuthenticatorAttachment(request.authenticator_attachment());
  }

  std::vector<WEBAUTHN_CREDENTIAL_EX> exclude_list_credentials =
      ToWinCredentialExVector(request.exclude_list());
  std::vector<WEBAUTHN_CREDENTIAL_EX*> exclude_list_ptrs;
  std::transform(
      exclude_list_credentials.begin(), exclude_list_credentials.end(),
      std::back_inserter(exclude_list_ptrs), [](auto& cred) { return &cred; });
  WEBAUTHN_CREDENTIAL_LIST exclude_credential_list{exclude_list_ptrs.size(),
                                                   exclude_list_ptrs.data()};

  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options{
      WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3,
      kWinWebAuthnTimeoutMilliseconds,
      WEBAUTHN_CREDENTIALS{
          0, nullptr},  // Ignored because pExcludeCredentialList is set.
      WEBAUTHN_EXTENSIONS{extensions.size(), extensions.data()},
      authenticator_attachment,
      request.resident_key_required(),
      ToWinUserVerificationRequirement(request.user_verification()),
      ToWinAttestationConveyancePreference(request.attestation_preference()),
      /*dwFlags=*/0,
      &cancellation_id,
      &exclude_credential_list,
  };

  WEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation = nullptr;
  std::unique_ptr<WEBAUTHN_CREDENTIAL_ATTESTATION,
                  std::function<void(PWEBAUTHN_CREDENTIAL_ATTESTATION)>>
      credential_attestation_deleter(
          credential_attestation,
          [webauthn_api](PWEBAUTHN_CREDENTIAL_ATTESTATION ptr) {
            webauthn_api->FreeCredentialAttestation(ptr);
          });

  HRESULT hresult = webauthn_api->AuthenticatorMakeCredential(
      h_wnd, &rp_info, &user_info, &cose_credential_parameters, &client_data,
      &options, &credential_attestation);
  if (hresult != S_OK) {
    return {WinErrorNameToCtapDeviceResponseCode(
                base::string16(webauthn_api->GetErrorName(hresult))),
            base::nullopt};
  }
  return {CtapDeviceResponseCode::kSuccess,
          ToAuthenticatorMakeCredentialResponse(*credential_attestation)};
}

std::pair<CtapDeviceResponseCode,
          base::Optional<AuthenticatorGetAssertionResponse>>
AuthenticatorGetAssertionBlocking(WinWebAuthnApi* webauthn_api,
                                  HWND h_wnd,
                                  GUID cancellation_id,
                                  CtapGetAssertionRequest request) {
  DCHECK(webauthn_api->IsAvailable());

  base::string16 rp_id16 = base::UTF8ToUTF16(request.rp_id());
  std::string client_data_json = request.client_data_json();
  WEBAUTHN_CLIENT_DATA client_data{
      WEBAUTHN_CLIENT_DATA_CURRENT_VERSION, client_data_json.size(),
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(client_data_json.data())),
      WEBAUTHN_HASH_ALGORITHM_SHA_256};

  base::Optional<base::string16> opt_app_id16 = base::nullopt;
  if (request.app_id()) {
    opt_app_id16 = base::UTF8ToUTF16(base::StringPiece(
        reinterpret_cast<const char*>(request.app_id()->data()),
        request.app_id()->size()));
  }

  std::vector<WEBAUTHN_CREDENTIAL_EX> allow_list_credentials =
      ToWinCredentialExVector(request.allow_list());
  std::vector<WEBAUTHN_CREDENTIAL_EX*> allow_list_ptrs;
  std::transform(allow_list_credentials.begin(), allow_list_credentials.end(),
                 std::back_inserter(allow_list_ptrs),
                 [](auto& cred) { return &cred; });
  WEBAUTHN_CREDENTIAL_LIST allow_credential_list{allow_list_ptrs.size(),
                                                 allow_list_ptrs.data()};

  auto legacy_credentials = ToWinCredentialVector(request.allow_list());

  uint32_t authenticator_attachment;
  if (opt_app_id16) {
    authenticator_attachment =
        WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM_U2F_V2;
  } else if (request.is_incognito_mode()) {
    // Disable all platform authenticators in incognito mode. We are going to
    // revisit this in crbug/908622.
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  } else {
    authenticator_attachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY;
  }

  static BOOL kUseAppIdTrue = TRUE;    // const
  static BOOL kUseAppIdFalse = FALSE;  // const
  WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options{
      WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_4,
      kWinWebAuthnTimeoutMilliseconds,
      // As of Nov 2018, the WebAuthNAuthenticatorGetAssertion method will
      // fail to challenge credentials via CTAP1 if the allowList is passed
      // in the extended form in WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS
      // (i.e.  pAllowCredentialList instead of CredentialList). The legacy
      // CredentialList field works fine, but does not support setting
      // transport restrictions on the credential descriptor.
      //
      // As a workaround, MS tells us to also set the CredentialList
      // parameter with an accurate cCredentials count and some arbitrary
      // pCredentials data.
      WEBAUTHN_CREDENTIALS{legacy_credentials.size(),
                           legacy_credentials.data()},
      WEBAUTHN_EXTENSIONS{0, nullptr},  // None supported.
      authenticator_attachment,
      ToWinUserVerificationRequirement(request.user_verification()),
      /*dwFlags=*/0,
      opt_app_id16 ? opt_app_id16->data() : nullptr,
      opt_app_id16 ? &kUseAppIdTrue : &kUseAppIdFalse,
      &cancellation_id,
      &allow_credential_list,
  };

  WEBAUTHN_ASSERTION* assertion = nullptr;
  std::unique_ptr<WEBAUTHN_ASSERTION, std::function<void(PWEBAUTHN_ASSERTION)>>
      assertion_deleter(assertion, [webauthn_api](PWEBAUTHN_ASSERTION ptr) {
        webauthn_api->FreeAssertion(ptr);
      });

  HRESULT hresult = webauthn_api->AuthenticatorGetAssertion(
      h_wnd, rp_id16.data(), &client_data, &options, &assertion);
  if (hresult != S_OK) {
    return {WinErrorNameToCtapDeviceResponseCode(
                base::string16(webauthn_api->GetErrorName(hresult))),
            base::nullopt};
  }
  return {CtapDeviceResponseCode::kSuccess,
          ToAuthenticatorGetAssertionResponse(*assertion)};
}

}  // namespace device
