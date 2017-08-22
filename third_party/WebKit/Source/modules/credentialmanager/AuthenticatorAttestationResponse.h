// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AuthenticatorAttestationResponse_h
#define AuthenticatorAttestationResponse_h

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/ModulesExport.h"
#include "modules/credentialmanager/AuthenticatorResponse.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class MODULES_EXPORT AuthenticatorAttestationResponse final
    : public AuthenticatorResponse {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AuthenticatorAttestationResponse* Create(
      DOMArrayBuffer* client_data_json,
      DOMArrayBuffer* attestation_object);

  virtual ~AuthenticatorAttestationResponse();

  DOMArrayBuffer* attestationObject() const {
    return attestation_object_.Get();
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AuthenticatorAttestationResponse(DOMArrayBuffer* client_data_json,
                                            DOMArrayBuffer* attestation_object);

  const Member<DOMArrayBuffer> attestation_object_;
};

}  // namespace blink

#endif  // AuthenticatorAttestationResponse_h
