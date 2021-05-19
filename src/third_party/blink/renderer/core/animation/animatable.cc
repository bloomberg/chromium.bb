// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animatable.h"

#include "third_party/blink/renderer/bindings/core/v8/unrestricted_double_or_keyframe_animation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/unrestricted_double_or_keyframe_effect_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_get_animations_options.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_input.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/permissions_policy/layout_animations_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {
namespace {

// A helper method which is used to trigger a violation report for cases where
// the |element.animate| API is used to animate a CSS property which is blocked
// by the permissions policy 'layout-animations'.
void ReportPermissionsPolicyViolationsIfNecessary(
    const ExecutionContext& context,
    const KeyframeEffectModelBase& effect) {
  for (const auto& property_handle : effect.Properties()) {
    if (!property_handle.IsCSSProperty())
      continue;
    const auto& css_property = property_handle.GetCSSProperty();
    if (LayoutAnimationsPolicy::AffectedCSSProperties().Contains(
            &css_property)) {
      LayoutAnimationsPolicy::ReportViolation(css_property, context);
    }
  }
}

UnrestrictedDoubleOrKeyframeEffectOptions CoerceEffectOptions(
    UnrestrictedDoubleOrKeyframeAnimationOptions options) {
  if (options.IsKeyframeAnimationOptions()) {
    return UnrestrictedDoubleOrKeyframeEffectOptions::FromKeyframeEffectOptions(
        options.GetAsKeyframeAnimationOptions());
  } else {
    return UnrestrictedDoubleOrKeyframeEffectOptions::FromUnrestrictedDouble(
        options.GetAsUnrestrictedDouble());
  }
}

}  // namespace

// https://drafts.csswg.org/web-animations/#dom-animatable-animate
Animation* Animatable::animate(
    ScriptState* script_state,
    const ScriptValue& keyframes,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return nullptr;
  Element* element = GetAnimationTarget();
  if (!element->GetExecutionContext())
    return nullptr;
  KeyframeEffect* effect =
      KeyframeEffect::Create(script_state, element, keyframes,
                             CoerceEffectOptions(options), exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Creation of the keyframe effect parses JavaScript, which could result
  // in destruction of the execution context. Recheck that it is still valid.
  if (!element->GetExecutionContext())
    return nullptr;

  ReportPermissionsPolicyViolationsIfNecessary(*element->GetExecutionContext(),
                                               *effect->Model());
  if (!options.IsKeyframeAnimationOptions())
    return element->GetDocument().Timeline().Play(effect);

  Animation* animation;
  const KeyframeAnimationOptions* options_dict =
      options.GetAsKeyframeAnimationOptions();
  if (!options_dict->hasTimeline()) {
    animation = element->GetDocument().Timeline().Play(effect);
  } else if (AnimationTimeline* timeline = options_dict->timeline()) {
    animation = timeline->Play(effect);
  } else {
    animation = Animation::Create(element->GetExecutionContext(), effect,
                                  nullptr, exception_state);
  }

  if (!animation)
    return nullptr;

  animation->setId(options_dict->id());
  return animation;
}

Animation* Animatable::animate(ScriptState* script_state,
                               const ScriptValue& keyframes,
                               ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return nullptr;
  Element* element = GetAnimationTarget();
  if (!element->GetExecutionContext())
    return nullptr;
  KeyframeEffect* effect =
      KeyframeEffect::Create(script_state, element, keyframes, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Creation of the keyframe effect parses JavaScript, which could result
  // in destruction of the execution context. Recheck that it is still valid.
  if (!element->GetExecutionContext())
    return nullptr;

  ReportPermissionsPolicyViolationsIfNecessary(*element->GetExecutionContext(),
                                               *effect->Model());
  return element->GetDocument().Timeline().Play(effect);
}

HeapVector<Member<Animation>> Animatable::getAnimations(
    GetAnimationsOptions* options) {
  bool use_subtree = options && options->subtree();
  Element* element = GetAnimationTarget();
  if (use_subtree)
    element->GetDocument().UpdateStyleAndLayoutTreeForSubtree(element);
  else
    element->GetDocument().UpdateStyleAndLayoutTreeForNode(element);

  HeapVector<Member<Animation>> animations;
  if (!use_subtree && !element->HasAnimations())
    return animations;

  for (const auto& animation :
       element->GetDocument().GetDocumentAnimations().getAnimations(
           element->GetTreeScope())) {
    DCHECK(animation->effect());
    // TODO(gtsteel) make this use the idl properties
    Element* target = To<KeyframeEffect>(animation->effect())->EffectTarget();
    if (element == target || (use_subtree && element->contains(target))) {
      // DocumentAnimations::getAnimations should only give us animations that
      // are either current or in effect.
      DCHECK(animation->effect()->IsCurrent() ||
             animation->effect()->IsInEffect());
      animations.push_back(animation);
    }
  }
  return animations;
}

}  // namespace blink
