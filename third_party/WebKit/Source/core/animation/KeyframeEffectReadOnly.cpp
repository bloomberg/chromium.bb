// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/KeyframeEffectReadOnly.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/UnrestrictedDoubleOrKeyframeEffectOptions.h"
#include "core/animation/Animation.h"
#include "core/animation/EffectInput.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/SampledEffect.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Element.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/UseCounter.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGElement.h"

namespace blink {

KeyframeEffectReadOnly* KeyframeEffectReadOnly::Create(
    Element* target,
    EffectModel* model,
    const Timing& timing,
    Priority priority,
    EventDelegate* event_delegate) {
  return new KeyframeEffectReadOnly(target, model, timing, priority,
                                    event_delegate);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::Create(
    ExecutionContext* execution_context,
    Element* element,
    const DictionarySequenceOrDictionary& effect_input,
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
  return Create(element,
                EffectInput::Convert(element, effect_input, execution_context,
                                     exception_state),
                timing);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::Create(
    ExecutionContext* execution_context,
    Element* element,
    const DictionarySequenceOrDictionary& effect_input,
    ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::WebAnimationsAPIEnabled());
  if (element) {
    UseCounter::Count(
        element->GetDocument(),
        WebFeature::kAnimationConstructorKeyframeListEffectNoTiming);
  }
  return Create(element,
                EffectInput::Convert(element, effect_input, execution_context,
                                     exception_state),
                Timing());
}

KeyframeEffectReadOnly::KeyframeEffectReadOnly(Element* target,
                                               EffectModel* model,
                                               const Timing& timing,
                                               Priority priority,
                                               EventDelegate* event_delegate)
    : AnimationEffectReadOnly(timing, event_delegate),
      target_(target),
      model_(model),
      sampled_effect_(nullptr),
      priority_(priority) {}

void KeyframeEffectReadOnly::Attach(Animation* animation) {
  if (target_) {
    target_->EnsureElementAnimations().Animations().insert(animation);
    target_->SetNeedsAnimationStyleRecalc();
    if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() &&
        target_->IsSVGElement())
      ToSVGElement(target_)->SetWebAnimationsPending();
  }
  AnimationEffectReadOnly::Attach(animation);
}

void KeyframeEffectReadOnly::Detach() {
  if (target_)
    target_->GetElementAnimations()->Animations().erase(GetAnimation());
  if (sampled_effect_)
    ClearEffects();
  AnimationEffectReadOnly::Detach();
}

void KeyframeEffectReadOnly::SpecifiedTimingChanged() {
  if (GetAnimation()) {
    // FIXME: Needs to consider groups when added.
    DCHECK_EQ(GetAnimation()->effect(), this);
    GetAnimation()->SetCompositorPending(true);
  }
}

static EffectStack& EnsureEffectStack(Element* element) {
  return element->EnsureElementAnimations().GetEffectStack();
}

bool KeyframeEffectReadOnly::HasMultipleTransformProperties() const {
  if (!target_->GetComputedStyle())
    return false;

  unsigned transform_property_count = 0;
  if (target_->GetComputedStyle()->HasTransformOperations())
    transform_property_count++;
  if (target_->GetComputedStyle()->Rotate())
    transform_property_count++;
  if (target_->GetComputedStyle()->Scale())
    transform_property_count++;
  if (target_->GetComputedStyle()->Translate())
    transform_property_count++;
  return transform_property_count > 1;
}

// Returns true if transform, translate, rotate or scale is composited
// and a motion path or other transform properties
// has been introduced on the element
bool KeyframeEffectReadOnly::HasIncompatibleStyle() {
  if (!target_->GetComputedStyle())
    return false;

  bool affects_transform =
      GetAnimation()->Affects(*target_, CSSPropertyTransform) ||
      GetAnimation()->Affects(*target_, CSSPropertyScale) ||
      GetAnimation()->Affects(*target_, CSSPropertyRotate) ||
      GetAnimation()->Affects(*target_, CSSPropertyTranslate);

  if (GetAnimation()->HasActiveAnimationsOnCompositor()) {
    if (target_->GetComputedStyle()->HasOffset() && affects_transform)
      return true;
    return HasMultipleTransformProperties();
  }

  return false;
}

void KeyframeEffectReadOnly::ApplyEffects() {
  DCHECK(IsInEffect());
  DCHECK(GetAnimation());
  if (!target_ || !model_)
    return;

  if (HasIncompatibleStyle())
    GetAnimation()->CancelAnimationOnCompositor();

  double iteration = CurrentIteration();
  DCHECK_GE(iteration, 0);
  bool changed = false;
  if (sampled_effect_) {
    changed = model_->Sample(clampTo<int>(iteration, 0), Progress(),
                             IterationDuration(),
                             sampled_effect_->MutableInterpolations());
  } else {
    Vector<RefPtr<Interpolation>> interpolations;
    model_->Sample(clampTo<int>(iteration, 0), Progress(), IterationDuration(),
                   interpolations);
    if (!interpolations.IsEmpty()) {
      SampledEffect* sampled_effect = SampledEffect::Create(this);
      sampled_effect->MutableInterpolations().swap(interpolations);
      sampled_effect_ = sampled_effect;
      EnsureEffectStack(target_).Add(sampled_effect);
      changed = true;
    } else {
      return;
    }
  }

  if (changed) {
    target_->SetNeedsAnimationStyleRecalc();
    if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() &&
        target_->IsSVGElement())
      ToSVGElement(*target_).SetWebAnimationsPending();
  }
}

void KeyframeEffectReadOnly::ClearEffects() {
  DCHECK(GetAnimation());
  DCHECK(sampled_effect_);

  sampled_effect_->Clear();
  sampled_effect_ = nullptr;
  RestartAnimationOnCompositor();
  target_->SetNeedsAnimationStyleRecalc();
  if (RuntimeEnabledFeatures::WebAnimationsSVGEnabled() &&
      target_->IsSVGElement())
    ToSVGElement(*target_).ClearWebAnimatedAttributes();
  Invalidate();
}

void KeyframeEffectReadOnly::UpdateChildrenAndEffects() const {
  if (!model_)
    return;
  DCHECK(GetAnimation());
  if (IsInEffect() && !GetAnimation()->EffectSuppressed())
    const_cast<KeyframeEffectReadOnly*>(this)->ApplyEffects();
  else if (sampled_effect_)
    const_cast<KeyframeEffectReadOnly*>(this)->ClearEffects();
}

double KeyframeEffectReadOnly::CalculateTimeToEffectChange(
    bool forwards,
    double local_time,
    double time_to_next_iteration) const {
  const double start_time = SpecifiedTiming().start_delay;
  const double end_time_minus_end_delay = start_time + ActiveDurationInternal();
  const double end_time =
      end_time_minus_end_delay + SpecifiedTiming().end_delay;
  const double after_time = std::min(end_time_minus_end_delay, end_time);

  switch (GetPhase()) {
    case kPhaseNone:
      return std::numeric_limits<double>::infinity();
    case kPhaseBefore:
      DCHECK_GE(start_time, local_time);
      return forwards ? start_time - local_time
                      : std::numeric_limits<double>::infinity();
    case kPhaseActive:
      if (forwards) {
        // Need service to apply fill / fire events.
        const double time_to_end = after_time - local_time;
        if (RequiresIterationEvents()) {
          return std::min(time_to_end, time_to_next_iteration);
        }
        return time_to_end;
      }
      return 0;
    case kPhaseAfter:
      DCHECK_GE(local_time, after_time);
      // If this KeyframeEffect is still in effect then it will need to update
      // when its parent goes out of effect. We have no way of knowing when
      // that will be, however, so the parent will need to supply it.
      return forwards ? std::numeric_limits<double>::infinity()
                      : local_time - after_time;
    default:
      NOTREACHED();
      return std::numeric_limits<double>::infinity();
  }
}

void KeyframeEffectReadOnly::NotifySampledEffectRemovedFromEffectStack() {
  sampled_effect_ = nullptr;
}

CompositorAnimations::FailureCode
KeyframeEffectReadOnly::CheckCanStartAnimationOnCompositor(
    double animation_playback_rate) const {
  if (!Model()) {
    return CompositorAnimations::FailureCode::Actionable(
        "Animation effect has no keyframes");
  }

  if (!target_) {
    return CompositorAnimations::FailureCode::Actionable(
        "Animation effect has no target element");
  }

  if (target_->GetComputedStyle() && target_->GetComputedStyle()->HasOffset()) {
    return CompositorAnimations::FailureCode::Actionable(
        "Accelerated animations do not support elements with offset-position "
        "or offset-path CSS properties");
  }

  // Do not put transforms on compositor if more than one of them are defined
  // in computed style because they need to be explicitly ordered
  if (HasMultipleTransformProperties()) {
    return CompositorAnimations::FailureCode::Actionable(
        "Animation effect applies to multiple transform-related properties");
  }

  return CompositorAnimations::CheckCanStartAnimationOnCompositor(
      SpecifiedTiming(), *target_, GetAnimation(), *Model(),
      animation_playback_rate);
}

void KeyframeEffectReadOnly::StartAnimationOnCompositor(
    int group,
    double start_time,
    double current_time,
    double animation_playback_rate) {
  DCHECK(!HasActiveAnimationsOnCompositor());
  DCHECK(CheckCanStartAnimationOnCompositor(animation_playback_rate).Ok());

  CompositorAnimations::StartAnimationOnCompositor(
      *target_, group, start_time, current_time, SpecifiedTiming(),
      *GetAnimation(), *Model(), compositor_animation_ids_,
      animation_playback_rate);
  DCHECK(!compositor_animation_ids_.IsEmpty());
}

bool KeyframeEffectReadOnly::HasActiveAnimationsOnCompositor() const {
  return !compositor_animation_ids_.IsEmpty();
}

bool KeyframeEffectReadOnly::HasActiveAnimationsOnCompositor(
    const PropertyHandle& property) const {
  return HasActiveAnimationsOnCompositor() && Affects(property);
}

bool KeyframeEffectReadOnly::Affects(const PropertyHandle& property) const {
  return model_ && model_->Affects(property);
}

bool KeyframeEffectReadOnly::CancelAnimationOnCompositor() {
  // FIXME: cancelAnimationOnCompositor is called from withins style recalc.
  // This queries compositingState, which is not necessarily up to date.
  // https://code.google.com/p/chromium/issues/detail?id=339847
  DisableCompositingQueryAsserts disabler;
  if (!HasActiveAnimationsOnCompositor())
    return false;
  if (!target_ || !target_->GetLayoutObject())
    return false;
  DCHECK(GetAnimation());
  for (const auto& compositor_animation_id : compositor_animation_ids_) {
    CompositorAnimations::CancelAnimationOnCompositor(*target_, *GetAnimation(),
                                                      compositor_animation_id);
  }
  compositor_animation_ids_.clear();
  return true;
}

void KeyframeEffectReadOnly::RestartAnimationOnCompositor() {
  if (CancelAnimationOnCompositor())
    GetAnimation()->SetCompositorPending(true);
}

void KeyframeEffectReadOnly::CancelIncompatibleAnimationsOnCompositor() {
  if (target_ && GetAnimation() && Model()) {
    CompositorAnimations::CancelIncompatibleAnimationsOnCompositor(
        *target_, *GetAnimation(), *Model());
  }
}

void KeyframeEffectReadOnly::PauseAnimationForTestingOnCompositor(
    double pause_time) {
  DCHECK(HasActiveAnimationsOnCompositor());
  if (!target_ || !target_->GetLayoutObject())
    return;
  DCHECK(GetAnimation());
  for (const auto& compositor_animation_id : compositor_animation_ids_) {
    CompositorAnimations::PauseAnimationForTestingOnCompositor(
        *target_, *GetAnimation(), compositor_animation_id, pause_time);
  }
}

void KeyframeEffectReadOnly::AttachCompositedLayers() {
  DCHECK(target_);
  DCHECK(GetAnimation());
  CompositorAnimations::AttachCompositedLayers(*target_, *GetAnimation());
}

DEFINE_TRACE(KeyframeEffectReadOnly) {
  visitor->Trace(target_);
  visitor->Trace(model_);
  visitor->Trace(sampled_effect_);
  AnimationEffectReadOnly::Trace(visitor);
}

}  // namespace blink
