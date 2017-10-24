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

#ifndef SVGAnimatedIntegerOptionalInteger_h
#define SVGAnimatedIntegerOptionalInteger_h

#include "core/svg/SVGAnimatedInteger.h"
#include "core/svg/SVGIntegerOptionalInteger.h"
#include "platform/heap/Handle.h"

namespace blink {

// SVG Spec: http://www.w3.org/TR/SVG11/types.html <number-optional-number>
// Unlike other SVGAnimated* class, this class is not exposed to Javascript
// directly, while DOM attribute and SMIL animations operate on this class.
// From Javascript, the two SVGAnimatedIntegers |firstInteger| and
// |secondInteger| are used.
// For example, see SVGFEDropShadowElement::stdDeviation{X,Y}()
class SVGAnimatedIntegerOptionalInteger
    : public GarbageCollectedFinalized<SVGAnimatedIntegerOptionalInteger>,
      public SVGAnimatedPropertyCommon<SVGIntegerOptionalInteger> {
  USING_GARBAGE_COLLECTED_MIXIN(SVGAnimatedIntegerOptionalInteger);

 public:
  static SVGAnimatedIntegerOptionalInteger* Create(
      SVGElement* context_element,
      const QualifiedName& attribute_name,
      float initial_first_value = 0,
      float initial_second_value = 0) {
    return new SVGAnimatedIntegerOptionalInteger(
        context_element, attribute_name, initial_first_value,
        initial_second_value);
  }

  void SetAnimatedValue(SVGPropertyBase*) override;
  bool NeedsSynchronizeAttribute() override;
  void AnimationEnded() override;

  SVGAnimatedInteger* FirstInteger() { return first_integer_.Get(); }
  SVGAnimatedInteger* SecondInteger() { return second_integer_.Get(); }

  void Trace(blink::Visitor*) override;

 protected:
  SVGAnimatedIntegerOptionalInteger(SVGElement* context_element,
                                    const QualifiedName& attribute_name,
                                    float initial_first_value,
                                    float initial_second_value);

  Member<SVGAnimatedInteger> first_integer_;
  Member<SVGAnimatedInteger> second_integer_;
};

}  // namespace blink

#endif  // SVGAnimatedIntegerOptionalInteger_h
