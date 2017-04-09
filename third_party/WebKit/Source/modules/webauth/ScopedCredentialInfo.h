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
  static ScopedCredentialInfo* Create(DOMArrayBuffer* client_data,
                                      DOMArrayBuffer* attestation) {
    return new ScopedCredentialInfo(client_data, attestation);
  }

  ScopedCredentialInfo(DOMArrayBuffer* client_data, DOMArrayBuffer* attestation)
      : client_data_(client_data), attestation_(attestation) {}

  virtual ~ScopedCredentialInfo() {}

  DOMArrayBuffer* clientData() const { return client_data_.Get(); }
  DOMArrayBuffer* attestation() const { return attestation_.Get(); }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(client_data_);
    visitor->Trace(attestation_);
  }

 private:
  const Member<DOMArrayBuffer> client_data_;
  const Member<DOMArrayBuffer> attestation_;
};

}  // namespace blink

#endif  // ScopedCredentialInfo_h
