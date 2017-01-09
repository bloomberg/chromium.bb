// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyRegistry_h
#define PropertyRegistry_h

#include "core/animation/InterpolationType.h"
#include "core/animation/InterpolationTypesMap.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class PropertyRegistry : public GarbageCollected<PropertyRegistry> {
 public:
  static PropertyRegistry* create() { return new PropertyRegistry(); }

  class Registration : public GarbageCollectedFinalized<Registration> {
   public:
    Registration(const CSSSyntaxDescriptor& syntax,
                 bool inherits,
                 const CSSValue* initial,
                 PassRefPtr<CSSVariableData> initialVariableData,
                 InterpolationTypes interpolationTypes)
        : m_syntax(syntax),
          m_inherits(inherits),
          m_initial(initial),
          m_initialVariableData(initialVariableData),
          m_interpolationTypes(std::move(interpolationTypes)) {}

    const CSSSyntaxDescriptor& syntax() const { return m_syntax; }
    bool inherits() const { return m_inherits; }
    const CSSValue* initial() const { return m_initial; }
    CSSVariableData* initialVariableData() const {
      return m_initialVariableData.get();
    }
    const InterpolationTypes& interpolationTypes() const {
      return m_interpolationTypes;
    }

    DEFINE_INLINE_TRACE() { visitor->trace(m_initial); }

   private:
    const CSSSyntaxDescriptor m_syntax;
    const bool m_inherits;
    const Member<const CSSValue> m_initial;
    const RefPtr<CSSVariableData> m_initialVariableData;
    const InterpolationTypes m_interpolationTypes;
  };

  void registerProperty(const AtomicString&,
                        const CSSSyntaxDescriptor&,
                        bool inherits,
                        const CSSValue* initial,
                        PassRefPtr<CSSVariableData> initialVariableData,
                        InterpolationTypes);
  const Registration* registration(const AtomicString&) const;
  size_t registrationCount() const { return m_registrations.size(); }

  DEFINE_INLINE_TRACE() { visitor->trace(m_registrations); }

 private:
  HeapHashMap<AtomicString, Member<Registration>> m_registrations;
};

}  // namespace blink

#endif  // PropertyRegistry_h
