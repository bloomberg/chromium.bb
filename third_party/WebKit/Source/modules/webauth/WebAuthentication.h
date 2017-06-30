// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAuthentication_h
#define WebAuthentication_h

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMArrayBuffer.h"
#include "modules/webauth/AuthenticationAssertionOptions.h"
#include "modules/webauth/ScopedCredentialInfo.h"
#include "platform/bindings/ScriptWrappable.h"
#include "public/platform/modules/webauth/authenticator.mojom-blink.h"

namespace blink {

class RelyingPartyAccount;
class AuthenticationAssertionOptions;
class ScopedCredentialOptions;
class ScopedCredentialParameters;
class ScriptPromiseResolver;

typedef ArrayBufferOrArrayBufferView BufferSource;

class WebAuthentication final
    : public GarbageCollectedFinalized<WebAuthentication>,
      public ScriptWrappable,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WebAuthentication);

 public:
  static WebAuthentication* Create(LocalFrame& frame) {
    return new WebAuthentication(frame);
  }

  virtual ~WebAuthentication();

  // WebAuthentication.idl
  ScriptPromise makeCredential(ScriptState*,
                               const RelyingPartyAccount&,
                               const HeapVector<ScopedCredentialParameters>,
                               const BufferSource&,
                               ScopedCredentialOptions&);
  ScriptPromise getAssertion(ScriptState*,
                             const BufferSource&,
                             const AuthenticationAssertionOptions&);

  webauth::mojom::blink::Authenticator* Authenticator() const {
    return authenticator_.get();
  }

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit WebAuthentication(LocalFrame&);

  void OnMakeCredential(ScriptPromiseResolver*,
                        webauth::mojom::blink::AuthenticatorStatus,
                        webauth::mojom::blink::ScopedCredentialInfoPtr);
  ScriptPromise RejectIfNotSupported(ScriptState*);
  void OnAuthenticatorConnectionError();
  bool MarkRequestComplete(ScriptPromiseResolver*);
  void Cleanup();

  webauth::mojom::blink::AuthenticatorPtr authenticator_;
  HeapHashSet<Member<ScriptPromiseResolver>> authenticator_requests_;
};
}  // namespace blink

#endif  // WebAuthentication_h
