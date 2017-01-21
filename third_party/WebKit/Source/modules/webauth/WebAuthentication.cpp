// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webauth/WebAuthentication.h"

#include "bindings/core/v8/ScriptPromise.h"

namespace blink {

WebAuthentication::WebAuthentication(LocalFrame& frame) {}

WebAuthentication::~WebAuthentication() {}

void WebAuthentication::dispose() {}

ScriptPromise WebAuthentication::makeCredential(
    ScriptState* scriptState,
    const RelyingPartyAccount& accountInformation,
    const HeapVector<ScopedCredentialParameters> cryptoParameters,
    const BufferSource& attestationChallenge,
    ScopedCredentialOptions& options) {
  NOTREACHED();
  return ScriptPromise();
}

ScriptPromise WebAuthentication::getAssertion(
    ScriptState* scriptState,
    const BufferSource& assertionChallenge,
    const AuthenticationAssertionOptions& options) {
  NOTREACHED();
  return ScriptPromise();
}

}  // namespace blink
