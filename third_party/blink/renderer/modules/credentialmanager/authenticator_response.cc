// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/authenticator_response.h"

namespace blink {

AuthenticatorResponse* AuthenticatorResponse::Create(
    DOMArrayBuffer* client_data_json) {
  return MakeGarbageCollected<AuthenticatorResponse>(client_data_json);
}

AuthenticatorResponse::AuthenticatorResponse(DOMArrayBuffer* client_data_json)
    : client_data_json_(client_data_json) {}

AuthenticatorResponse::~AuthenticatorResponse() = default;

void AuthenticatorResponse::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_data_json_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
