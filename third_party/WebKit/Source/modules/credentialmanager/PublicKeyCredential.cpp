// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/PublicKeyCredential.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

namespace {
// https://www.w3.org/TR/webauthn/#dom-publickeycredential-type-slot:
constexpr char kPublicKeyCredentialType[] = "public-key";
}  // namespace

PublicKeyCredential* PublicKeyCredential::Create(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    const AuthenticationExtensionsClientOutputs& extension_outputs) {
  return new PublicKeyCredential(id, raw_id, response, extension_outputs);
}

PublicKeyCredential::PublicKeyCredential(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    const AuthenticationExtensionsClientOutputs& extension_outputs)
    : Credential(id, kPublicKeyCredentialType),
      raw_id_(raw_id),
      response_(response),
      extension_outputs_(extension_outputs) {}

ScriptPromise
PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(
    ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      DOMException::Create(kNotSupportedError, "Operation not implemented."));
}

void PublicKeyCredential::getClientExtensionResults(
    AuthenticationExtensionsClientOutputs& result) const {
  result = extension_outputs_;
}

void PublicKeyCredential::Trace(blink::Visitor* visitor) {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
  Credential::Trace(visitor);
}

bool PublicKeyCredential::IsPublicKeyCredential() const {
  return true;
}

}  // namespace blink
