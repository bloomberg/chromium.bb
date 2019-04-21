// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace base {
class OneShotTimer;
}

namespace device {

struct PlatformAuthenticatorInfo;
class CtapGetAssertionRequest;
class FidoRequestHandlerBase;

enum class FidoReturnCode : uint8_t;

}  // namespace device

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class RenderFrameHost;

namespace client_data {
// These enumerate the possible values for the `type` member of
// CollectedClientData. See
// https://w3c.github.io/webauthn/#dom-collectedclientdata-type
CONTENT_EXPORT extern const char kCreateType[];
CONTENT_EXPORT extern const char kGetType[];
}  // namespace client_data

// Common code for any WebAuthn Authenticator interfaces.
class CONTENT_EXPORT AuthenticatorCommon {
 public:
  // Permits setting connector and timer for testing.
  AuthenticatorCommon(RenderFrameHost* render_frame_host,
                      service_manager::Connector*,
                      std::unique_ptr<base::OneShotTimer>);
  virtual ~AuthenticatorCommon();

  // This is not-quite an implementation of blink::mojom::Authenticator. The
  // first two functions take the caller's origin explicitly. This allows the
  // caller origin to be overridden if needed.
  void MakeCredential(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback);
  void GetAssertion(url::Origin caller_origin,
                    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    blink::mojom::Authenticator::GetAssertionCallback callback);
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback);

  // Synchronous implementation of
  // IsUserVerifyingPlatformAuthenticatorAvailable.
  bool IsUserVerifyingPlatformAuthenticatorAvailableImpl(
      AuthenticatorRequestClientDelegate* request_delegate);

  void Cleanup();

  base::flat_set<device::FidoTransportProtocol> enabled_transports_for_testing()
      const {
    return transports_;
  }
  void set_transports_for_testing(
      base::flat_set<device::FidoTransportProtocol> transports) {
    transports_ = transports;
  }

 protected:
  virtual void UpdateRequestDelegate();

  std::unique_ptr<AuthenticatorRequestClientDelegate> request_delegate_;

 private:
  friend class AuthenticatorImplTest;

  // Enumerates whether or not to check that the WebContents has focus.
  enum class Focus {
    kDoCheck,
    kDontCheck,
  };

  bool IsFocused() const;

  // Builds the CollectedClientData[1] dictionary with the given values,
  // serializes it to JSON, and returns the resulting string. For legacy U2F
  // requests coming from the CryptoToken U2F extension, modifies the object key
  // 'type' as required[2].
  // [1] https://w3c.github.io/webauthn/#dictdef-collectedclientdata
  // [2]
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#client-data
  static std::string SerializeCollectedClientDataToJson(
      const std::string& type,
      const std::string& origin,
      base::span<const uint8_t> challenge,
      bool use_legacy_u2f_type_key = false);

  // Callback to handle the async response from a U2fDevice.
  void OnRegisterResponse(
      device::FidoReturnCode status_code,
      base::Optional<device::AuthenticatorMakeCredentialResponse> response_data,
      base::Optional<device::FidoTransportProtocol> transport_used);

  // Callback to complete the registration process once a decision about
  // whether or not to return attestation data has been made.
  void OnRegisterResponseAttestationDecided(
      device::AuthenticatorMakeCredentialResponse response_data,
      bool is_transport_used_internal,
      bool attestation_permitted);

  // Callback to handle the async response from a U2fDevice.
  void OnSignResponse(
      device::FidoReturnCode status_code,
      base::Optional<std::vector<device::AuthenticatorGetAssertionResponse>>
          response_data,
      base::Optional<device::FidoTransportProtocol> transport_used);

  void FailWithErrorAndCleanup();

  // Runs when timer expires and cancels all issued requests to a U2fDevice.
  void OnTimeout();
  // Runs when the user cancels WebAuthN request via UI dialog.
  void Cancel();

  // Called when a GetAssertion has completed, either because an allow_list was
  // used and so an answer is returned directly, or because the user selected an
  // account from the options.
  void OnAccountSelected(device::AuthenticatorGetAssertionResponse response);

  // Decides whether or not UI is present that needs to block on user
  // acknowledgement before returning the error, and handles the error
  // appropriately.
  void SignalFailureToRequestDelegate(
      AuthenticatorRequestClientDelegate::InterestingFailureReason reason);

  void InvokeCallbackAndCleanup(
      blink::mojom::Authenticator::MakeCredentialCallback callback,
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::MakeCredentialAuthenticatorResponsePtr response = nullptr,
      Focus focus_check = Focus::kDontCheck);
  void InvokeCallbackAndCleanup(
      blink::mojom::Authenticator::GetAssertionCallback callback,
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response = nullptr);

  base::Optional<device::PlatformAuthenticatorInfo>
  CreatePlatformAuthenticatorIfAvailable();
  base::Optional<device::PlatformAuthenticatorInfo>
  CreatePlatformAuthenticatorIfAvailableAndCheckIfCredentialExists(
      const device::CtapGetAssertionRequest& request);

  BrowserContext* browser_context() const;

  RenderFrameHost* const render_frame_host_;
  service_manager::Connector* connector_ = nullptr;
  base::flat_set<device::FidoTransportProtocol> transports_;

  std::unique_ptr<device::FidoRequestHandlerBase> request_;
  blink::mojom::Authenticator::MakeCredentialCallback
      make_credential_response_callback_;
  blink::mojom::Authenticator::GetAssertionCallback
      get_assertion_response_callback_;
  std::string client_data_json_;
  bool attestation_requested_;
  url::Origin caller_origin_;
  std::string relying_party_id_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::Optional<std::string> app_id_;
  // awaiting_attestation_response_ is true if the embedder has been queried
  // about an attestsation decision and the response is still pending.
  bool awaiting_attestation_response_ = false;
  blink::mojom::AuthenticatorStatus error_awaiting_user_acknowledgement_ =
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;

  base::WeakPtrFactory<AuthenticatorCommon> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorCommon);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_H_
