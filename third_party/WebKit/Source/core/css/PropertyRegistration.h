// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyRegistration_h
#define PropertyRegistration_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/InterpolationTypesMap.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "wtf/Allocator.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class PropertyDescriptor;
class ScriptState;

using CSSInterpolationTypes = Vector<std::unique_ptr<CSSInterpolationType>>;

class PropertyRegistration
    : public GarbageCollectedFinalized<PropertyRegistration> {
 public:
  static void registerProperty(ScriptState*,
                               const PropertyDescriptor&,
                               ExceptionState&);

  PropertyRegistration(const CSSSyntaxDescriptor&,
                       bool inherits,
                       const CSSValue* initial,
                       PassRefPtr<CSSVariableData> initialVariableData,
                       CSSInterpolationTypes);

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

}  // namespace blink

#endif  // PropertyRegistration_h
