// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyRegistry_h
#define PropertyRegistry_h

#include "core/css/PropertyRegistration.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class PropertyRegistry : public GarbageCollected<PropertyRegistry> {
 public:
  static PropertyRegistry* create() { return new PropertyRegistry(); }

  void registerProperty(const AtomicString&,
                        const CSSSyntaxDescriptor&,
                        bool inherits,
                        const CSSValue* initial,
                        PassRefPtr<CSSVariableData> initialVariableData,
                        CSSInterpolationTypes);
  const PropertyRegistration* registration(const AtomicString&) const;
  size_t registrationCount() const { return m_registrations.size(); }

  DEFINE_INLINE_TRACE() { visitor->trace(m_registrations); }

 private:
  HeapHashMap<AtomicString, Member<PropertyRegistration>> m_registrations;
};

}  // namespace blink

#endif  // PropertyRegistry_h
