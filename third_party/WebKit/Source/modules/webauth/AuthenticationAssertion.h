// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AuthenticationAssertion_h
#define AuthenticationAssertion_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "modules/webauth/ScopedCredential.h"

namespace blink {

class AuthenticationAssertion final
    : public GarbageCollectedFinalized<AuthenticationAssertion>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AuthenticationAssertion* Create(ScopedCredential* credential,
                                         DOMArrayBuffer* client_data,
                                         DOMArrayBuffer* authenticator_data,
                                         DOMArrayBuffer* signature) {
    return new AuthenticationAssertion(credential, client_data,
                                       authenticator_data, signature);
  }

  AuthenticationAssertion(ScopedCredential* credential,
                          DOMArrayBuffer* client_data,
                          DOMArrayBuffer* authenticator_data,
                          DOMArrayBuffer* signature)
      : credential_(credential),
        client_data_(client_data),
        authenticator_data_(authenticator_data),
        signature_(signature) {}

  virtual ~AuthenticationAssertion() {}

  ScopedCredential* credential() const { return credential_.Get(); }
  DOMArrayBuffer* clientData() const { return client_data_.Get(); }
  DOMArrayBuffer* authenticatorData() const {
    return authenticator_data_.Get();
  }
  DOMArrayBuffer* signature() const { return signature_.Get(); }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(credential_);
    visitor->Trace(client_data_);
    visitor->Trace(authenticator_data_);
    visitor->Trace(signature_);
  }

 private:
  const Member<ScopedCredential> credential_;
  const Member<DOMArrayBuffer> client_data_;
  const Member<DOMArrayBuffer> authenticator_data_;
  const Member<DOMArrayBuffer> signature_;
};

}  // namespace blink

#endif  // AuthenticationAssertion_h
