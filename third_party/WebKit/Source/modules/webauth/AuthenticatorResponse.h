// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AuthenticatorResponse_h
#define AuthenticatorResponse_h

#include "core/dom/DOMArrayBuffer.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class MODULES_EXPORT AuthenticatorResponse
    : public GarbageCollectedFinalized<AuthenticatorResponse>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AuthenticatorResponse* Create(DOMArrayBuffer* client_data_json);

  virtual ~AuthenticatorResponse();

  DOMArrayBuffer* clientDataJSON() const { return client_data_json_.Get(); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit AuthenticatorResponse(DOMArrayBuffer* client_data_json);

 private:
  const Member<DOMArrayBuffer> client_data_json_;
};

}  // namespace blink

#endif  // AuthenticatorResponse_h
