// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedCredential_h
#define ScopedCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"

namespace blink {

class ScopedCredentialType;

class ScopedCredential final
    : public GarbageCollectedFinalized<ScopedCredential>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class ScopedCredentialType { SCOPEDCRED };

  static ScopedCredential* create(const ScopedCredentialType type,
                                  DOMArrayBuffer* id) {
    return new ScopedCredential(type, id);
  }

  ScopedCredential(const ScopedCredentialType type, DOMArrayBuffer* id)
      : m_type(type), m_id(id) {}

  virtual ~ScopedCredential() {}

  String type() const {
    DCHECK_EQ(m_type, ScopedCredentialType::SCOPEDCRED);
    return "ScopedCred";
  }

  DOMArrayBuffer* id() const { return m_id.get(); }

  ScopedCredentialType stringToCredentialType(const String& type) {
    // There is currently only one type
    return ScopedCredentialType::SCOPEDCRED;
  }

  DEFINE_INLINE_TRACE() { visitor->trace(m_id); }

 private:
  const ScopedCredentialType m_type;
  const Member<DOMArrayBuffer> m_id;
};

}  // namespace blink

#endif  // ScopedCredential_h
