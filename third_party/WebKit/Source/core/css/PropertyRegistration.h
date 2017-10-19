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
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class PropertyDescriptor;

using CSSInterpolationTypes = Vector<std::unique_ptr<CSSInterpolationType>>;

class CORE_EXPORT PropertyRegistration
    : public GarbageCollectedFinalized<PropertyRegistration> {
 public:
  static void registerProperty(ExecutionContext*,
                               const PropertyDescriptor&,
                               ExceptionState&);

  const CSSSyntaxDescriptor& Syntax() const { return syntax_; }
  bool Inherits() const { return inherits_; }
  const CSSValue* Initial() const { return initial_; }
  CSSVariableData* InitialVariableData() const {
    return initial_variable_data_.get();
  }
  const InterpolationTypes& GetInterpolationTypes() const {
    return interpolation_types_;
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(initial_); }

 private:
  PropertyRegistration(const AtomicString& name,
                       const CSSSyntaxDescriptor&,
                       bool inherits,
                       const CSSValue* initial,
                       scoped_refptr<CSSVariableData> initial_variable_data);

  const CSSSyntaxDescriptor syntax_;
  const bool inherits_;
  const Member<const CSSValue> initial_;
  const scoped_refptr<CSSVariableData> initial_variable_data_;
  const InterpolationTypes interpolation_types_;
};

}  // namespace blink

#endif  // PropertyRegistration_h
