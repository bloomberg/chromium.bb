// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PublicKeyCredential_h
#define PublicKeyCredential_h

#include "core/dom/DOMArrayBuffer.h"
#include "modules/ModulesExport.h"
#include "modules/webauth/AuthenticatorResponse.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class AuthenticatorResponse;

class MODULES_EXPORT PublicKeyCredential final
    : public GarbageCollected<PublicKeyCredential>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PublicKeyCredential* Create(DOMArrayBuffer* raw_id,
                                     AuthenticatorResponse*);

  DOMArrayBuffer* rawId() const { return raw_id_.Get(); }
  AuthenticatorResponse* response() const { return response_.Get(); }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PublicKeyCredential(DOMArrayBuffer* raw_id, AuthenticatorResponse*);

  const Member<DOMArrayBuffer> raw_id_;
  const Member<AuthenticatorResponse> response_;
};

}  // namespace blink

#endif  // PublicKeyCredential_h
