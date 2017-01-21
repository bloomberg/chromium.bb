// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAuthentication_h
#define WebAuthentication_h

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "modules/webauth/AuthenticationAssertionOptions.h"
#include "modules/webauth/RelyingPartyAccount.h"
#include "modules/webauth/ScopedCredentialOptions.h"
#include "modules/webauth/ScopedCredentialParameters.h"

namespace blink {

class LocalFrame;
class ScriptState;
class RelyingPartyAccount;
class AuthenticationAssertionOptions;
class ScopedCredentialParameters;
class ScopedCredentialOptions;

typedef ArrayBufferOrArrayBufferView BufferSource;

class WebAuthentication final
    : public GarbageCollectedFinalized<WebAuthentication>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebAuthentication* create(LocalFrame& frame) {
    return new WebAuthentication(frame);
  }

  virtual ~WebAuthentication();

  void dispose();

  ScriptPromise makeCredential(ScriptState*,
                               const RelyingPartyAccount&,
                               const HeapVector<ScopedCredentialParameters>,
                               const BufferSource&,
                               ScopedCredentialOptions&);
  ScriptPromise getAssertion(ScriptState*,
                             const BufferSource&,
                             const AuthenticationAssertionOptions&);

  DEFINE_INLINE_TRACE() {}

 private:
  explicit WebAuthentication(LocalFrame&);
};

}  // namespace blink

#endif  // WebAuthentication_h
