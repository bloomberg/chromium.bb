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

#include "core/animation/css/CSSAnimations.h"

#include <algorithm>
#include <bitset>
#include "core/StylePropertyShorthand.h"
#include "core/animation/Animation.h"
#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/InertEffect.h"
#include "core/animation/Interpolation.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/TransitionInterpolation.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSPropertyEquality.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValueList.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/StyleEngine.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/PseudoElement.h"
#include "core/events/AnimationEvent.h"
#include "core/events/TransitionEvent.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/PaintLayer.h"
#include "platform/Histogram.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/HashSet.h"
#include "public/platform/Platform.h"

namespace blink {

using PropertySet = HashSet<CSSPropertyID>;

namespace {

StringKeyframeEffectModel* CreateKeyframeEffectModel(
    StyleResolver* resolver,
    const Element* animating_element,
    Element& element,
    const ComputedStyle* style,
    const ComputedStyle* parent_style,
    const AtomicString& name,
    TimingFunction* default_timing_function,
    size_t animation_index) {
  // When the animating element is null, use its parent for scoping purposes.
  const Element* element_for_scoping =
      animating_element ? animating_element : &element;
  const StyleRuleKeyframes* keyframes_rule =
      resolver->FindKeyframesRule(element_for_scoping, name);
  DCHECK(keyframes_rule);

  StringKeyframeVector keyframes;
  const HeapVector<Member<StyleRuleKeyframe>>& style_keyframes =
      keyframes_rule->Keyframes();

  // Construct and populate the style for each keyframe
  PropertySet specified_properties_for_use_counter;
  for (size_t i = 0; i < style_keyframes.size(); ++i) {
    const StyleRuleKeyframe* style_keyframe = style_keyframes[i].Get();
    RefPtr<StringKeyframe> keyframe = StringKeyframe::Create();
    const Vector<double>& offsets = style_keyframe->Keys();
    DCHECK(!offsets.IsEmpty());
    keyframe->SetOffset(offsets[0]);
    keyframe->SetEasing(default_timing_function);
    const StylePropertySet& properties = style_keyframe->Properties();
    for (unsigned j = 0; j < properties.PropertyCount(); j++) {
      CSSPropertyID property = properties.PropertyAt(j).Id();
      specified_properties_for_use_counter.insert(property);
      if (property == CSSPropertyAnimationTimingFunction) {
        const CSSValue& value = properties.PropertyAt(j).Value();
        RefPtr<TimingFunction> timing_function;
        if (value.IsInheritedValue() && parent_style->Animations()) {
          timing_function = parent_style->Animations()->TimingFunctionList()[0];
        } else if (value.IsValueList()) {
          timing_function = CSSToStyleMap::MapAnimationTimingFunction(
              ToCSSValueList(value).Item(0));
        } else {
          DCHECK(value.IsCSSWideKeyword());
          timing_function = CSSTimingData::InitialTimingFunction();
        }
        keyframe->SetEasing(std::move(timing_function));
      } else if (!CSSAnimations::IsAnimationAffectingProperty(property)) {
        keyframe->SetCSSPropertyValue(property,
                                      properties.PropertyAt(j).Value());
      }
    }
    keyframes.push_back(keyframe);
    // The last keyframe specified at a given offset is used.
    for (size_t j = 1; j < offsets.size(); ++j) {
      keyframes.push_back(
          ToStringKeyframe(keyframe->CloneWithOffset(offsets[j]).Get()));
    }
  }

  DEFINE_STATIC_LOCAL(SparseHistogram, property_histogram,
                      ("WebCore.Animation.CSSProperties"));
  for (CSSPropertyID property : specified_properties_for_use_counter) {
    DCHECK(isValidCSSPropertyID(property));
    UseCounter::CountAnimatedCSS(element_for_scoping->GetDocument(), property);

    // TODO(crbug.com/458925): Remove legacy histogram and counts
    property_histogram.Sample(
        UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(property));
  }

  // Merge duplicate keyframes.
  std::stable_sort(keyframes.begin(), keyframes.end(),
                   Keyframe::CompareOffsets);
  size_t target_index = 0;
  for (size_t i = 1; i < keyframes.size(); i++) {
    if (keyframes[i]->Offset() == keyframes[target_index]->Offset()) {
      for (const auto& property : keyframes[i]->Properties())
        keyframes[target_index]->SetCSSPropertyValue(
            property.CssProperty(), keyframes[i]->CssPropertyValue(property));
    } else {
      target_index++;
      keyframes[target_index] = keyframes[i];
    }
  }
  if (!keyframes.IsEmpty())
    keyframes.Shrink(target_index + 1);

  // Add 0% and 100% keyframes if absent.
  RefPtr<StringKeyframe> start_keyframe =
      keyframes.IsEmpty() ? nullptr : keyframes[0];
  if (!start_keyframe || keyframes[0]->Offset() != 0) {
    start_keyframe = StringKeyframe::Create();
    start_keyframe->SetOffset(0);
    start_keyframe->SetEasing(default_timing_function);
    keyframes.push_front(start_keyframe);
  }
  RefPtr<StringKeyframe> end_keyframe = keyframes[keyframes.size() - 1];
  if (end_keyframe->Offset() != 1) {
    end_keyframe = StringKeyframe::Create();
    end_keyframe->SetOffset(1);
    end_keyframe->SetEasing(default_timing_function);
    keyframes.push_back(end_keyframe);
  }
  DCHECK_GE(keyframes.size(), 2U);
  DCHECK(!keyframes.front()->Offset());
  DCHECK_EQ(keyframes.back()->Offset(), 1);

  StringKeyframeEffectModel* model =
      StringKeyframeEffectModel::Create(keyframes, &keyframes[0]->Easing());
  if (animation_index > 0 && model->HasSyntheticKeyframes()) {
    UseCounter::Count(element_for_scoping->GetDocument(),
                      WebFeature::kCSSAnimationsStackedNeutralKeyframe);
  }
  return model;
}

}  // namespace

CSSAnimations::CSSAnimations() {}

bool CSSAnimations::IsAnimationForInspector(const Animation& animation) {
  for (const auto& running_animation : running_animations_) {
    if (running_animation->animation->SequenceNumber() ==
        animation.SequenceNumber())
      return true;
  }
  return false;
}

bool CSSAnimations::IsTransitionAnimationForInspector(
    const Animation& animation) const {
  for (const auto& it : transitions_) {
    if (it.value.animation->SequenceNumber() == animation.SequenceNumber())
      return true;
  }
  return false;
}

namespace {

const KeyframeEffectModelBase* GetKeyframeEffectModelBase(
    const AnimationEffectReadOnly* effect) {
  if (!effect)
    return nullptr;
  const EffectModel* model = nullptr;
  if (effect->IsKeyframeEffectReadOnly())
    model = ToKeyframeEffectReadOnly(effect)->Model();
  else if (effect->IsInertEffect())
    model = ToInertEffect(effect)->Model();
  if (!model || !model->IsKeyframeEffectModel())
    return nullptr;
  return ToKeyframeEffectModelBase(model);
}

}  // namespace

void CSSAnimations::CalculateCompositorAnimationUpdate(
    CSSAnimationUpdate& update,
    const Element* animating_element,
    Element& element,
    const ComputedStyle& style,
    const ComputedStyle* parent_style,
    bool was_viewport_resized) {
  ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;

  // We only update compositor animations in response to changes in the base
  // style.
  if (!element_animations || element_animations->IsAnimationStyleChange())
    return;

  if (!animating_element->GetLayoutObject() ||
      !animating_element->GetLayoutObject()->Style())
    return;

  const ComputedStyle& old_style =
      *animating_element->GetLayoutObject()->Style();
  if (!old_style.ShouldCompositeForCurrentAnimations())
    return;

  bool transform_zoom_changed =
      old_style.HasCurrentTransformAnimation() &&
      old_style.EffectiveZoom() != style.EffectiveZoom();
  for (auto& entry : element_animations->Animations()) {
    Animation& animation = *entry.key;
    const KeyframeEffectModelBase* keyframe_effect =
        GetKeyframeEffectModelBase(animation.effect());
    if (!keyframe_effect)
      continue;

    bool update_compositor_keyframes = false;
    if ((transform_zoom_changed || was_viewport_resized) &&
        (keyframe_effect->Affects(PropertyHandle(CSSPropertyTransform)) ||
         keyframe_effect->Affects(PropertyHandle(CSSPropertyTranslate))) &&
        keyframe_effect->SnapshotAllCompositorKeyframes(element, style,
                                                        parent_style)) {
      update_compositor_keyframes = true;
    } else if (keyframe_effect->HasSyntheticKeyframes() &&
               keyframe_effect->SnapshotNeutralCompositorKeyframes(
                   element, old_style, style, parent_style)) {
      update_compositor_keyframes = true;
    }

    if (update_compositor_keyframes)
      update.UpdateCompositorKeyframes(&animation);
  }
}

void CSSAnimations::CalculateAnimationUpdate(CSSAnimationUpdate& update,
                                             const Element* animating_element,
                                             Element& element,
                                             const ComputedStyle& style,
                                             const ComputedStyle* parent_style,
                                             StyleResolver* resolver) {
  const ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;

  bool is_animation_style_change =
      element_animations && element_animations->IsAnimationStyleChange();

#if !DCHECK_IS_ON()
  // If we're in an animation style change, no animations can have started, been
  // cancelled or changed play state. When DCHECK is enabled, we verify this
  // optimization.
  if (is_animation_style_change) {
    CalculateAnimationActiveInterpolations(update, animating_element);
    return;
  }
#endif

  const CSSAnimationData* animation_data = style.Animations();
  const CSSAnimations* css_animations =
      element_animations ? &element_animations->CssAnimations() : nullptr;
  const Element* element_for_scoping =
      animating_element ? animating_element : &element;

  Vector<bool> cancel_running_animation_flags(
      css_animations ? css_animations->running_animations_.size() : 0);
  for (bool& flag : cancel_running_animation_flags)
    flag = true;

  if (animation_data && style.Display() != EDisplay::kNone) {
    const Vector<AtomicString>& name_list = animation_data->NameList();
    for (size_t i = 0; i < name_list.size(); ++i) {
      AtomicString name = name_list[i];
      if (name == CSSAnimationData::InitialName())
        continue;

      // Find n where this is the nth occurence of this animation name.
      size_t name_index = 0;
      for (size_t j = 0; j < i; j++) {
        if (name_list[j] == name)
          name_index++;
      }

      const bool is_paused =
          CSSTimingData::GetRepeated(animation_data->PlayStateList(), i) ==
          kAnimPlayStatePaused;

      Timing timing = animation_data->ConvertToTiming(i);
      Timing specified_timing = timing;
      RefPtr<TimingFunction> keyframe_timing_function = timing.timing_function;
      timing.timing_function = Timing::Defaults().timing_function;

      StyleRuleKeyframes* keyframes_rule =
          resolver->FindKeyframesRule(element_for_scoping, name);
      if (!keyframes_rule)
        continue;  // Cancel the animation if there's no style rule for it.

      const RunningAnimation* existing_animation = nullptr;
      size_t existing_animation_index = 0;

      if (css_animations) {
        for (size_t i = 0; i < css_animations->running_animations_.size();
             i++) {
          const RunningAnimation& running_animation =
              *css_animations->running_animations_[i];
          if (running_animation.name == name &&
              running_animation.name_index == name_index) {
            existing_animation = &running_animation;
            existing_animation_index = i;
            break;
          }
        }
      }

      if (existing_animation) {
        cancel_running_animation_flags[existing_animation_index] = false;

        Animation* animation = existing_animation->animation.Get();

        if (keyframes_rule != existing_animation->style_rule ||
            keyframes_rule->Version() !=
                existing_animation->style_rule_version ||
            existing_animation->specified_timing != specified_timing) {
          DCHECK(!is_animation_style_change);
          update.UpdateAnimation(
              existing_animation_index, animation,
              *InertEffect::Create(
                  CreateKeyframeEffectModel(resolver, animating_element,
                                            element, &style, parent_style, name,
                                            keyframe_timing_function.Get(), i),
                  timing, is_paused, animation->UnlimitedCurrentTimeInternal()),
              specified_timing, keyframes_rule);
        }

        if (is_paused != animation->Paused()) {
          DCHECK(!is_animation_style_change);
          update.ToggleAnimationIndexPaused(existing_animation_index);
        }
      } else {
        DCHECK(!is_animation_style_change);
        update.StartAnimation(
            name, name_index,
            *InertEffect::Create(
                CreateKeyframeEffectModel(resolver, animating_element, element,
                                          &style, parent_style, name,
                                          keyframe_timing_function.Get(), i),
                timing, is_paused, 0),
            specified_timing, keyframes_rule);
      }
    }
  }

  for (size_t i = 0; i < cancel_running_animation_flags.size(); i++) {
    if (cancel_running_animation_flags[i]) {
      DCHECK(css_animations && !is_animation_style_change);
      update.CancelAnimation(
          i, *css_animations->running_animations_[i]->animation);
    }
  }

  CalculateAnimationActiveInterpolations(update, animating_element);
}

void CSSAnimations::SnapshotCompositorKeyframes(
    Element& element,
    CSSAnimationUpdate& update,
    const ComputedStyle& style,
    const ComputedStyle* parent_style) {
  const auto& snapshot = [&element, &style,
                          parent_style](const AnimationEffectReadOnly* effect) {
    const KeyframeEffectModelBase* keyframe_effect =
        GetKeyframeEffectModelBase(effect);
    if (keyframe_effect && keyframe_effect->NeedsCompositorKeyframesSnapshot())
      keyframe_effect->SnapshotAllCompositorKeyframes(element, style,
                                                      parent_style);
  };

  ElementAnimations* element_animations = element.GetElementAnimations();
  if (element_animations) {
    for (auto& entry : element_animations->Animations())
      snapshot(entry.key->effect());
  }

  for (const auto& new_animation : update.NewAnimations())
    snapshot(new_animation.effect.Get());

  for (const auto& updated_animation : update.AnimationsWithUpdates())
    snapshot(updated_animation.effect.Get());

  for (const auto& new_transition : update.NewTransitions())
    snapshot(new_transition.value.effect.Get());
}

void CSSAnimations::MaybeApplyPendingUpdate(Element* element) {
  previous_active_interpolations_for_custom_animations_.clear();
  previous_active_interpolations_for_standard_animations_.clear();
  if (pending_update_.IsEmpty())
    return;

  previous_active_interpolations_for_custom_animations_.swap(
      pending_update_.ActiveInterpolationsForCustomAnimations());
  previous_active_interpolations_for_standard_animations_.swap(
      pending_update_.ActiveInterpolationsForStandardAnimations());

  // FIXME: cancelling, pausing, unpausing animations all query
  // compositingState, which is not necessarily up to date here
  // since we call this from recalc style.
  // https://code.google.com/p/chromium/issues/detail?id=339847
  DisableCompositingQueryAsserts disabler;

  for (size_t paused_index :
       pending_update_.AnimationIndicesWithPauseToggled()) {
    Animation& animation = *running_animations_[paused_index]->animation;
    if (animation.Paused())
      animation.Unpause();
    else
      animation.pause();
    if (animation.Outdated())
      animation.Update(kTimingUpdateOnDemand);
  }

  for (const auto& animation : pending_update_.UpdatedCompositorKeyframes())
    animation->SetCompositorPending(true);

  for (const auto& entry : pending_update_.AnimationsWithUpdates()) {
    KeyframeEffectReadOnly* effect =
        ToKeyframeEffectReadOnly(entry.animation->effect());

    effect->SetModel(entry.effect->Model());
    effect->UpdateSpecifiedTiming(entry.effect->SpecifiedTiming());

    running_animations_[entry.index]->Update(entry);
  }

  const Vector<size_t>& cancelled_indices =
      pending_update_.CancelledAnimationIndices();
  for (size_t i = cancelled_indices.size(); i-- > 0;) {
    DCHECK(i == cancelled_indices.size() - 1 ||
           cancelled_indices[i] < cancelled_indices[i + 1]);
    Animation& animation =
        *running_animations_[cancelled_indices[i]]->animation;
    animation.cancel();
    animation.Update(kTimingUpdateOnDemand);
    running_animations_.erase(cancelled_indices[i]);
  }

  for (const auto& entry : pending_update_.NewAnimations()) {
    const InertEffect* inert_animation = entry.effect.Get();
    AnimationEventDelegate* event_delegate =
        new AnimationEventDelegate(element, entry.name);
    KeyframeEffect* effect = KeyframeEffect::Create(
        element, inert_animation->Model(), inert_animation->SpecifiedTiming(),
        KeyframeEffectReadOnly::kDefaultPriority, event_delegate);
    Animation* animation = element->GetDocument().Timeline().Play(effect);
    animation->setId(entry.name);
    if (inert_animation->Paused())
      animation->pause();
    animation->Update(kTimingUpdateOnDemand);

    running_animations_.push_back(new RunningAnimation(animation, entry));
  }

  // Transitions that are run on the compositor only update main-thread state
  // lazily. However, we need the new state to know what the from state shoud
  // be when transitions are retargeted. Instead of triggering complete style
  // recalculation, we find these cases by searching for new transitions that
  // have matching cancelled animation property IDs on the compositor.
  HeapHashMap<PropertyHandle, std::pair<Member<KeyframeEffectReadOnly>, double>>
      retargeted_compositor_transitions;
  for (const PropertyHandle& property :
       pending_update_.CancelledTransitions()) {
    DCHECK(transitions_.Contains(property));

    Animation* animation = transitions_.Take(property).animation;
    KeyframeEffectReadOnly* effect =
        ToKeyframeEffectReadOnly(animation->effect());
    if (effect->HasActiveAnimationsOnCompositor(property) &&
        pending_update_.NewTransitions().find(property) !=
            pending_update_.NewTransitions().end() &&
        !animation->Limited()) {
      retargeted_compositor_transitions.insert(
          property, std::pair<KeyframeEffectReadOnly*, double>(
                        effect, animation->StartTimeInternal()));
    }
    animation->cancel();
    // after cancelation, transitions must be downgraded or they'll fail
    // to be considered when retriggering themselves. This can happen if
    // the transition is captured through getAnimations then played.
    if (animation->effect() && animation->effect()->IsKeyframeEffectReadOnly())
      ToKeyframeEffectReadOnly(animation->effect())->DowngradeToNormal();
    animation->Update(kTimingUpdateOnDemand);
  }

  for (const PropertyHandle& property : pending_update_.FinishedTransitions()) {
    // This transition can also be cancelled and finished at the same time
    if (transitions_.Contains(property)) {
      Animation* animation = transitions_.Take(property).animation;
      // Transition must be downgraded
      if (animation->effect() &&
          animation->effect()->IsKeyframeEffectReadOnly())
        ToKeyframeEffectReadOnly(animation->effect())->DowngradeToNormal();
    }
  }

  for (const auto& entry : pending_update_.NewTransitions()) {
    const CSSAnimationUpdate::NewTransition& new_transition = entry.value;

    RunningTransition running_transition;
    running_transition.from = new_transition.from;
    running_transition.to = new_transition.to;
    running_transition.reversing_adjusted_start_value =
        new_transition.reversing_adjusted_start_value;
    running_transition.reversing_shortening_factor =
        new_transition.reversing_shortening_factor;

    const PropertyHandle& property = new_transition.property;
    const InertEffect* inert_animation = new_transition.effect.Get();
    TransitionEventDelegate* event_delegate =
        new TransitionEventDelegate(element, property);

    EffectModel* model = inert_animation->Model();

    if (retargeted_compositor_transitions.Contains(property)) {
      const std::pair<Member<KeyframeEffectReadOnly>, double>& old_transition =
          retargeted_compositor_transitions.at(property);
      KeyframeEffectReadOnly* old_animation = old_transition.first;
      double old_start_time = old_transition.second;
      double inherited_time =
          IsNull(old_start_time)
              ? 0
              : element->GetDocument().Timeline().CurrentTimeInternal() -
                    old_start_time;

      TransitionKeyframeEffectModel* old_effect =
          ToTransitionKeyframeEffectModel(inert_animation->Model());
      const KeyframeVector& frames = old_effect->GetFrames();

      TransitionKeyframeVector new_frames;
      new_frames.push_back(ToTransitionKeyframe(frames[0]->Clone().Get()));
      new_frames.push_back(ToTransitionKeyframe(frames[1]->Clone().Get()));
      new_frames.push_back(ToTransitionKeyframe(frames[2]->Clone().Get()));

      InertEffect* inert_animation_for_sampling = InertEffect::Create(
          old_animation->Model(), old_animation->SpecifiedTiming(), false,
          inherited_time);
      Vector<RefPtr<Interpolation>> sample;
      inert_animation_for_sampling->Sample(sample);
      if (sample.size() == 1) {
        const TransitionInterpolation& interpolation =
            ToTransitionInterpolation(*sample.at(0));
        new_frames[0]->SetValue(interpolation.GetInterpolatedValue());
        new_frames[0]->SetCompositorValue(
            interpolation.GetInterpolatedCompositorValue());
        new_frames[1]->SetValue(interpolation.GetInterpolatedValue());
        new_frames[1]->SetCompositorValue(
            interpolation.GetInterpolatedCompositorValue());
        model = TransitionKeyframeEffectModel::Create(new_frames);
      }
    }

    KeyframeEffect* transition = KeyframeEffect::Create(
        element, model, inert_animation->SpecifiedTiming(),
        KeyframeEffectReadOnly::kTransitionPriority, event_delegate);
    Animation* animation = element->GetDocument().Timeline().Play(transition);
    if (property.IsCSSCustomProperty()) {
      animation->setId(property.CustomPropertyName());
    } else {
      animation->setId(getPropertyName(property.CssProperty()));
    }
    // Set the current time as the start time for retargeted transitions
    if (retargeted_compositor_transitions.Contains(property)) {
      animation->setStartTime(element->GetDocument().Timeline().currentTime(),
                              false);
    }
    animation->Update(kTimingUpdateOnDemand);
    running_transition.animation = animation;
    transitions_.Set(property, running_transition);
    DCHECK(isValidCSSPropertyID(property.CssProperty()));
    UseCounter::CountAnimatedCSS(element->GetDocument(),
                                 property.CssProperty());

    // TODO(crbug.com/458925): Remove legacy histogram and counts
    DEFINE_STATIC_LOCAL(SparseHistogram, property_histogram,
                        ("WebCore.Animation.CSSProperties"));
    property_histogram.Sample(
        UseCounter::MapCSSPropertyIdToCSSSampleIdForHistogram(
            property.CssProperty()));
  }
  ClearPendingUpdate();
}

void CSSAnimations::CalculateTransitionUpdateForProperty(
    TransitionUpdateState& state,
    const PropertyHandle& property,
    size_t transition_index) {
  state.listed_properties.insert(property);

  // FIXME: We should transition if an !important property changes even when an
  // animation is running, but this is a bit hard to do with the current
  // applyMatchedProperties system.
  if (property.IsCSSCustomProperty()) {
    if (state.update.ActiveInterpolationsForCustomAnimations().Contains(
            property) ||
        (state.animating_element->GetElementAnimations() &&
         state.animating_element->GetElementAnimations()
             ->CssAnimations()
             .previous_active_interpolations_for_custom_animations_.Contains(
                 property))) {
      return;
    }
  } else if (state.update.ActiveInterpolationsForStandardAnimations().Contains(
                 property) ||
             (state.animating_element->GetElementAnimations() &&
              state.animating_element->GetElementAnimations()
                  ->CssAnimations()
                  .previous_active_interpolations_for_standard_animations_
                  .Contains(property))) {
    return;
  }

  const RunningTransition* interrupted_transition = nullptr;
  if (state.active_transitions) {
    TransitionMap::const_iterator active_transition_iter =
        state.active_transitions->find(property);
    if (active_transition_iter != state.active_transitions->end()) {
      const RunningTransition* running_transition =
          &active_transition_iter->value;
      if (CSSPropertyEquality::PropertiesEqual(property, state.style,
                                               *running_transition->to)) {
        return;
      }
      state.update.CancelTransition(property);
      DCHECK(!state.animating_element->GetElementAnimations() ||
             !state.animating_element->GetElementAnimations()
                  ->IsAnimationStyleChange());

      if (CSSPropertyEquality::PropertiesEqual(
              property, state.style,
              *running_transition->reversing_adjusted_start_value)) {
        interrupted_transition = running_transition;
      }
    }
  }

  const PropertyRegistry* registry =
      state.animating_element->GetDocument().GetPropertyRegistry();
  if (property.IsCSSCustomProperty()) {
    if (!registry || !registry->Registration(property.CustomPropertyName())) {
      return;
    }
  }

  if (CSSPropertyEquality::PropertiesEqual(property, state.old_style,
                                           state.style)) {
    return;
  }

  CSSInterpolationTypesMap map(registry);
  CSSInterpolationEnvironment old_environment(map, state.old_style);
  CSSInterpolationEnvironment new_environment(map, state.style);
  InterpolationValue start = nullptr;
  InterpolationValue end = nullptr;
  const InterpolationType* transition_type = nullptr;
  for (const auto& interpolation_type : map.Get(property)) {
    start = interpolation_type->MaybeConvertUnderlyingValue(old_environment);
    if (!start) {
      continue;
    }
    end = interpolation_type->MaybeConvertUnderlyingValue(new_environment);
    if (!end) {
      continue;
    }
    // Merge will only succeed if the two values are considered interpolable.
    if (interpolation_type->MaybeMergeSingles(start.Clone(), end.Clone())) {
      transition_type = interpolation_type.get();
      break;
    }
  }

  // No smooth interpolation exists between these values so don't start a
  // transition.
  if (!transition_type) {
    return;
  }

  // If we have multiple transitions on the same property, we will use the
  // last one since we iterate over them in order.

  Timing timing = state.transition_data.ConvertToTiming(transition_index);
  if (timing.start_delay + timing.iteration_duration <= 0) {
    // We may have started a transition in a prior CSSTransitionData update,
    // this CSSTransitionData update needs to override them.
    // TODO(alancutter): Just iterate over the CSSTransitionDatas in reverse and
    // skip any properties that have already been visited so we don't need to
    // "undo" work like this.
    state.update.UnstartTransition(property);
    return;
  }

  const ComputedStyle* reversing_adjusted_start_value = &state.old_style;
  double reversing_shortening_factor = 1;
  if (interrupted_transition) {
    const double interrupted_progress =
        interrupted_transition->animation->effect()->Progress();
    if (!std::isnan(interrupted_progress)) {
      reversing_adjusted_start_value = interrupted_transition->to.Get();
      reversing_shortening_factor =
          clampTo((interrupted_progress *
                   interrupted_transition->reversing_shortening_factor) +
                      (1 - interrupted_transition->reversing_shortening_factor),
                  0.0, 1.0);
      timing.iteration_duration *= reversing_shortening_factor;
      if (timing.start_delay < 0) {
        timing.start_delay *= reversing_shortening_factor;
      }
    }
  }

  TransitionKeyframeVector keyframes;
  double start_keyframe_offset = 0;

  if (timing.start_delay > 0) {
    timing.iteration_duration += timing.start_delay;
    start_keyframe_offset = timing.start_delay / timing.iteration_duration;
    timing.start_delay = 0;
  }

  RefPtr<TransitionKeyframe> delay_keyframe =
      TransitionKeyframe::Create(property);
  delay_keyframe->SetValue(TypedInterpolationValue::Create(
      *transition_type, start.interpolable_value->Clone(),
      start.non_interpolable_value));
  delay_keyframe->SetOffset(0);
  keyframes.push_back(delay_keyframe);

  RefPtr<TransitionKeyframe> start_keyframe =
      TransitionKeyframe::Create(property);
  start_keyframe->SetValue(TypedInterpolationValue::Create(
      *transition_type, start.interpolable_value->Clone(),
      start.non_interpolable_value));
  start_keyframe->SetOffset(start_keyframe_offset);
  start_keyframe->SetEasing(std::move(timing.timing_function));
  timing.timing_function = LinearTimingFunction::Shared();
  keyframes.push_back(start_keyframe);

  RefPtr<TransitionKeyframe> end_keyframe =
      TransitionKeyframe::Create(property);
  end_keyframe->SetValue(TypedInterpolationValue::Create(
      *transition_type, end.interpolable_value->Clone(),
      end.non_interpolable_value));
  end_keyframe->SetOffset(1);
  keyframes.push_back(end_keyframe);

  if (CompositorAnimations::IsCompositableProperty(property.CssProperty())) {
    RefPtr<AnimatableValue> from = CSSAnimatableValueFactory::Create(
        property.CssProperty(), state.old_style);
    RefPtr<AnimatableValue> to =
        CSSAnimatableValueFactory::Create(property.CssProperty(), state.style);
    delay_keyframe->SetCompositorValue(from);
    start_keyframe->SetCompositorValue(from);
    end_keyframe->SetCompositorValue(to);
  }

  TransitionKeyframeEffectModel* model =
      TransitionKeyframeEffectModel::Create(keyframes);
  if (!state.cloned_style) {
    state.cloned_style = ComputedStyle::Clone(state.style);
  }
  state.update.StartTransition(property, &state.old_style, state.cloned_style,
                               reversing_adjusted_start_value,
                               reversing_shortening_factor,
                               *InertEffect::Create(model, timing, false, 0));
  DCHECK(!state.animating_element->GetElementAnimations() ||
         !state.animating_element->GetElementAnimations()
              ->IsAnimationStyleChange());
}

void CSSAnimations::CalculateTransitionUpdateForCustomProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index) {
  if (transition_property.property_type !=
      CSSTransitionData::kTransitionUnknownProperty) {
    return;
  }
  if (!CSSVariableParser::IsValidVariableName(
          transition_property.property_string)) {
    return;
  }
  CalculateTransitionUpdateForProperty(
      state, PropertyHandle(transition_property.property_string),
      transition_index);
}

void CSSAnimations::CalculateTransitionUpdateForStandardProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index) {
  if (transition_property.property_type !=
      CSSTransitionData::kTransitionKnownProperty) {
    return;
  }

  CSSPropertyID resolved_id =
      resolveCSSPropertyID(transition_property.unresolved_property);
  bool animate_all = resolved_id == CSSPropertyAll;
  const StylePropertyShorthand& property_list =
      animate_all ? PropertiesForTransitionAll()
                  : shorthandForProperty(resolved_id);
  // If not a shorthand we only execute one iteration of this loop, and
  // refer to the property directly.
  for (unsigned i = 0; !i || i < property_list.length(); ++i) {
    CSSPropertyID longhand_id =
        property_list.length() ? property_list.properties()[i] : resolved_id;
    PropertyHandle property = PropertyHandle(longhand_id);
    DCHECK_GE(longhand_id, firstCSSProperty);

    if (!animate_all && !CSSPropertyAPI::Get(longhand_id).IsInterpolable()) {
      continue;
    }

    CalculateTransitionUpdateForProperty(state, property, transition_index);
  }
}

void CSSAnimations::CalculateTransitionUpdate(CSSAnimationUpdate& update,
                                              PropertyPass property_pass,
                                              const Element* animating_element,
                                              const ComputedStyle& style) {
  if (!animating_element)
    return;

  if (animating_element->GetDocument().FinishingOrIsPrinting())
    return;

  ElementAnimations* element_animations =
      animating_element->GetElementAnimations();
  const TransitionMap* active_transitions =
      element_animations ? &element_animations->CssAnimations().transitions_
                         : nullptr;
  const CSSTransitionData* transition_data = style.Transitions();

  const bool animation_style_recalc =
      element_animations && element_animations->IsAnimationStyleChange();

  HashSet<PropertyHandle> listed_properties;
  bool any_transition_had_transition_all = false;
  const LayoutObject* layout_object = animating_element->GetLayoutObject();
  if (!animation_style_recalc && style.Display() != EDisplay::kNone &&
      layout_object && layout_object->Style() && transition_data) {
    TransitionUpdateState state = {
        update,  animating_element,  *layout_object->Style(), style,
        nullptr, active_transitions, listed_properties,       *transition_data};

    for (size_t transition_index = 0;
         transition_index < transition_data->PropertyList().size();
         ++transition_index) {
      const CSSTransitionData::TransitionProperty& transition_property =
          transition_data->PropertyList()[transition_index];
      if (transition_property.unresolved_property == CSSPropertyAll) {
        any_transition_had_transition_all = true;
      }
      if (property_pass == PropertyPass::kCustom) {
        CalculateTransitionUpdateForCustomProperty(state, transition_property,
                                                   transition_index);
      } else {
        DCHECK_EQ(property_pass, PropertyPass::kStandard);
        CalculateTransitionUpdateForStandardProperty(state, transition_property,
                                                     transition_index);
      }
    }
  }

  if (active_transitions) {
    for (const auto& entry : *active_transitions) {
      const PropertyHandle& property = entry.key;
      if (property.IsCSSCustomProperty() !=
          (property_pass == PropertyPass::kCustom)) {
        continue;
      }
      if (!any_transition_had_transition_all && !animation_style_recalc &&
          !listed_properties.Contains(property)) {
        update.CancelTransition(property);
      } else if (entry.value.animation->FinishedInternal()) {
        update.FinishTransition(property);
      }
    }
  }

  CalculateTransitionActiveInterpolations(update, property_pass,
                                          animating_element);
}

void CSSAnimations::Cancel() {
  for (const auto& running_animation : running_animations_) {
    running_animation->animation->cancel();
    running_animation->animation->Update(kTimingUpdateOnDemand);
  }

  for (const auto& entry : transitions_) {
    entry.value.animation->cancel();
    entry.value.animation->Update(kTimingUpdateOnDemand);
  }

  running_animations_.clear();
  transitions_.clear();
  ClearPendingUpdate();
}

namespace {

bool IsCustomPropertyHandle(const PropertyHandle& property) {
  return property.IsCSSCustomProperty();
}

// TODO(alancutter): CSS properties and presentation attributes may have
// identical effects. By grouping them in the same set we introduce a bug where
// arbitrary hash iteration will determine the order the apply in and thus which
// one "wins". We should be more deliberate about the order of application in
// the case of effect collisions.
// Example: Both 'color' and 'svg-color' set the color on ComputedStyle but are
// considered distinct properties in the ActiveInterpolationsMap.
bool IsStandardPropertyHandle(const PropertyHandle& property) {
  return (property.IsCSSProperty() && !property.IsCSSCustomProperty()) ||
         property.IsPresentationAttribute();
}

void AdoptActiveAnimationInterpolations(
    EffectStack* effect_stack,
    CSSAnimationUpdate& update,
    const HeapVector<Member<const InertEffect>>* new_animations,
    const HeapHashSet<Member<const Animation>>* suppressed_animations) {
  ActiveInterpolationsMap custom_interpolations(
      EffectStack::ActiveInterpolations(
          effect_stack, new_animations, suppressed_animations,
          KeyframeEffectReadOnly::kDefaultPriority, IsCustomPropertyHandle));
  update.AdoptActiveInterpolationsForCustomAnimations(custom_interpolations);

  ActiveInterpolationsMap standard_interpolations(
      EffectStack::ActiveInterpolations(
          effect_stack, new_animations, suppressed_animations,
          KeyframeEffectReadOnly::kDefaultPriority, IsStandardPropertyHandle));
  update.AdoptActiveInterpolationsForStandardAnimations(
      standard_interpolations);
}

}  // namespace

void CSSAnimations::CalculateAnimationActiveInterpolations(
    CSSAnimationUpdate& update,
    const Element* animating_element) {
  ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;
  EffectStack* effect_stack =
      element_animations ? &element_animations->GetEffectStack() : nullptr;

  if (update.NewAnimations().IsEmpty() &&
      update.SuppressedAnimations().IsEmpty()) {
    AdoptActiveAnimationInterpolations(effect_stack, update, nullptr, nullptr);
    return;
  }

  HeapVector<Member<const InertEffect>> new_effects;
  for (const auto& new_animation : update.NewAnimations())
    new_effects.push_back(new_animation.effect);

  // Animations with updates use a temporary InertEffect for the current frame.
  for (const auto& updated_animation : update.AnimationsWithUpdates())
    new_effects.push_back(updated_animation.effect);

  AdoptActiveAnimationInterpolations(effect_stack, update, &new_effects,
                                     &update.SuppressedAnimations());
}

namespace {

EffectStack::PropertyHandleFilter PropertyFilter(
    CSSAnimations::PropertyPass property_pass) {
  if (property_pass == CSSAnimations::PropertyPass::kCustom) {
    return IsCustomPropertyHandle;
  }
  DCHECK_EQ(property_pass, CSSAnimations::PropertyPass::kStandard);
  return IsStandardPropertyHandle;
}

}  // namespace

void CSSAnimations::CalculateTransitionActiveInterpolations(
    CSSAnimationUpdate& update,
    PropertyPass property_pass,
    const Element* animating_element) {
  ElementAnimations* element_animations =
      animating_element ? animating_element->GetElementAnimations() : nullptr;
  EffectStack* effect_stack =
      element_animations ? &element_animations->GetEffectStack() : nullptr;

  ActiveInterpolationsMap active_interpolations_for_transitions;
  if (update.NewTransitions().IsEmpty() &&
      update.CancelledTransitions().IsEmpty()) {
    active_interpolations_for_transitions = EffectStack::ActiveInterpolations(
        effect_stack, nullptr, nullptr,
        KeyframeEffectReadOnly::kTransitionPriority,
        PropertyFilter(property_pass));
  } else {
    HeapVector<Member<const InertEffect>> new_transitions;
    for (const auto& entry : update.NewTransitions())
      new_transitions.push_back(entry.value.effect.Get());

    HeapHashSet<Member<const Animation>> cancelled_animations;
    if (!update.CancelledTransitions().IsEmpty()) {
      DCHECK(element_animations);
      const TransitionMap& transition_map =
          element_animations->CssAnimations().transitions_;
      for (const PropertyHandle& property : update.CancelledTransitions()) {
        DCHECK(transition_map.Contains(property));
        cancelled_animations.insert(
            transition_map.at(property).animation.Get());
      }
    }

    active_interpolations_for_transitions = EffectStack::ActiveInterpolations(
        effect_stack, &new_transitions, &cancelled_animations,
        KeyframeEffectReadOnly::kTransitionPriority,
        PropertyFilter(property_pass));
  }

  const ActiveInterpolationsMap& animations =
      property_pass == PropertyPass::kCustom
          ? update.ActiveInterpolationsForCustomAnimations()
          : update.ActiveInterpolationsForStandardAnimations();
  // Properties being animated by animations don't get values from transitions
  // applied.
  if (!animations.IsEmpty() &&
      !active_interpolations_for_transitions.IsEmpty()) {
    for (const auto& entry : animations)
      active_interpolations_for_transitions.erase(entry.key);
  }

  if (property_pass == PropertyPass::kCustom) {
    update.AdoptActiveInterpolationsForCustomTransitions(
        active_interpolations_for_transitions);
  } else {
    DCHECK_EQ(property_pass, PropertyPass::kStandard);
    update.AdoptActiveInterpolationsForStandardTransitions(
        active_interpolations_for_transitions);
  }
}

EventTarget* CSSAnimations::AnimationEventDelegate::GetEventTarget() const {
  return EventPath::EventTargetRespectingTargetRules(*animation_target_);
}

void CSSAnimations::AnimationEventDelegate::MaybeDispatch(
    Document::ListenerType listener_type,
    const AtomicString& event_name,
    double elapsed_time) {
  if (animation_target_->GetDocument().HasListenerType(listener_type)) {
    AnimationEvent* event =
        AnimationEvent::Create(event_name, name_, elapsed_time);
    event->SetTarget(GetEventTarget());
    GetDocument().EnqueueAnimationFrameEvent(event);
  }
}

bool CSSAnimations::AnimationEventDelegate::RequiresIterationEvents(
    const AnimationEffectReadOnly& animation_node) {
  return GetDocument().HasListenerType(Document::kAnimationIterationListener);
}

void CSSAnimations::AnimationEventDelegate::OnEventCondition(
    const AnimationEffectReadOnly& animation_node) {
  const AnimationEffectReadOnly::Phase current_phase =
      animation_node.GetPhase();
  const double current_iteration = animation_node.CurrentIteration();

  if (previous_phase_ != current_phase &&
      (current_phase == AnimationEffectReadOnly::kPhaseActive ||
       current_phase == AnimationEffectReadOnly::kPhaseAfter) &&
      (previous_phase_ == AnimationEffectReadOnly::kPhaseNone ||
       previous_phase_ == AnimationEffectReadOnly::kPhaseBefore)) {
    const double start_delay = animation_node.SpecifiedTiming().start_delay;
    const double elapsed_time = start_delay < 0 ? -start_delay : 0;
    MaybeDispatch(Document::kAnimationStartListener,
                  EventTypeNames::animationstart, elapsed_time);
  }

  if (current_phase == AnimationEffectReadOnly::kPhaseActive &&
      previous_phase_ == current_phase &&
      previous_iteration_ != current_iteration) {
    // We fire only a single event for all iterations thast terminate
    // between a single pair of samples. See http://crbug.com/275263. For
    // compatibility with the existing implementation, this event uses
    // the elapsedTime for the first iteration in question.
    DCHECK(!std::isnan(animation_node.SpecifiedTiming().iteration_duration));
    const double elapsed_time =
        animation_node.SpecifiedTiming().iteration_duration *
        (previous_iteration_ + 1);
    MaybeDispatch(Document::kAnimationIterationListener,
                  EventTypeNames::animationiteration, elapsed_time);
  }

  if (current_phase == AnimationEffectReadOnly::kPhaseAfter &&
      previous_phase_ != AnimationEffectReadOnly::kPhaseAfter)
    MaybeDispatch(Document::kAnimationEndListener, EventTypeNames::animationend,
                  animation_node.ActiveDurationInternal());

  previous_phase_ = current_phase;
  previous_iteration_ = current_iteration;
}

DEFINE_TRACE(CSSAnimations::AnimationEventDelegate) {
  visitor->Trace(animation_target_);
  AnimationEffectReadOnly::EventDelegate::Trace(visitor);
}

EventTarget* CSSAnimations::TransitionEventDelegate::GetEventTarget() const {
  return EventPath::EventTargetRespectingTargetRules(*transition_target_);
}

void CSSAnimations::TransitionEventDelegate::OnEventCondition(
    const AnimationEffectReadOnly& animation_node) {
  const AnimationEffectReadOnly::Phase current_phase =
      animation_node.GetPhase();
  if (current_phase == AnimationEffectReadOnly::kPhaseAfter &&
      current_phase != previous_phase_ &&
      GetDocument().HasListenerType(Document::kTransitionEndListener)) {
    String property_name = property_.IsCSSCustomProperty()
                               ? property_.CustomPropertyName()
                               : getPropertyNameString(property_.CssProperty());
    const Timing& timing = animation_node.SpecifiedTiming();
    double elapsed_time = timing.iteration_duration;
    const AtomicString& event_type = EventTypeNames::transitionend;
    String pseudo_element =
        PseudoElement::PseudoElementNameForEvents(GetPseudoId());
    TransitionEvent* event = TransitionEvent::Create(
        event_type, property_name, elapsed_time, pseudo_element);
    event->SetTarget(GetEventTarget());
    GetDocument().EnqueueAnimationFrameEvent(event);
  }

  previous_phase_ = current_phase;
}

DEFINE_TRACE(CSSAnimations::TransitionEventDelegate) {
  visitor->Trace(transition_target_);
  AnimationEffectReadOnly::EventDelegate::Trace(visitor);
}

const StylePropertyShorthand& CSSAnimations::PropertiesForTransitionAll() {
  DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
  DEFINE_STATIC_LOCAL(StylePropertyShorthand, property_shorthand, ());
  if (properties.IsEmpty()) {
    for (int i = firstCSSProperty; i <= lastCSSProperty; ++i) {
      CSSPropertyID id = convertToCSSPropertyID(i);
      // Avoid creating overlapping transitions with perspective-origin and
      // transition-origin.
      if (id == CSSPropertyWebkitPerspectiveOriginX ||
          id == CSSPropertyWebkitPerspectiveOriginY ||
          id == CSSPropertyWebkitTransformOriginX ||
          id == CSSPropertyWebkitTransformOriginY ||
          id == CSSPropertyWebkitTransformOriginZ)
        continue;
      if (CSSPropertyAPI::Get(id).IsInterpolable())
        properties.push_back(id);
    }
    property_shorthand = StylePropertyShorthand(
        CSSPropertyInvalid, properties.begin(), properties.size());
  }
  return property_shorthand;
}

// Properties that affect animations are not allowed to be affected by
// animations. http://w3c.github.io/web-animations/#not-animatable-section
bool CSSAnimations::IsAnimationAffectingProperty(CSSPropertyID property) {
  switch (property) {
    case CSSPropertyAnimation:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyDisplay:
    case CSSPropertyTransition:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
      return true;
    default:
      return false;
  }
}

bool CSSAnimations::IsAffectedByKeyframesFromScope(
    const Element& element,
    const TreeScope& tree_scope) {
  // Animated elements are affected by @keyframes rules from the same scope
  // and from their shadow sub-trees if they are shadow hosts.
  if (element.GetTreeScope() == tree_scope)
    return true;
  if (!IsShadowHost(element))
    return false;
  if (tree_scope.RootNode() == tree_scope.GetDocument())
    return false;
  return ToShadowRoot(tree_scope.RootNode()).host() == element;
}

bool CSSAnimations::IsAnimatingCustomProperties(
    const ElementAnimations* element_animations) {
  return element_animations &&
         element_animations->GetEffectStack().AffectsProperties(
             IsCustomPropertyHandle);
}

DEFINE_TRACE(CSSAnimations) {
  visitor->Trace(transitions_);
  visitor->Trace(pending_update_);
  visitor->Trace(running_animations_);
}

}  // namespace blink
