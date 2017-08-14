// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/PublicKeyCredential.h"

namespace blink {

PublicKeyCredential* PublicKeyCredential::Create(
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response) {
  return new PublicKeyCredential(raw_id, response);
}

PublicKeyCredential::PublicKeyCredential(DOMArrayBuffer* raw_id,
                                         AuthenticatorResponse* response)
    : raw_id_(raw_id), response_(response) {}

DEFINE_TRACE(PublicKeyCredential) {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
}

}  // namespace blink
