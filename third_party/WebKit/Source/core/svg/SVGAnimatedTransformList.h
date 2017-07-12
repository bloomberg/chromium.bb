/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGAnimatedTransformList_h
#define SVGAnimatedTransformList_h

#include "core/svg/SVGTransformListTearOff.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

// SVG Spec:
// http://www.w3.org/TR/SVG11/coords.html#InterfaceSVGAnimatedTransformList
class SVGAnimatedTransformList final
    : public SVGAnimatedProperty<SVGTransformList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SVGAnimatedTransformList* Create(
      SVGElement* context_element,
      const QualifiedName& attribute_name,
      CSSPropertyID css_property_id = CSSPropertyInvalid) {
    return new SVGAnimatedTransformList(context_element, attribute_name,
                                        css_property_id);
  }

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    SVGAnimatedProperty<SVGTransformList>::TraceWrappers(visitor);
    ScriptWrappable::TraceWrappers(visitor);
  }

 protected:
  SVGAnimatedTransformList(SVGElement* context_element,
                           const QualifiedName& attribute_name,
                           CSSPropertyID css_property_id)
      : SVGAnimatedProperty<SVGTransformList>(context_element,
                                              attribute_name,
                                              SVGTransformList::Create(),
                                              css_property_id) {}
};

}  // namespace blink

#endif
