// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/AuthenticatorAssertionResponse.h"

namespace blink {

AuthenticatorAssertionResponse* AuthenticatorAssertionResponse::Create(
    DOMArrayBuffer* client_data_json,
    DOMArrayBuffer* authenticator_data,
    DOMArrayBuffer* signature) {
  return new AuthenticatorAssertionResponse(client_data_json,
                                            authenticator_data, signature);
}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(
    DOMArrayBuffer* client_data_json,
    DOMArrayBuffer* authenticator_data,
    DOMArrayBuffer* signature)
    : AuthenticatorResponse(client_data_json),
      authenticator_data_(authenticator_data),
      signature_(signature) {}

AuthenticatorAssertionResponse::~AuthenticatorAssertionResponse() {}

DEFINE_TRACE(AuthenticatorAssertionResponse) {
  visitor->Trace(authenticator_data_);
  visitor->Trace(signature_);
  AuthenticatorResponse::Trace(visitor);
}

}  // namespace blink
