// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/WebAuthentication.h"

#include "bindings/core/v8/ScriptPromise.h"

namespace blink {

WebAuthentication::WebAuthentication(LocalFrame& frame) {}

WebAuthentication::~WebAuthentication() {}

void WebAuthentication::Dispose() {}

ScriptPromise WebAuthentication::makeCredential(
    ScriptState* script_state,
    const RelyingPartyAccount& account_information,
    const HeapVector<ScopedCredentialParameters> crypto_parameters,
    const BufferSource& attestation_challenge,
    ScopedCredentialOptions& options) {
  NOTREACHED();
  return ScriptPromise();
}

ScriptPromise WebAuthentication::getAssertion(
    ScriptState* script_state,
    const BufferSource& assertion_challenge,
    const AuthenticationAssertionOptions& options) {
  NOTREACHED();
  return ScriptPromise();
}

}  // namespace blink
