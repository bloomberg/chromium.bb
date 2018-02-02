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

#include "core/animation/KeyframeEffect.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/unrestricted_double_or_keyframe_effect_options.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/EffectInput.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Element.h"
#include "core/frame/UseCounter.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

KeyframeEffect* KeyframeEffect::Create(
    Element* target,
    KeyframeEffectModelBase* model,
    const Timing& timing,
    KeyframeEffectReadOnly::Priority priority,
    EventDelegate* event_delegate) {
  return new KeyframeEffect(target, model, timing, priority, event_delegate);
}

KeyframeEffect* KeyframeEffect::Create(
    ScriptState* script_state,
    Element* element,
    const ScriptValue& keyframes,
    const UnrestrictedDoubleOrKeyframeEffectOptions& options,
    ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::WebAnimationsAPIEnabled());
  if (element) {
    UseCounter::Count(
        element->GetDocument(),
        WebFeature::kAnimationConstructorKeyframeListEffectObjectTiming);
  }
  Timing timing;
  Document* document = element ? &element->GetDocument() : nullptr;
  if (!TimingInput::Convert(options, timing, document, exception_state))
    return nullptr;

  EffectModel::CompositeOperation composite = EffectModel::kCompositeReplace;
  if (options.IsKeyframeEffectOptions()) {
    composite = EffectModel::ExtractCompositeOperation(
        options.GetAsKeyframeEffectOptions());
  }

  KeyframeEffectModelBase* model = EffectInput::Convert(
      element, keyframes, composite, script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return Create(element, model, timing);
}

KeyframeEffect* KeyframeEffect::Create(ScriptState* script_state,
                                       Element* element,
                                       const ScriptValue& keyframes,
                                       ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::WebAnimationsAPIEnabled());
  if (element) {
    UseCounter::Count(
        element->GetDocument(),
        WebFeature::kAnimationConstructorKeyframeListEffectNoTiming);
  }
  KeyframeEffectModelBase* model =
      EffectInput::Convert(element, keyframes, EffectModel::kCompositeReplace,
                           script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return Create(element, model, Timing());
}

KeyframeEffect* KeyframeEffect::Create(ScriptState* script_state,
                                       KeyframeEffectReadOnly* source,
                                       ExceptionState& exception_state) {
  Timing new_timing = source->SpecifiedTiming();
  KeyframeEffectModelBase* model = source->Model()->Clone();
  return new KeyframeEffect(source->Target(), model, new_timing,
                            source->GetPriority(), source->GetEventDelegate());
}

KeyframeEffect::KeyframeEffect(Element* target,
                               KeyframeEffectModelBase* model,
                               const Timing& timing,
                               KeyframeEffectReadOnly::Priority priority,
                               EventDelegate* event_delegate)
    : KeyframeEffectReadOnly(target, model, timing, priority, event_delegate) {}

KeyframeEffect::~KeyframeEffect() = default;

void KeyframeEffect::setComposite(String composite_string) {
  EffectModel::CompositeOperation composite;
  if (EffectModel::StringToCompositeOperation(composite_string, composite))
    Model()->SetComposite(composite);
}

AnimationEffectTiming* KeyframeEffect::timing() {
  return AnimationEffectTiming::Create(this);
}

}  // namespace blink
