/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGPropertyTearOff_h
#define SVGPropertyTearOff_h

#include "core/dom/QualifiedName.h"
#include "core/svg/SVGElement.h"
#include "core/svg/properties/SVGProperty.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;

enum PropertyIsAnimValType { kPropertyIsNotAnimVal, kPropertyIsAnimVal };

class SVGPropertyTearOffBase
    : public GarbageCollectedFinalized<SVGPropertyTearOffBase> {
 public:
  virtual ~SVGPropertyTearOffBase() {}

  PropertyIsAnimValType PropertyIsAnimVal() const {
    return property_is_anim_val_;
  }

  bool IsAnimVal() const { return property_is_anim_val_ == kPropertyIsAnimVal; }
  bool IsImmutable() const { return IsAnimVal(); }

  virtual void CommitChange();

  SVGElement* contextElement() const { return context_element_; }

  const QualifiedName& AttributeName() { return attribute_name_; }

  void AttachToSVGElementAttribute(SVGElement* context_element,
                                   const QualifiedName& attribute_name) {
    DCHECK(!IsImmutable());
    DCHECK(context_element);
    DCHECK(attribute_name != QualifiedName::Null());
    context_element_ = context_element;
    // Requires SVGPropertyTearOffBase to be the left-most class in the
    // inheritance hierarchy.
    ScriptWrappableVisitor::WriteBarrier(this, context_element_.Get());
    attribute_name_ = attribute_name;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {}

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    visitor->TraceWrappersWithManualWriteBarrier(context_element_.Get());
  }

  static void ThrowReadOnly(ExceptionState&);

 protected:
  SVGPropertyTearOffBase(SVGElement* context_element,
                         PropertyIsAnimValType property_is_anim_val,
                         const QualifiedName& attribute_name)
      : context_element_(context_element),
        property_is_anim_val_(property_is_anim_val),
        attribute_name_(attribute_name) {}

 private:
  // This raw pointer is safe since the SVG element is guaranteed to be kept
  // alive by a V8 wrapper.
  // See http://crbug.com/528275 for the detail.
  UntracedMember<SVGElement> context_element_;

  PropertyIsAnimValType property_is_anim_val_;
  QualifiedName attribute_name_;
};

template <typename Property>
class SVGPropertyTearOff : public SVGPropertyTearOffBase {
 public:
  Property* Target() {
    if (IsAnimVal())
      contextElement()->EnsureAttributeAnimValUpdated();

    return target_.Get();
  }

  void SetTarget(Property* target) { target_ = target; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(target_);
    SVGPropertyTearOffBase::Trace(visitor);
  }

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    SVGPropertyTearOffBase::TraceWrappers(visitor);
  }

 protected:
  SVGPropertyTearOff(Property* target,
                     SVGElement* context_element,
                     PropertyIsAnimValType property_is_anim_val,
                     const QualifiedName& attribute_name)
      : SVGPropertyTearOffBase(context_element,
                               property_is_anim_val,
                               attribute_name),
        target_(target) {
    DCHECK(target_);
  }

 private:
  Member<Property> target_;
};

}  // namespace blink

#endif  // SVGPropertyTearOff_h
