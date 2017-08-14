// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/AuthenticatorResponse.h"

namespace blink {

AuthenticatorResponse* AuthenticatorResponse::Create(
    DOMArrayBuffer* client_data_json) {
  return new AuthenticatorResponse(client_data_json);
}

AuthenticatorResponse::AuthenticatorResponse(DOMArrayBuffer* client_data_json)
    : client_data_json_(client_data_json) {}

AuthenticatorResponse::~AuthenticatorResponse() {}

DEFINE_TRACE(AuthenticatorResponse) {
  visitor->Trace(client_data_json_);
}

}  // namespace blink
