// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "crypto/sha2.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/modules/webauthn/authenticator.mojom.h"
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

class AuthenticatorRequestClientDelegate;
class BrowserContext;
class RenderFrameHost;

namespace client_data {
// These enumerate the possible values for the `type` member of
// CollectedClientData. See
// https://w3c.github.io/webauthn/#dom-collectedclientdata-type
CONTENT_EXPORT extern const char kCreateType[];
CONTENT_EXPORT extern const char kGetType[];
}  // namespace client_data

// Implementation of the public Authenticator interface.
class CONTENT_EXPORT AuthenticatorImpl : public blink::mojom::Authenticator,
                                         public WebContentsObserver {
 public:
  explicit AuthenticatorImpl(RenderFrameHost* render_frame_host);

  // Permits setting connector and timer for testing. Using this constructor
  // will also empty out the protocol set, since no device discovery will take
  // place during tests.
  AuthenticatorImpl(RenderFrameHost* render_frame_host,
                    service_manager::Connector*,
                    std::unique_ptr<base::OneShotTimer>);
  ~AuthenticatorImpl() override;

  // Creates a binding between this implementation and |request|.
  //
  // Note that one AuthenticatorImpl instance can be bound to exactly one
  // interface connection at a time, and disconnected when the frame navigates
  // to a new active document.
  void Bind(blink::mojom::AuthenticatorRequest request);

  base::flat_set<device::FidoTransportProtocol> enabled_transports_for_testing()
      const {
    return transports_;
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

  // mojom:Authenticator
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override;
  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override;

  // Synchronous implementation of IsUserVerfyingPlatformAuthenticatorAvailable.
  bool IsUserVerifyingPlatformAuthenticatorAvailableImpl();

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Callback to handle the async response from a U2fDevice.
  void OnRegisterResponse(
      device::FidoReturnCode status_code,
      base::Optional<device::AuthenticatorMakeCredentialResponse> response_data,
      base::Optional<device::FidoTransportProtocol> transport_used);

  // Callback to complete the registration process once a decision about
  // whether or not to return attestation data has been made.
  void OnRegisterResponseAttestationDecided(
      device::AuthenticatorMakeCredentialResponse response_data,
      bool attestation_permitted);

  // Callback to handle the async response from a U2fDevice.
  void OnSignResponse(
      device::FidoReturnCode status_code,
      base::Optional<device::AuthenticatorGetAssertionResponse> response_data,
      base::Optional<device::FidoTransportProtocol> transport_used);

  void FailWithNotAllowedErrorAndCleanup();

  // Runs when timer expires and cancels all issued requests to a U2fDevice.
  void OnTimeout();
  // Runs when the user cancels WebAuthN request via UI dialog.
  void Cancel();

  void InvokeCallbackAndCleanup(
      MakeCredentialCallback callback,
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
      Focus focus_check);
  void InvokeCallbackAndCleanup(
      GetAssertionCallback callback,
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response);
  void Cleanup();

  base::Optional<device::PlatformAuthenticatorInfo>
  CreatePlatformAuthenticatorIfAvailable();
  base::Optional<device::PlatformAuthenticatorInfo>
  CreatePlatformAuthenticatorIfAvailableAndCheckIfCredentialExists(
      const device::CtapGetAssertionRequest& request);

  BrowserContext* browser_context() const;

  RenderFrameHost* const render_frame_host_;
  service_manager::Connector* connector_ = nullptr;
  const base::flat_set<device::FidoTransportProtocol> transports_;

  std::unique_ptr<device::FidoRequestHandlerBase> request_;
  MakeCredentialCallback make_credential_response_callback_;
  GetAssertionCallback get_assertion_response_callback_;
  std::string client_data_json_;
  blink::mojom::AttestationConveyancePreference attestation_preference_;
  url::Origin caller_origin_;
  std::string relying_party_id_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::Optional<std::string> app_id_;
  // awaiting_attestation_response_ is true if the embedder has been queried
  // about an attestsation decision and the response is still pending.
  bool awaiting_attestation_response_ = false;

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::Binding<blink::mojom::Authenticator> binding_;

  base::WeakPtrFactory<AuthenticatorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
