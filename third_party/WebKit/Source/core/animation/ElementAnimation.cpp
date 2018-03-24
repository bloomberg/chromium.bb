// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ElementAnimation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/Animation.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/EffectInput.h"
#include "core/animation/EffectModel.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Timing.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

Animation* ElementAnimation::animate(
    ScriptState* script_state,
    Element& element,
    const ScriptValue& keyframes,
    UnrestrictedDoubleOrKeyframeAnimationOptions options,
    ExceptionState& exception_state) {
  EffectModel::CompositeOperation composite = EffectModel::kCompositeReplace;
  if (options.IsKeyframeAnimationOptions()) {
    composite = EffectModel::StringToCompositeOperation(
        options.GetAsKeyframeAnimationOptions().composite());
  }

  KeyframeEffectModelBase* effect = EffectInput::Convert(
      &element, keyframes, composite, script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;

  Timing timing;
  if (!TimingInput::Convert(options, timing, &element.GetDocument(),
                            exception_state))
    return nullptr;

  Animation* animation = animateInternal(element, effect, timing);
  if (options.IsKeyframeAnimationOptions())
    animation->setId(options.GetAsKeyframeAnimationOptions().id());
  return animation;
}

Animation* ElementAnimation::animate(ScriptState* script_state,
                                     Element& element,
                                     const ScriptValue& keyframes,
                                     ExceptionState& exception_state) {
  KeyframeEffectModelBase* effect =
      EffectInput::Convert(&element, keyframes, EffectModel::kCompositeReplace,
                           script_state, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return animateInternal(element, effect, Timing());
}

HeapVector<Member<Animation>> ElementAnimation::getAnimations(
    Element& element) {
  element.GetDocument().UpdateStyleAndLayoutTreeForNode(&element);

  HeapVector<Member<Animation>> animations;
  if (!element.HasAnimations())
    return animations;

  for (const auto& animation :
       element.GetDocument().Timeline().getAnimations()) {
    DCHECK(animation->effect());
    if (ToKeyframeEffect(animation->effect())->target() == element &&
        (animation->effect()->IsCurrent() || animation->effect()->IsInEffect()))
      animations.push_back(animation);
  }
  return animations;
}

Animation* ElementAnimation::animateInternal(Element& element,
                                             KeyframeEffectModelBase* effect,
                                             const Timing& timing) {
  KeyframeEffect* keyframe_effect =
      KeyframeEffect::Create(&element, effect, timing);
  return element.GetDocument().Timeline().Play(keyframe_effect);
}

}  // namespace blink
