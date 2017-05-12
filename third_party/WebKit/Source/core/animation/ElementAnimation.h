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

#ifndef ElementAnimation_h
#define ElementAnimation_h

#include "bindings/core/v8/DictionarySequenceOrDictionary.h"
#include "bindings/core/v8/UnrestrictedDoubleOrKeyframeAnimationOptions.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/EffectInput.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExecutionContext.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ElementAnimation {
  STATIC_ONLY(ElementAnimation);

 public:
  static Animation* animate(
      ScriptState* script_state,
      Element& element,
      const DictionarySequenceOrDictionary& effect_input,
      UnrestrictedDoubleOrKeyframeAnimationOptions options,
      ExceptionState& exception_state) {
    EffectModel* effect = EffectInput::Convert(
        &element, effect_input, ExecutionContext::From(script_state),
        exception_state);
    if (exception_state.HadException())
      return nullptr;

    Timing timing;
    if (!TimingInput::Convert(options, timing, &element.GetDocument(),
                              exception_state))
      return nullptr;

    if (options.isKeyframeAnimationOptions()) {
      Animation* animation = animate(element, effect, timing);
      animation->setId(options.getAsKeyframeAnimationOptions().id());
      return animation;
    }
    return animate(element, effect, timing);
  }

  static Animation* animate(ScriptState* script_state,
                            Element& element,
                            const DictionarySequenceOrDictionary& effect_input,
                            ExceptionState& exception_state) {
    EffectModel* effect = EffectInput::Convert(
        &element, effect_input, ExecutionContext::From(script_state),
        exception_state);
    if (exception_state.HadException())
      return nullptr;
    return animate(element, effect, Timing());
  }

  static Animation* animate(Element& element,
                            EffectModel* effect,
                            const Timing& timing) {
    KeyframeEffect* keyframe_effect =
        KeyframeEffect::Create(&element, effect, timing);
    return element.GetDocument().Timeline().Play(keyframe_effect);
  }

  static HeapVector<Member<Animation>> getAnimations(Element& element) {
    HeapVector<Member<Animation>> animations;

    if (!element.HasAnimations())
      return animations;

    for (const auto& animation :
         element.GetDocument().Timeline().getAnimations()) {
      DCHECK(animation->effect());
      if (ToKeyframeEffectReadOnly(animation->effect())->Target() == element &&
          (animation->effect()->IsCurrent() ||
           animation->effect()->IsInEffect()))
        animations.push_back(animation);
    }
    return animations;
  }
};

}  // namespace blink

#endif  // ElementAnimation_h
