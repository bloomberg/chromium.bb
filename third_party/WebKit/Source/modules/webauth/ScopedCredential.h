// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedCredential_h
#define ScopedCredential_h

#include "core/dom/DOMArrayBuffer.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class ScopedCredentialType;

class ScopedCredential final
    : public GarbageCollectedFinalized<ScopedCredential>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class ScopedCredentialType { SCOPEDCRED };

  static ScopedCredential* Create(const ScopedCredentialType type,
                                  DOMArrayBuffer* id) {
    return new ScopedCredential(type, id);
  }

  ScopedCredential(const ScopedCredentialType type, DOMArrayBuffer* id)
      : type_(type), id_(id) {}

  virtual ~ScopedCredential() {}

  String type() const {
    DCHECK_EQ(type_, ScopedCredentialType::SCOPEDCRED);
    return "ScopedCred";
  }

  DOMArrayBuffer* id() const { return id_.Get(); }

  ScopedCredentialType StringToCredentialType(const String& type) {
    // There is currently only one type
    return ScopedCredentialType::SCOPEDCRED;
  }

  DEFINE_INLINE_TRACE() { visitor->Trace(id_); }

 private:
  const ScopedCredentialType type_;
  const Member<DOMArrayBuffer> id_;
};

}  // namespace blink

#endif  // ScopedCredential_h
