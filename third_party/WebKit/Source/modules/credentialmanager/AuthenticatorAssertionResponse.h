// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AuthenticatorAssertionResponse_h
#define AuthenticatorAssertionResponse_h

#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/ModulesExport.h"
#include "modules/credentialmanager/AuthenticatorResponse.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class MODULES_EXPORT AuthenticatorAssertionResponse final
    : public AuthenticatorResponse {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AuthenticatorAssertionResponse* Create(
      DOMArrayBuffer* client_data_json,
      DOMArrayBuffer* authenticator_data,
      DOMArrayBuffer* signature,
      DOMArrayBuffer* user_handle);

  virtual ~AuthenticatorAssertionResponse();

  DOMArrayBuffer* authenticatorData() const {
    return authenticator_data_.Get();
  }

  DOMArrayBuffer* signature() const { return signature_.Get(); }

  DOMArrayBuffer* userHandle() const { return user_handle_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit AuthenticatorAssertionResponse(DOMArrayBuffer* client_data_json,
                                          DOMArrayBuffer* authenticator_data,
                                          DOMArrayBuffer* signature,
                                          DOMArrayBuffer* user_handle);
  const Member<DOMArrayBuffer> authenticator_data_;
  const Member<DOMArrayBuffer> signature_;
  const Member<DOMArrayBuffer> user_handle_;
};

}  // namespace blink

#endif  // AuthenticatorAssertionResponse_h
