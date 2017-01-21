// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedCredentialInfo_h
#define ScopedCredentialInfo_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"

namespace blink {

class ScopedCredentialInfo final
    : public GarbageCollectedFinalized<ScopedCredentialInfo>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScopedCredentialInfo* create(DOMArrayBuffer* clientData,
                                      DOMArrayBuffer* attestation) {
    return new ScopedCredentialInfo(clientData, attestation);
  }

  ScopedCredentialInfo(DOMArrayBuffer* clientData, DOMArrayBuffer* attestation)
      : m_clientData(clientData), m_attestation(attestation) {}

  virtual ~ScopedCredentialInfo() {}

  DOMArrayBuffer* clientData() const { return m_clientData.get(); }
  DOMArrayBuffer* attestation() const { return m_attestation.get(); }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_clientData);
    visitor->trace(m_attestation);
  }

 private:
  const Member<DOMArrayBuffer> m_clientData;
  const Member<DOMArrayBuffer> m_attestation;
};

}  // namespace blink

#endif  // ScopedCredentialInfo_h
