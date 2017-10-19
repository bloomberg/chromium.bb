// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PublicKeyCredential_h
#define PublicKeyCredential_h

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/ModulesExport.h"
#include "modules/credentialmanager/AuthenticatorResponse.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"

namespace blink {

class AuthenticatorResponse;

class MODULES_EXPORT PublicKeyCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PublicKeyCredential* Create(const String& id,
                                     DOMArrayBuffer* raw_id,
                                     AuthenticatorResponse*);

  DOMArrayBuffer* rawId() const { return raw_id_.Get(); }
  AuthenticatorResponse* response() const { return response_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit PublicKeyCredential(const String& id,
                               DOMArrayBuffer* raw_id,
                               AuthenticatorResponse*);

  const Member<DOMArrayBuffer> raw_id_;
  const Member<AuthenticatorResponse> response_;
};

}  // namespace blink

#endif  // PublicKeyCredential_h
