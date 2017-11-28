// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/WebAuthenticationClient.h"

#include <utility>
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/credentialmanager/CredentialManagerTypeConverters.h"
#include "modules/credentialmanager/MakePublicKeyCredentialOptions.h"
#include "public/platform/InterfaceProvider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

namespace {
using PublicKeyCallbacks = WebAuthenticationClient::PublicKeyCallbacks;

WebCredentialManagerError GetWebCredentialManagerErrorFromStatus(
    webauth::mojom::blink::AuthenticatorStatus status) {
  switch (status) {
    case webauth::mojom::blink::AuthenticatorStatus::NOT_ALLOWED_ERROR:
      return WebCredentialManagerError::kWebCredentialManagerNotAllowedError;
    case webauth::mojom::blink::AuthenticatorStatus::NOT_SUPPORTED_ERROR:
      return WebCredentialManagerError::kWebCredentialManagerNotSupportedError;
    case webauth::mojom::blink::AuthenticatorStatus::SECURITY_ERROR:
      return WebCredentialManagerError::kWebCredentialManagerSecurityError;
    case webauth::mojom::blink::AuthenticatorStatus::UNKNOWN_ERROR:
      return WebCredentialManagerError::kWebCredentialManagerUnknownError;
    case webauth::mojom::blink::AuthenticatorStatus::CANCELLED:
      return WebCredentialManagerError::kWebCredentialManagerCancelledError;
    case webauth::mojom::blink::AuthenticatorStatus::NOT_IMPLEMENTED:
      return blink::kWebCredentialManagerNotImplementedError;
    case webauth::mojom::blink::AuthenticatorStatus::SUCCESS:
      NOTREACHED();
      break;
  };

  NOTREACHED();
  return blink::WebCredentialManagerError::kWebCredentialManagerUnknownError;
}

void RespondToPublicKeyCallback(
    std::unique_ptr<PublicKeyCallbacks> callbacks,
    webauth::mojom::blink::AuthenticatorStatus status,
    webauth::mojom::blink::PublicKeyCredentialInfoPtr credential) {
  if (status != webauth::mojom::AuthenticatorStatus::SUCCESS) {
    DCHECK(!credential);
    callbacks->OnError(GetWebCredentialManagerErrorFromStatus(status));
    return;
  }

  // Ensure we have an AuthenticatorAttestationResponse
  DCHECK(credential);
  DCHECK(!credential->client_data_json.IsEmpty());
  DCHECK(!credential->response->attestation_object.IsEmpty());
  callbacks->OnSuccess(std::move(credential));
}
}  // namespace

}  // namespace blink

namespace blink {

WebAuthenticationClient::WebAuthenticationClient(LocalFrame& frame) {
  frame.GetInterfaceProvider().GetInterface(mojo::MakeRequest(&authenticator_));
  authenticator_.set_connection_error_handler(ConvertToBaseCallback(
      WTF::Bind(&WebAuthenticationClient::OnAuthenticatorConnectionError,
                WrapWeakPersistent(this))));
}

WebAuthenticationClient::~WebAuthenticationClient() {}

void WebAuthenticationClient::DispatchMakeCredential(
    const MakePublicKeyCredentialOptions& publicKey,
    std::unique_ptr<PublicKeyCallbacks> callbacks) {
  auto options =
      webauth::mojom::blink::MakePublicKeyCredentialOptions::From(publicKey);
  if (!options) {
    callbacks->OnError(
        WebCredentialManagerError::kWebCredentialManagerNotSupportedError);
    return;
  }

  authenticator_->MakeCredential(
      std::move(options),
      ConvertToBaseCallback(WTF::Bind(&RespondToPublicKeyCallback,
                                      WTF::Passed(std::move(callbacks)))));
  return;
}

void WebAuthenticationClient::GetAssertion(
    const PublicKeyCredentialRequestOptions& publicKey,
    PublicKeyCallbacks* callbacks) {
  // TODO (kpaulhamus): implement GetAssertion and removed NOTREACHED().
  NOTREACHED();
  return;
}

void WebAuthenticationClient::OnAuthenticatorConnectionError() {
  Cleanup();
}

void WebAuthenticationClient::Trace(blink::Visitor* visitor) {}

// Closes the Mojo connection.
void WebAuthenticationClient::Cleanup() {
  authenticator_.reset();
}

}  // namespace blink
