// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/PublicKeyCredential.h"

namespace blink {

PublicKeyCredential* PublicKeyCredential::Create(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response) {
  return new PublicKeyCredential(id, raw_id, response);
}

PublicKeyCredential::PublicKeyCredential(const String& id,
                                         DOMArrayBuffer* raw_id,
                                         AuthenticatorResponse* response)
    : Credential(id), raw_id_(raw_id), response_(response) {}

DEFINE_TRACE(PublicKeyCredential) {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
  Credential::Trace(visitor);
}

}  // namespace blink
