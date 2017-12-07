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

void RespondToPublicKeyCallback(
    std::unique_ptr<PublicKeyCallbacks> callbacks,
    webauth::mojom::blink::AuthenticatorStatus status,
    webauth::mojom::blink::PublicKeyCredentialInfoPtr credential) {
  if (status != webauth::mojom::AuthenticatorStatus::SUCCESS) {
    DCHECK(!credential);
    callbacks->OnError(mojo::ConvertTo<WebCredentialManagerError>(status));
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
  authenticator_.set_connection_error_handler(
      WTF::Bind(&WebAuthenticationClient::OnAuthenticatorConnectionError,
                WrapWeakPersistent(this)));
}

WebAuthenticationClient::~WebAuthenticationClient() {}

void WebAuthenticationClient::DispatchMakeCredential(
    const MakePublicKeyCredentialOptions& publicKey,
    std::unique_ptr<PublicKeyCallbacks> callbacks) {
  if (!authenticator_) {
    callbacks->OnError(
        WebCredentialManagerError::kWebCredentialManagerNotImplementedError);
    return;
  }

  auto options =
      webauth::mojom::blink::MakePublicKeyCredentialOptions::From(publicKey);
  if (!options) {
    callbacks->OnError(
        WebCredentialManagerError::kWebCredentialManagerNotSupportedError);
    return;
  }

  authenticator_->MakeCredential(std::move(options),
                                 WTF::Bind(&RespondToPublicKeyCallback,
                                           WTF::Passed(std::move(callbacks))));
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
