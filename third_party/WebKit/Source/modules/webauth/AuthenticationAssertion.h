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
  static AuthenticationAssertion* create(ScopedCredential* credential,
                                         DOMArrayBuffer* clientData,
                                         DOMArrayBuffer* authenticatorData,
                                         DOMArrayBuffer* signature) {
    return new AuthenticationAssertion(credential, clientData,
                                       authenticatorData, signature);
  }

  AuthenticationAssertion(ScopedCredential* credential,
                          DOMArrayBuffer* clientData,
                          DOMArrayBuffer* authenticatorData,
                          DOMArrayBuffer* signature)
      : m_credential(credential),
        m_clientData(clientData),
        m_authenticatorData(authenticatorData),
        m_signature(signature) {}

  virtual ~AuthenticationAssertion() {}

  ScopedCredential* credential() const { return m_credential.get(); }
  DOMArrayBuffer* clientData() const { return m_clientData.get(); }
  DOMArrayBuffer* authenticatorData() const {
    return m_authenticatorData.get();
  }
  DOMArrayBuffer* signature() const { return m_signature.get(); }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_credential);
    visitor->trace(m_clientData);
    visitor->trace(m_authenticatorData);
    visitor->trace(m_signature);
  }

 private:
  const Member<ScopedCredential> m_credential;
  const Member<DOMArrayBuffer> m_clientData;
  const Member<DOMArrayBuffer> m_authenticatorData;
  const Member<DOMArrayBuffer> m_signature;
};

}  // namespace blink

#endif  // AuthenticationAssertion_h
