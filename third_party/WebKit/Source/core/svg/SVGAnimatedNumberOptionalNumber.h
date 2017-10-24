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

#ifndef SVGAnimatedNumberOptionalNumber_h
#define SVGAnimatedNumberOptionalNumber_h

#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGNumberOptionalNumber.h"
#include "platform/heap/Handle.h"

namespace blink {

// SVG Spec: http://www.w3.org/TR/SVG11/types.html <number-optional-number>
// Unlike other SVGAnimated* class, this class is not exposed to Javascript
// directly, while DOM attribute and SMIL animations operate on this class.
// From Javascript, the two SVGAnimatedNumbers |firstNumber| and |secondNumber|
// are used.
// For example, see SVGFEDropShadowElement::stdDeviation{X,Y}()
class SVGAnimatedNumberOptionalNumber
    : public GarbageCollectedFinalized<SVGAnimatedNumberOptionalNumber>,
      public SVGAnimatedPropertyCommon<SVGNumberOptionalNumber> {
  USING_GARBAGE_COLLECTED_MIXIN(SVGAnimatedNumberOptionalNumber);

 public:
  static SVGAnimatedNumberOptionalNumber* Create(
      SVGElement* context_element,
      const QualifiedName& attribute_name,
      float initial_first_value = 0,
      float initial_second_value = 0) {
    return new SVGAnimatedNumberOptionalNumber(context_element, attribute_name,
                                               initial_first_value,
                                               initial_second_value);
  }

  void SetAnimatedValue(SVGPropertyBase*) override;
  bool NeedsSynchronizeAttribute() override;
  void AnimationEnded() override;

  SVGAnimatedNumber* FirstNumber() { return first_number_.Get(); }
  SVGAnimatedNumber* SecondNumber() { return second_number_.Get(); }

  void Trace(blink::Visitor*) override;

 protected:
  SVGAnimatedNumberOptionalNumber(SVGElement* context_element,
                                  const QualifiedName& attribute_name,
                                  float initial_first_value,
                                  float initial_second_value);

  Member<SVGAnimatedNumber> first_number_;
  Member<SVGAnimatedNumber> second_number_;
};

}  // namespace blink

#endif  // SVGAnimatedNumberOptionalNumber_h
