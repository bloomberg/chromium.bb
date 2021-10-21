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

#include "third_party/blink/renderer/core/animation/css/css_animations.h"

#include <algorithm>
#include <bitset>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_update_scope.h"
#include "third_party/blink/renderer/core/animation/css/css_keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_transition.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/inert_effect.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/animation_event.h"
#include "third_party/blink/renderer/core/events/transition_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

using PropertySet = HashSet<CSSPropertyName>;

namespace {

// Processes keyframe rules, extracting the timing function and properties being
// animated for each keyframe. The extraction process is doing more work that
// strictly required for the setup to step 5 in the spec
// (https://drafts.csswg.org/css-animations-2/#keyframes) as an optimization
// to avoid needing to process each rule multiple times to extract different
// properties.
StringKeyframeVector ProcessKeyframesRule(
    const StyleRuleKeyframes* keyframes_rule,
    const Document& document,
    const ComputedStyle* parent_style,
    TimingFunction* default_timing_function,
    WritingMode writing_mode,
    TextDirection text_direction) {
  StringKeyframeVector keyframes;
  const HeapVector<Member<StyleRuleKeyframe>>& style_keyframes =
      keyframes_rule->Keyframes();

  for (wtf_size_t i = 0; i < style_keyframes.size(); ++i) {
    const StyleRuleKeyframe* style_keyframe = style_keyframes[i].Get();
    auto* keyframe = MakeGarbageCollected<StringKeyframe>();
    const Vector<double>& offsets = style_keyframe->Keys();
    DCHECK(!offsets.IsEmpty());
    keyframe->SetOffset(offsets[0]);
    keyframe->SetEasing(default_timing_function);
    const CSSPropertyValueSet& properties = style_keyframe->Properties();
    for (unsigned j = 0; j < properties.PropertyCount(); j++) {
      CSSPropertyValueSet::PropertyReference property_reference =
          properties.PropertyAt(j);
      CSSPropertyRef ref(property_reference.Name(), document);
      const CSSProperty& property = ref.GetProperty();
      if (property.PropertyID() == CSSPropertyID::kAnimationTimingFunction) {
        const CSSValue& value = property_reference.Value();
        scoped_refptr<TimingFunction> timing_function;
        if (value.IsInheritedValue() && parent_style->Animations()) {
          timing_function = parent_style->Animations()->TimingFunctionList()[0];
        } else if (auto* value_list = DynamicTo<CSSValueList>(value)) {
          timing_function =
              CSSToStyleMap::MapAnimationTimingFunction(value_list->Item(0));
        } else {
          DCHECK(value.IsCSSWideKeyword());
          timing_function = CSSTimingData::InitialTimingFunction();
        }
        keyframe->SetEasing(std::move(timing_function));
      } else if (!CSSAnimations::IsAnimationAffectingProperty(property)) {
        // Map Logical to physical property name.
        const CSSProperty& physical_property =
            property.ResolveDirectionAwareProperty(text_direction,
                                                   writing_mode);
        const CSSPropertyName& name = physical_property.GetCSSPropertyName();
        keyframe->SetCSSPropertyValue(name, property_reference.Value());
      }
    }
    keyframes.push_back(keyframe);
    // The last keyframe specified at a given offset is used.
    for (wtf_size_t j = 1; j < offsets.size(); ++j) {
      keyframes.push_back(
          To<StringKeyframe>(keyframe->CloneWithOffset(offsets[j])));
    }
  }

  std::stable_sort(keyframes.begin(), keyframes.end(),
                   [](const Member<Keyframe>& a, const Member<Keyframe>& b) {
                     return a->CheckedOffset() < b->CheckedOffset();
                   });
  return keyframes;
}

// Finds the index of a keyframe with matching offset and easing.
absl::optional<int> FindIndexOfMatchingKeyframe(
    const StringKeyframeVector& keyframes,
    wtf_size_t start_index,
    double offset,
    const TimingFunction& easing) {
  for (wtf_size_t i = start_index; i < keyframes.size(); i++) {
    StringKeyframe* keyframe = keyframes[i];

    // Keyframes are sorted by offset. Search can stop once we hit and offset
    // that exceeds the target value.
    if (offset < keyframe->CheckedOffset())
      break;

    if (easing.ToString() == keyframe->Easing().ToString())
      return i;
  }
  return absl::nullopt;
}

// Tests conditions for inserting a bounding keyframe, which are outlined in
// steps 6 and 7 of the spec for keyframe construction.
// https://drafts.csswg.org/css-animations-2/#keyframes
bool NeedsBoundaryKeyframe(StringKeyframe* candidate,
                           double offset,
                           const PropertySet& animated_properties,
                           const PropertySet& bounding_properties,
                           TimingFunction* default_timing_function) {
  if (!candidate)
    return true;

  if (candidate->CheckedOffset() != offset)
    return true;

  if (bounding_properties.size() == animated_properties.size())
    return false;

  return candidate->Easing().ToString() != default_timing_function->ToString();
}

StringKeyframeEffectModel* CreateKeyframeEffectModel(
    StyleResolver* resolver,
    Element& element,
    const ComputedStyle* style,
    const ComputedStyle* parent_style,
    const AtomicString& name,
    TimingFunction* default_timing_function,
    size_t animation_index) {
  // The algorithm for constructing string keyframes for a CSS animation is
  // covered in the following spec:
  // https://drafts.csswg.org/css-animations-2/#keyframes

  // For a given target (pseudo-)element, element, animation name, and
  // position of the animation in element’s animation-name list, keyframe
  // objects are generated as follows:

  // 1. Let default timing function be the timing function at the position
  //    of the resolved value of the animation-timing-function for element,
  //    repeating the list as necessary as described in CSS Animations 1 §4.2
  //    The animation-name property.

  // 2. Find the last @keyframes at-rule in document order with <keyframes-name>
  //    matching name.
  //    If there is no @keyframes at-rule with <keyframes-name> matching name,
  //    abort this procedure. In this case no animation is generated, and any
  //    existing animation matching name is canceled.

  const StyleRuleKeyframes* keyframes_rule =
      resolver->FindKeyframesRule(&element, name);
  DCHECK(keyframes_rule);

  // 3. Let keyframes be an empty sequence of keyframe objects.
  StringKeyframeVector keyframes;

  // 4. Let animated properties be an empty set of longhand CSS property names.
  PropertySet animated_properties;

  // Start and end properties are also tracked to simplify the process of
  // determining if the first and last keyframes are missing properties.
  PropertySet start_properties;
  PropertySet end_properties;

  // Properties that have already been processed at the current keyframe.
  PropertySet current_offset_properties;

  // 5. Perform a stable sort of the keyframe blocks in the @keyframes rule by
  //    the offset specified in the keyframe selector, and iterate over the
  //    result in reverse applying the following steps:
  keyframes = ProcessKeyframesRule(keyframes_rule, element.GetDocument(),
                                   parent_style, default_timing_function,
                                   style->GetWritingMode(), style->Direction());

  double last_offset = 1;
  wtf_size_t merged_frame_count = 0;
  for (wtf_size_t i = keyframes.size(); i > 0; --i) {
    // 5.1 Let keyframe offset be the value of the keyframe selector converted
    //     to a value in the range 0 ≤ keyframe offset ≤ 1.
    int source_index = i - 1;
    StringKeyframe* rule_keyframe = keyframes[source_index];
    double keyframe_offset = rule_keyframe->CheckedOffset();

    // 5.2 Let keyframe timing function be the value of the last valid
    //     declaration of animation-timing-function specified on the keyframe
    //     block, or, if there is no such valid declaration, default timing
    //     function.
    const TimingFunction& easing = rule_keyframe->Easing();

    // 5.3 After converting keyframe timing function to its canonical form (e.g.
    //     such that step-end becomes steps(1, end)) let keyframe refer to the
    //     existing keyframe in keyframes with matching keyframe offset and
    //     timing function, if any.
    //     If there is no such existing keyframe, let keyframe be a new empty
    //     keyframe with offset, keyframe offset, and timing function, keyframe
    //     timing function, and prepend it to keyframes.

    // Prevent stomping a rule override by tracking properties applied at
    // the current offset.
    if (last_offset != keyframe_offset) {
      current_offset_properties.clear();
      last_offset = keyframe_offset;
    }

    // Avoid unnecessary creation of extra keyframes by merging into
    // existing keyframes.
    absl::optional<int> existing_keyframe_index = FindIndexOfMatchingKeyframe(
        keyframes, source_index + merged_frame_count + 1, keyframe_offset,
        easing);
    int target_index;
    if (existing_keyframe_index) {
      // Merge keyframe propoerties.
      target_index = existing_keyframe_index.value();
      merged_frame_count++;
    } else {
      target_index = source_index + merged_frame_count;
      if (target_index != source_index) {
        // Move keyframe to fill the gap.
        keyframes[target_index] = keyframes[source_index];
        source_index = target_index;
      }
    }

    // 5.4 Iterate over all declarations in the keyframe block and add them to
    //     keyframe such that:
    //     * All variable references are resolved to their current values.
    //     * Each shorthand property is expanded to its longhand subproperties.
    //     * All logical properties are converted to their equivalent physical
    //       properties.
    //     * For any expanded physical longhand properties that appear more than
    //       once, only the last declaration in source order is added.
    //       Note, since multiple keyframe blocks may specify the same keyframe
    //       offset, and since this algorithm iterates over these blocks in
    //       reverse, this implies that if any properties are encountered that
    //       have already added at this same keyframe offset, they should be
    //       skipped.
    //     * All property values are replaced with their computed values.
    // 5.5 Add each physical longhand property name that was added to keyframe
    //     to animated properties.
    StringKeyframe* keyframe = keyframes[target_index];
    for (const auto& property : rule_keyframe->Properties()) {
      CSSPropertyName property_name = property.GetCSSPropertyName();

      // Since processing keyframes in reverse order, skipping properties that
      // have already been inserted prevents overwriting a later merged
      // keyframe.
      if (current_offset_properties.Contains(property_name))
        continue;

      if (source_index != target_index) {
        keyframe->SetCSSPropertyValue(
            property.GetCSSPropertyName(),
            rule_keyframe->CssPropertyValue(property));
      }

      current_offset_properties.insert(property_name);
      animated_properties.insert(property_name);
      if (keyframe_offset == 0)
        start_properties.insert(property_name);
      else if (keyframe_offset == 1)
        end_properties.insert(property_name);
    }
  }

  // Compact the vector of keyframes if any keyframes have been merged.
  keyframes.EraseAt(0, merged_frame_count);

  // 6.  If there is no keyframe in keyframes with offset 0, or if amongst the
  //     keyframes in keyframes with offset 0 not all of the properties in
  //     animated properties are present,
  //
  // 6.1 Let initial keyframe be the keyframe in keyframes with offset 0 and
  //     timing function default timing function.
  // 6.2 If there is no such keyframe, let initial keyframe be a new empty
  //     keyframe with offset 0, and timing function default timing function,
  //     and add it to keyframes after the last keyframe with offset 0.
  // 6.3 For each property in animated properties that is not present in some
  //     other keyframe with offset 0, add the computed value of that property
  //     for element to the keyframe.
  StringKeyframe* start_keyframe = keyframes.IsEmpty() ? nullptr : keyframes[0];
  if (NeedsBoundaryKeyframe(start_keyframe, 0, animated_properties,
                            start_properties, default_timing_function)) {
    start_keyframe = MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetOffset(0);
    start_keyframe->SetEasing(default_timing_function);
    keyframes.push_front(start_keyframe);
  }

  // 7.  Similarly, if there is no keyframe in keyframes with offset 1, or if
  //     amongst the keyframes in keyframes with offset 1 not all of the
  //     properties in animated properties are present,
  //
  // 7.1 Let final keyframe be the keyframe in keyframes with offset 1 and
  //     timing function default timing function.
  // 7.2 If there is no such keyframe, let final keyframe be a new empty
  //     keyframe with offset 1, and timing function default timing function,
  //     and add it to keyframes after the last keyframe with offset 1.
  // 7.3 For each property in animated properties that is not present in some
  //     other keyframe with offset 1, add the computed value of that property
  //     for element to the keyframe.
  StringKeyframe* end_keyframe = keyframes[keyframes.size() - 1];
  if (NeedsBoundaryKeyframe(end_keyframe, 1, animated_properties,
                            end_properties, default_timing_function)) {
    end_keyframe = MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetOffset(1);
    end_keyframe->SetEasing(default_timing_function);
    keyframes.push_back(end_keyframe);
  }

  DCHECK_GE(keyframes.size(), 2U);
  DCHECK_EQ(keyframes.front()->CheckedOffset(), 0);
  DCHECK_EQ(keyframes.back()->CheckedOffset(), 1);

  auto* model = MakeGarbageCollected<CssKeyframeEffectModel>(
      keyframes, EffectModel::kCompositeReplace, &start_keyframe->Easing());
  if (animation_index > 0 && model->HasSyntheticKeyframes()) {
    UseCounter::Count(element.GetDocument(),
                      WebFeature::kCSSAnimationsStackedNeutralKeyframe);
  }
  return model;
}

// Returns the start time of an animation given the start delay. A negative
// start delay results in the animation starting with non-zero progress.
AnimationTimeDelta StartTimeFromDelay(AnimationTimeDelta start_delay) {
  return start_delay < AnimationTimeDelta() ? -start_delay
                                            : AnimationTimeDelta();
}

// Timing functions for computing elapsed time of an event.

AnimationTimeDelta IntervalStart(const AnimationEffect& effect) {
  AnimationTimeDelta start_delay = effect.NormalizedTiming().start_delay;
  const AnimationTimeDelta active_duration =
      effect.NormalizedTiming().active_duration;
  // This fixes a problem where start_delay could be -0
  if (!start_delay.is_zero()) {
    start_delay = -start_delay;
  }
  return std::max(std::min(start_delay, active_duration), AnimationTimeDelta());
}

AnimationTimeDelta IntervalEnd(const AnimationEffect& effect) {
  const AnimationTimeDelta start_delay = effect.NormalizedTiming().start_delay;
  const AnimationTimeDelta end_delay = effect.NormalizedTiming().end_delay;
  const AnimationTimeDelta active_duration =
      effect.NormalizedTiming().active_duration;
  const AnimationTimeDelta target_effect_end =
      std::max(start_delay + active_duration + end_delay, AnimationTimeDelta());
  return std::max(std::min(target_effect_end - start_delay, active_duration),
                  AnimationTimeDelta());
}

AnimationTimeDelta IterationElapsedTime(const AnimationEffect& effect,
                                        double previous_iteration) {
  const double current_iteration = effect.CurrentIteration().value();
  const double iteration_boundary = (previous_iteration > current_iteration)
                                        ? current_iteration + 1
                                        : current_iteration;
  const double iteration_start = effect.SpecifiedTiming().iteration_start;
  const AnimationTimeDelta iteration_duration =
      effect.NormalizedTiming().iteration_duration;
  return iteration_duration * (iteration_boundary - iteration_start);
}

AnimationTimeline* ComputeTimeline(Element* element,
                                   const StyleNameOrKeyword& timeline_name) {
  Document& document = element->GetDocument();
  if (timeline_name.IsKeyword()) {
    if (timeline_name.GetKeyword() == CSSValueID::kAuto)
      return &document.Timeline();
    DCHECK_EQ(timeline_name.GetKeyword(), CSSValueID::kNone);
    return nullptr;
  }
  return document.GetStyleEngine().FindScrollTimeline(
      timeline_name.GetName().GetValue());
}

}  // namespace

CSSAnimations::CSSAnimations() = default;

namespace {

const KeyframeEffectModelBase* GetKeyframeEffectModelBase(
    const AnimationEffect* effect) {
  if (!effect)
    return nullptr;
  const EffectModel* model = nullptr;
  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(effect))
    model = keyframe_effect->Model();
  else if (auto* inert_effect = DynamicTo<InertEffect>(effect))
    model = inert_effect->Model();
  if (!model || !model->IsKeyframeEffectModel())
    return nullptr;
  return To<KeyframeEffectModelBase>(model);
}

bool ComputedValuesEqual(const PropertyHandle& property,
                         const ComputedStyle& a,
                         const ComputedStyle& b) {
  // If zoom hasn't changed, compare internal values (stored with zoom applied)
  // for speed. Custom properties are never zoomed so they are checked here too.
  if (a.EffectiveZoom() == b.EffectiveZoom() ||
      property.IsCSSCustomProperty()) {
    return CSSPropertyEquality::PropertiesEqual(property, a, b);
  }

  // If zoom has changed, we must construct and compare the unzoomed
  // computed values.
  if (property.GetCSSProperty().PropertyID() == CSSPropertyID::kTransform) {
    // Transform lists require special handling in this case to deal with
    // layout-dependent interpolation which does not yet have a CSSValue.
    return a.Transform().Zoom(1 / a.EffectiveZoom()) ==
           b.Transform().Zoom(1 / b.EffectiveZoom());
  } else {
    const CSSValue* a_val =
        ComputedStyleUtils::ComputedPropertyValue(property.GetCSSProperty(), a);
    const CSSValue* b_val =
        ComputedStyleUtils::ComputedPropertyValue(property.GetCSSProperty(), b);
    // Computed values can be null if not able to parse.
    if (a_val && b_val)
      return *a_val == *b_val;
    // Fallback to the zoom-unaware comparator if either value could not be
    // parsed.
    return CSSPropertyEquality::PropertiesEqual(property, a, b);
  }
}

}  // namespace

void CSSAnimations::CalculateCompositorAnimationUpdate(
    CSSAnimationUpdate& update,
    const Element& animating_element,
    Element& element,
    const ComputedStyle& style,
    const ComputedStyle* parent_style,
    bool was_viewport_resized) {
  ElementAnimations* element_animations =
      animating_element.GetElementAnimations();

  // If the change in style is only due to the Blink-side animation update, we
  // do not need to update the compositor-side animations. The compositor is
  // already changing the same properties and as such this update would provide
  // no new information.
  if (!element_animations || element_animations->IsAnimationStyleChange())
    return;

  const ComputedStyle* old_style = animating_element.GetComputedStyle();
  if (!old_style || old_style->IsEnsuredInDisplayNone() ||
      !old_style->HasCurrentCompositableAnimation()) {
    return;
  }

  bool transform_zoom_changed =
      old_style->HasCurrentTransformAnimation() &&
      old_style->EffectiveZoom() != style.EffectiveZoom();

  const auto& snapshot = [&](AnimationEffect* effect) {
    const KeyframeEffectModelBase* keyframe_effect =
        GetKeyframeEffectModelBase(effect);
    if (!keyframe_effect)
      return false;

    if ((transform_zoom_changed || was_viewport_resized) &&
        (keyframe_effect->Affects(PropertyHandle(GetCSSPropertyTransform())) ||
         keyframe_effect->Affects(PropertyHandle(GetCSSPropertyTranslate()))))
      keyframe_effect->InvalidateCompositorKeyframesSnapshot();

    if (keyframe_effect->SnapshotAllCompositorKeyframesIfNecessary(
            element, style, parent_style)) {
      return true;
    } else if (keyframe_effect->HasSyntheticKeyframes() &&
               keyframe_effect->SnapshotNeutralCompositorKeyframes(
                   element, *old_style, style, parent_style)) {
      return true;
    }
    return false;
  };

  for (auto& entry : element_animations->Animations()) {
    Animation& animation = *entry.key;
    if (snapshot(animation.effect()))
      update.UpdateCompositorKeyframes(&animation);
  }

  for (auto& entry : element_animations->GetWorkletAnimations()) {
    WorkletAnimationBase& animation = *entry;
    if (snapshot(animation.GetEffect()))
      animation.InvalidateCompositingState();
  }
}

void CSSAnimations::CalculateAnimationUpdate(CSSAnimationUpdate& update,
                                             const Element& animating_element,
                                             Element& element,
                                             const ComputedStyle& style,
                                             const ComputedStyle* parent_style,
                                             StyleResolver* resolver) {
  ElementAnimations* element_animations =
      animating_element.GetElementAnimations();

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

  // Rebuild the keyframe model for a CSS animation if it may have been
  // invalidated by a change to the text direction or writing mode.
  const ComputedStyle* old_style = animating_element.GetComputedStyle();
  bool logical_property_mapping_change =
      !old_style || old_style->Direction() != style.Direction() ||
      old_style->GetWritingMode() != style.GetWritingMode();

  if (logical_property_mapping_change && element_animations) {
    // Update computed keyframes for any running animations that depend on
    // logical properties.
    for (auto& entry : element_animations->Animations()) {
      Animation* animation = entry.key;
      if (auto* keyframe_effect =
              DynamicTo<KeyframeEffect>(animation->effect())) {
        keyframe_effect->SetLogicalPropertyResolutionContext(
            style.Direction(), style.GetWritingMode());
        animation->UpdateIfNecessary();
      }
    }
  }

  const CSSAnimationData* animation_data = style.Animations();
  const CSSAnimations* css_animations =
      element_animations ? &element_animations->CssAnimations() : nullptr;

  Vector<bool> cancel_running_animation_flags(
      css_animations ? css_animations->running_animations_.size() : 0);
  for (bool& flag : cancel_running_animation_flags)
    flag = true;

  if (animation_data && style.Display() != EDisplay::kNone) {
    const Vector<AtomicString>& name_list = animation_data->NameList();
    for (wtf_size_t i = 0; i < name_list.size(); ++i) {
      AtomicString name = name_list[i];
      if (name == CSSAnimationData::InitialName())
        continue;

      // Find n where this is the nth occurrence of this animation name.
      wtf_size_t name_index = 0;
      for (wtf_size_t j = 0; j < i; j++) {
        if (name_list[j] == name)
          name_index++;
      }

      const bool is_paused =
          CSSTimingData::GetRepeated(animation_data->PlayStateList(), i) ==
          EAnimPlayState::kPaused;

      Timing timing = animation_data->ConvertToTiming(i);
      // We need to copy timing to a second object for cases where the original
      // is modified and we still need original values.
      Timing specified_timing = timing;
      scoped_refptr<TimingFunction> keyframe_timing_function =
          timing.timing_function;
      timing.timing_function = Timing().timing_function;

      StyleRuleKeyframes* keyframes_rule =
          resolver->FindKeyframesRule(&element, name);
      if (!keyframes_rule)
        continue;  // Cancel the animation if there's no style rule for it.

      const StyleNameOrKeyword& timeline_name = animation_data->GetTimeline(i);

      const RunningAnimation* existing_animation = nullptr;
      wtf_size_t existing_animation_index = 0;

      if (css_animations) {
        for (wtf_size_t j = 0; j < css_animations->running_animations_.size();
             j++) {
          const RunningAnimation& running_animation =
              *css_animations->running_animations_[j];
          if (running_animation.name == name &&
              running_animation.name_index == name_index) {
            existing_animation = &running_animation;
            existing_animation_index = j;
            break;
          }
        }
      }

      if (existing_animation) {
        cancel_running_animation_flags[existing_animation_index] = false;
        CSSAnimation* animation =
            DynamicTo<CSSAnimation>(existing_animation->animation.Get());
        animation->SetAnimationIndex(i);
        const bool was_paused =
            CSSTimingData::GetRepeated(existing_animation->play_state_list,
                                       i) == EAnimPlayState::kPaused;

        // Explicit calls to web-animation play controls override changes to
        // play state via the animation-play-state style. Ensure that the new
        // play state based on animation-play-state differs from the current
        // play state and that the change is not blocked by a sticky state.
        bool toggle_pause_state = false;
        if (is_paused != was_paused && !animation->getIgnoreCSSPlayState()) {
          if (animation->Paused() && !is_paused)
            toggle_pause_state = true;
          else if (animation->Playing() && is_paused)
            toggle_pause_state = true;
        }

        bool will_be_playing =
            toggle_pause_state ? animation->Paused() : animation->Playing();

        AnimationTimeline* timeline = existing_animation->Timeline();
        if (!is_animation_style_change && !animation->GetIgnoreCSSTimeline())
          timeline = ComputeTimeline(&element, timeline_name);

        if (keyframes_rule != existing_animation->style_rule ||
            keyframes_rule->Version() !=
                existing_animation->style_rule_version ||
            existing_animation->specified_timing != specified_timing ||
            is_paused != was_paused || logical_property_mapping_change ||
            timeline != existing_animation->Timeline()) {
          DCHECK(!is_animation_style_change);

          absl::optional<TimelinePhase> inherited_phase;
          absl::optional<AnimationTimeDelta> inherited_time;
          absl::optional<AnimationTimeDelta> timeline_duration;

          if (timeline) {
            inherited_phase = absl::make_optional(timeline->Phase());
            inherited_time = animation->UnlimitedCurrentTime();
            timeline_duration = timeline->GetDuration();

            if (will_be_playing &&
                ((timeline != existing_animation->Timeline()) ||
                 animation->ResetsCurrentTimeOnResume())) {
              if (timeline->IsScrollTimeline()) {
                inherited_time = timeline->CurrentTime();
              } else {
                AnimationTimeline* previous_timeline =
                    existing_animation->Timeline();
                // Check to see if we are switching from a scroll timeline to a
                // document timeline.
                if (previous_timeline &&
                    previous_timeline->IsScrollTimeline() &&
                    previous_timeline->CurrentTime()) {
                  // For now, CSS Animations do not support duration 'auto'.
                  // Issue: https://github.com/w3c/csswg-drafts/issues/6530
                  DCHECK(specified_timing.iteration_duration);

                  // We need to maintain current progress in the animation when
                  // switching from scroll timeline to document timeline.
                  double progress = previous_timeline->CurrentTime().value() /
                                    previous_timeline->GetDuration().value();

                  AnimationTimeDelta end_time = std::max(
                      specified_timing.start_delay +
                          MultiplyZeroAlwaysGivesZero(
                              specified_timing.iteration_duration.value(),
                              specified_timing.iteration_count) +
                          specified_timing.end_delay,
                      AnimationTimeDelta());

                  inherited_time = progress * end_time;
                }
              }
            }
          }

          update.UpdateAnimation(
              existing_animation_index, animation,
              *MakeGarbageCollected<InertEffect>(
                  CreateKeyframeEffectModel(resolver, element, &style,
                                            parent_style, name,
                                            keyframe_timing_function.get(), i),
                  timing, is_paused, inherited_time, inherited_phase,
                  timeline_duration, animation->playbackRate()),
              specified_timing, keyframes_rule, timeline,
              animation_data->PlayStateList());
          if (toggle_pause_state)
            update.ToggleAnimationIndexPaused(existing_animation_index);
        }
      } else {
        DCHECK(!is_animation_style_change);
        AnimationTimeline* timeline = ComputeTimeline(&element, timeline_name);
        absl::optional<TimelinePhase> inherited_phase;
        absl::optional<AnimationTimeDelta> inherited_time =
            AnimationTimeDelta();

        absl::optional<AnimationTimeDelta> timeline_duration;
        if (timeline) {
          timeline_duration = timeline->GetDuration();
          if (!timeline->IsMonotonicallyIncreasing()) {
            inherited_phase = absl::make_optional(timeline->Phase());
            inherited_time = timeline->CurrentTime();
          }
        }
        update.StartAnimation(name, name_index, i,
                              *MakeGarbageCollected<InertEffect>(
                                  CreateKeyframeEffectModel(
                                      resolver, element, &style, parent_style,
                                      name, keyframe_timing_function.get(), i),
                                  timing, is_paused, inherited_time,
                                  inherited_phase, timeline_duration, 1.0),
                              specified_timing, keyframes_rule, timeline,
                              animation_data->PlayStateList());
      }
    }
  }

  for (wtf_size_t i = 0; i < cancel_running_animation_flags.size(); i++) {
    if (cancel_running_animation_flags[i]) {
      DCHECK(css_animations && !is_animation_style_change);
      update.CancelAnimation(
          i, *css_animations->running_animations_[i]->animation);
    }
  }

  CalculateAnimationActiveInterpolations(update, animating_element);
}

AnimationEffect::EventDelegate* CSSAnimations::CreateEventDelegate(
    Element* element,
    const PropertyHandle& property_handle,
    const AnimationEffect::EventDelegate* old_event_delegate) {
  const CSSAnimations::TransitionEventDelegate* old_transition_delegate =
      DynamicTo<CSSAnimations::TransitionEventDelegate>(old_event_delegate);
  Timing::Phase previous_phase =
      old_transition_delegate ? old_transition_delegate->getPreviousPhase()
                              : Timing::kPhaseNone;
  return MakeGarbageCollected<TransitionEventDelegate>(element, property_handle,
                                                       previous_phase);
}

AnimationEffect::EventDelegate* CSSAnimations::CreateEventDelegate(
    Element* element,
    const AtomicString& animation_name,
    const AnimationEffect::EventDelegate* old_event_delegate) {
  const CSSAnimations::AnimationEventDelegate* old_animation_delegate =
      DynamicTo<CSSAnimations::AnimationEventDelegate>(old_event_delegate);
  Timing::Phase previous_phase =
      old_animation_delegate ? old_animation_delegate->getPreviousPhase()
                             : Timing::kPhaseNone;
  absl::optional<double> previous_iteration =
      old_animation_delegate ? old_animation_delegate->getPreviousIteration()
                             : absl::nullopt;
  return MakeGarbageCollected<AnimationEventDelegate>(
      element, animation_name, previous_phase, previous_iteration);
}

void CSSAnimations::SnapshotCompositorKeyframes(
    Element& element,
    CSSAnimationUpdate& update,
    const ComputedStyle& style,
    const ComputedStyle* parent_style) {
  const auto& snapshot = [&element, &style,
                          parent_style](const AnimationEffect* effect) {
    const KeyframeEffectModelBase* keyframe_effect =
        GetKeyframeEffectModelBase(effect);
    if (keyframe_effect) {
      keyframe_effect->SnapshotAllCompositorKeyframesIfNecessary(element, style,
                                                                 parent_style);
    }
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
    snapshot(new_transition.value->effect.Get());
}

namespace {

bool AffectsBackgroundColor(const AnimationEffect& effect) {
  return effect.Affects(PropertyHandle(GetCSSPropertyBackgroundColor()));
}

void UpdateAnimationFlagsForEffect(const AnimationEffect& effect,
                                   ComputedStyle& style) {
  if (effect.Affects(PropertyHandle(GetCSSPropertyOpacity())))
    style.SetHasCurrentOpacityAnimation(true);
  if (effect.Affects(PropertyHandle(GetCSSPropertyTransform())) ||
      effect.Affects(PropertyHandle(GetCSSPropertyRotate())) ||
      effect.Affects(PropertyHandle(GetCSSPropertyScale())) ||
      effect.Affects(PropertyHandle(GetCSSPropertyTranslate()))) {
    style.SetHasCurrentTransformAnimation(true);
  }
  if (effect.Affects(PropertyHandle(GetCSSPropertyFilter())))
    style.SetHasCurrentFilterAnimation(true);
  if (effect.Affects(PropertyHandle(GetCSSPropertyBackdropFilter())))
    style.SetHasCurrentBackdropFilterAnimation(true);
  if (AffectsBackgroundColor(effect))
    style.SetHasCurrentBackgroundColorAnimation(true);
  if (effect.Affects(PropertyHandle(GetCSSPropertyClipPath())))
    style.SetHasCurrentClipPathAnimation(true);
}

void SetCompositablePaintAnimationChangedIfAffected(
    const AnimationEffect& effect,
    ComputedStyle& style) {
  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
      AffectsBackgroundColor(effect)) {
    style.SetCompositablePaintAnimationChanged(true);
  }
}

// Called for animations that are newly created or updated.
void UpdateAnimationFlagsForInertEffect(const InertEffect& effect,
                                        ComputedStyle& style) {
  if (!effect.IsCurrent())
    return;

  UpdateAnimationFlagsForEffect(effect, style);

  // We defensively assume that any update to an existing animation
  // would result in CompositorPending()==true.
  SetCompositablePaintAnimationChangedIfAffected(effect, style);
}

// Called for existing animations that are not modified in this update.
void UpdateAnimationFlagsForAnimation(const Animation& animation,
                                      ComputedStyle& style) {
  const AnimationEffect& effect = *animation.effect();

  if (!effect.IsCurrent())
    return;

  UpdateAnimationFlagsForEffect(effect, style);

  if (animation.CalculateAnimationPlayState() != Animation::kIdle &&
      animation.CompositorPending()) {
    // If something about the animation changed since the last frame (e.g. the
    // effect was modified), we may need to repaint. We use the
    // CompositorPending flag to detect such changes, and conditionally set
    // the CompositablePaintAnimationChanged on ComputedStyle which ultimately
    // invalidates paint.
    //
    // See ComputedStyle::UpdatePropertySpecificDifferences for how this flag
    // is used.
    SetCompositablePaintAnimationChangedIfAffected(effect, style);
  }
}

}  // namespace

void CSSAnimations::UpdateAnimationFlags(Element& animating_element,
                                         CSSAnimationUpdate& update,
                                         ComputedStyle& style) {
  for (const auto& new_animation : update.NewAnimations())
    UpdateAnimationFlagsForInertEffect(*new_animation.effect, style);

  for (const auto& updated_animation : update.AnimationsWithUpdates())
    UpdateAnimationFlagsForInertEffect(*updated_animation.effect, style);

  for (const auto& entry : update.NewTransitions())
    UpdateAnimationFlagsForInertEffect(*entry.value->effect, style);

  if (auto* element_animations = animating_element.GetElementAnimations()) {
    HeapHashSet<Member<const Animation>> cancelled_transitions =
        CreateCancelledTransitionsSet(element_animations, update);
    const HeapHashSet<Member<const Animation>>& suppressed_animations =
        update.SuppressedAnimations();

    auto is_suppressed = [&cancelled_transitions, &suppressed_animations](
                             const Animation& animation) -> bool {
      return suppressed_animations.Contains(&animation) ||
             cancelled_transitions.Contains(&animation);
    };

    for (auto& entry : element_animations->Animations()) {
      if (!is_suppressed(*entry.key))
        UpdateAnimationFlagsForAnimation(*entry.key, style);
    }

    for (auto& entry : element_animations->GetWorkletAnimations()) {
      // TODO(majidvp): we should check the effect's phase before updating the
      // style once the timing of effect is ready to use.
      // https://crbug.com/814851.
      UpdateAnimationFlagsForEffect(*entry->GetEffect(), style);
    }

    // All Animations in this list will get SetCompositorPending(true)
    // during MaybeApplyPendingUpdate.
    for (const Animation* animation : update.UpdatedCompositorKeyframes()) {
      if (!is_suppressed(*animation)) {
        SetCompositablePaintAnimationChangedIfAffected(*animation->effect(),
                                                       style);
      }
    }

    EffectStack& effect_stack = element_animations->GetEffectStack();

    if (style.HasCurrentOpacityAnimation()) {
      style.SetIsRunningOpacityAnimationOnCompositor(
          effect_stack.HasActiveAnimationsOnCompositor(
              PropertyHandle(GetCSSPropertyOpacity())));
    }
    if (style.HasCurrentTransformAnimation()) {
      style.SetIsRunningTransformAnimationOnCompositor(
          effect_stack.HasActiveAnimationsOnCompositor(
              PropertyHandle(GetCSSPropertyTransform())));
    }
    if (style.HasCurrentFilterAnimation()) {
      style.SetIsRunningFilterAnimationOnCompositor(
          effect_stack.HasActiveAnimationsOnCompositor(
              PropertyHandle(GetCSSPropertyFilter())));
    }
    if (style.HasCurrentBackdropFilterAnimation()) {
      style.SetIsRunningBackdropFilterAnimationOnCompositor(
          effect_stack.HasActiveAnimationsOnCompositor(
              PropertyHandle(GetCSSPropertyBackdropFilter())));
    }
  }
}

void CSSAnimations::MaybeApplyPendingUpdate(Element* element) {
  previous_active_interpolations_for_animations_.clear();
  if (pending_update_.IsEmpty())
    return;

  previous_active_interpolations_for_animations_.swap(
      pending_update_.ActiveInterpolationsForAnimations());

  if (!pending_update_.HasUpdates()) {
    ClearPendingUpdate();
    return;
  }

  for (wtf_size_t paused_index :
       pending_update_.AnimationIndicesWithPauseToggled()) {
    CSSAnimation* animation = DynamicTo<CSSAnimation>(
        running_animations_[paused_index]->animation.Get());

    if (animation->Paused()) {
      animation->Unpause();
      animation->resetIgnoreCSSPlayState();
    } else {
      animation->pause();
      animation->resetIgnoreCSSPlayState();
    }
    if (animation->Outdated())
      animation->Update(kTimingUpdateOnDemand);
  }

  for (const auto& animation : pending_update_.UpdatedCompositorKeyframes())
    animation->SetCompositorPending(true);

  for (const auto& entry : pending_update_.AnimationsWithUpdates()) {
    if (entry.animation->effect()) {
      auto* effect = To<KeyframeEffect>(entry.animation->effect());
      if (!effect->GetIgnoreCSSKeyframes())
        effect->SetModel(entry.effect->Model());
      effect->UpdateSpecifiedTiming(entry.effect->SpecifiedTiming());
    }
    if (entry.animation->timeline() != entry.timeline) {
      entry.animation->setTimeline(entry.timeline);
      To<CSSAnimation>(*entry.animation).ResetIgnoreCSSTimeline();
    }

    running_animations_[entry.index]->Update(entry);
    entry.animation->Update(kTimingUpdateOnDemand);
  }

  const Vector<wtf_size_t>& cancelled_indices =
      pending_update_.CancelledAnimationIndices();
  for (wtf_size_t i = cancelled_indices.size(); i-- > 0;) {
    DCHECK(i == cancelled_indices.size() - 1 ||
           cancelled_indices[i] < cancelled_indices[i + 1]);
    Animation& animation =
        *running_animations_[cancelled_indices[i]]->animation;
    animation.ClearOwningElement();
    if (animation.IsCSSAnimation() &&
        !DynamicTo<CSSAnimation>(animation)->getIgnoreCSSPlayState())
      animation.cancel();
    animation.Update(kTimingUpdateOnDemand);
    running_animations_.EraseAt(cancelled_indices[i]);
  }

  for (const auto& entry : pending_update_.NewAnimations()) {
    const InertEffect* inert_animation = entry.effect.Get();
    AnimationEventDelegate* event_delegate =
        MakeGarbageCollected<AnimationEventDelegate>(element, entry.name);
    auto* effect = MakeGarbageCollected<KeyframeEffect>(
        element, inert_animation->Model(), inert_animation->SpecifiedTiming(),
        KeyframeEffect::kDefaultPriority, event_delegate);
    auto* animation = MakeGarbageCollected<CSSAnimation>(
        element->GetExecutionContext(), entry.timeline, effect,
        entry.position_index, entry.name);
    animation->play();
    if (inert_animation->Paused())
      animation->pause();
    animation->resetIgnoreCSSPlayState();
    animation->Update(kTimingUpdateOnDemand);

    running_animations_.push_back(
        MakeGarbageCollected<RunningAnimation>(animation, entry));
  }

  // Track retargeted transitions that are running on the compositor in order
  // to update their start times.
  HashSet<PropertyHandle> retargeted_compositor_transitions;
  for (const PropertyHandle& property :
       pending_update_.CancelledTransitions()) {
    DCHECK(transitions_.Contains(property));

    Animation* animation = transitions_.Take(property)->animation;
    auto* effect = To<KeyframeEffect>(animation->effect());
    if (effect && effect->HasActiveAnimationsOnCompositor(property) &&
        pending_update_.NewTransitions().find(property) !=
            pending_update_.NewTransitions().end() &&
        !animation->Limited()) {
      retargeted_compositor_transitions.insert(property);
    }
    animation->ClearOwningElement();
    animation->cancel();
    // After cancellation, transitions must be downgraded or they'll fail
    // to be considered when retriggering themselves. This can happen if
    // the transition is captured through getAnimations then played.
    effect = DynamicTo<KeyframeEffect>(animation->effect());
    if (effect)
      effect->DowngradeToNormal();
    animation->Update(kTimingUpdateOnDemand);
  }

  for (const PropertyHandle& property : pending_update_.FinishedTransitions()) {
    // This transition can also be cancelled and finished at the same time
    if (transitions_.Contains(property)) {
      Animation* animation = transitions_.Take(property)->animation;
      // Transition must be downgraded
      if (auto* effect = DynamicTo<KeyframeEffect>(animation->effect()))
        effect->DowngradeToNormal();
    }
  }

  HashSet<PropertyHandle> suppressed_transitions;

  if (!pending_update_.NewTransitions().IsEmpty()) {
    element->GetDocument()
        .GetDocumentAnimations()
        .IncrementTrasitionGeneration();
  }

  for (const auto& entry : pending_update_.NewTransitions()) {
    const CSSAnimationUpdate::NewTransition* new_transition = entry.value;
    const PropertyHandle& property = new_transition->property;

    if (suppressed_transitions.Contains(property))
      continue;

    RunningTransition* running_transition =
        MakeGarbageCollected<RunningTransition>();
    running_transition->from = new_transition->from;
    running_transition->to = new_transition->to;
    running_transition->reversing_adjusted_start_value =
        new_transition->reversing_adjusted_start_value;
    running_transition->reversing_shortening_factor =
        new_transition->reversing_shortening_factor;

    const InertEffect* inert_animation = new_transition->effect.Get();
    TransitionEventDelegate* event_delegate =
        MakeGarbageCollected<TransitionEventDelegate>(element, property);

    KeyframeEffectModelBase* model = inert_animation->Model();

    auto* transition_effect = MakeGarbageCollected<KeyframeEffect>(
        element, model, inert_animation->SpecifiedTiming(),
        KeyframeEffect::kTransitionPriority, event_delegate);
    auto* animation = MakeGarbageCollected<CSSTransition>(
        element->GetExecutionContext(), &(element->GetDocument().Timeline()),
        transition_effect,
        element->GetDocument().GetDocumentAnimations().TransitionGeneration(),
        property);

    animation->play();

    // Set the current time as the start time for retargeted transitions
    if (retargeted_compositor_transitions.Contains(property)) {
      animation->setStartTime(element->GetDocument().Timeline().currentTime(),
                              ASSERT_NO_EXCEPTION);
    }
    animation->Update(kTimingUpdateOnDemand);
    running_transition->animation = animation;
    transitions_.Set(property, running_transition);
  }
  ClearPendingUpdate();
}

HeapHashSet<Member<const Animation>>
CSSAnimations::CreateCancelledTransitionsSet(
    ElementAnimations* element_animations,
    CSSAnimationUpdate& update) {
  HeapHashSet<Member<const Animation>> cancelled_transitions;
  if (!update.CancelledTransitions().IsEmpty()) {
    DCHECK(element_animations);
    const TransitionMap& transition_map =
        element_animations->CssAnimations().transitions_;
    for (const PropertyHandle& property : update.CancelledTransitions()) {
      DCHECK(transition_map.Contains(property));
      cancelled_transitions.insert(
          transition_map.at(property)->animation.Get());
    }
  }
  return cancelled_transitions;
}

bool CSSAnimations::CanCalculateTransitionUpdateForProperty(
    TransitionUpdateState& state,
    const PropertyHandle& property) {
  // TODO(crbug.com/1226772): We should transition if an !important property
  // changes even when an animation is running.
  if (state.update.ActiveInterpolationsForAnimations().Contains(property) ||
      (state.animating_element.GetElementAnimations() &&
       state.animating_element.GetElementAnimations()
           ->CssAnimations()
           .previous_active_interpolations_for_animations_.Contains(
               property))) {
    return false;
  }
  return true;
}

void CSSAnimations::CalculateTransitionUpdateForPropertyHandle(
    TransitionUpdateState& state,
    const PropertyHandle& property,
    size_t transition_index) {
  state.listed_properties.insert(property);

  if (!CanCalculateTransitionUpdateForProperty(state, property))
    return;

  const RunningTransition* interrupted_transition = nullptr;
  if (state.active_transitions) {
    TransitionMap::const_iterator active_transition_iter =
        state.active_transitions->find(property);
    if (active_transition_iter != state.active_transitions->end()) {
      const RunningTransition* running_transition =
          active_transition_iter->value;
      if (ComputedValuesEqual(property, state.style, *running_transition->to)) {
        if (!state.transition_data) {
          if (!running_transition->animation->FinishedInternal()) {
            UseCounter::Count(
                state.animating_element.GetDocument(),
                WebFeature::kCSSTransitionCancelledByRemovingStyle);
          }
          // TODO(crbug.com/934700): Add a return to this branch to correctly
          // continue transitions under default settings (all 0s) in the absence
          // of a change in base computed style.
        } else {
          return;
        }
      }
      state.update.CancelTransition(property);
      DCHECK(!state.animating_element.GetElementAnimations() ||
             !state.animating_element.GetElementAnimations()
                  ->IsAnimationStyleChange());

      if (ComputedValuesEqual(
              property, state.style,
              *running_transition->reversing_adjusted_start_value)) {
        interrupted_transition = running_transition;
      }
    }
  }

  // In the default configuration (transition: all 0s) we continue and cancel
  // transitions but do not start them.
  if (!state.transition_data)
    return;

  const PropertyRegistry* registry =
      state.animating_element.GetDocument().GetPropertyRegistry();
  if (property.IsCSSCustomProperty()) {
    if (!registry || !registry->Registration(property.CustomPropertyName())) {
      return;
    }
  }

  // Lazy evaluation of the before change style. We only need to update where
  // we are transitioning from if the final destination is changing.
  if (!state.before_change_style) {
    // By calling GetBaseComputedStyleOrThis, we're using the style from the
    // previous frame if no base style is found. Elements that have not been
    // animated will not have a base style. Elements that were previously
    // animated, but where all previously running animations have stopped may
    // also be missing a base style. In both cases, the old style is equivalent
    // to the base computed style.
    state.before_change_style = CalculateBeforeChangeStyle(
        state.animating_element, *state.old_style.GetBaseComputedStyleOrThis());
  }

  if (ComputedValuesEqual(property, *state.before_change_style, state.style)) {
    return;
  }

  CSSInterpolationTypesMap map(registry, state.animating_element.GetDocument());
  CSSInterpolationEnvironment old_environment(map, *state.before_change_style);
  CSSInterpolationEnvironment new_environment(map, state.style);
  const InterpolationType* transition_type = nullptr;
  InterpolationValue start = nullptr;
  InterpolationValue end = nullptr;

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

  Timing timing = state.transition_data->ConvertToTiming(transition_index);
  // CSS Transitions always have a valid duration (i.e. the value 'auto' is not
  // supported), so iteration_duration will always be set.
  if (timing.start_delay + timing.iteration_duration.value() <=
      AnimationTimeDelta()) {
    // We may have started a transition in a prior CSSTransitionData update,
    // this CSSTransitionData update needs to override them.
    // TODO(alancutter): Just iterate over the CSSTransitionDatas in reverse and
    // skip any properties that have already been visited so we don't need to
    // "undo" work like this.
    state.update.UnstartTransition(property);
    return;
  }

  const ComputedStyle* reversing_adjusted_start_value =
      state.before_change_style.get();
  double reversing_shortening_factor = 1;
  if (interrupted_transition) {
    AnimationEffect* effect = interrupted_transition->animation->effect();
    const absl::optional<double> interrupted_progress =
        effect ? effect->Progress() : absl::nullopt;
    if (interrupted_progress) {
      reversing_adjusted_start_value = interrupted_transition->to.get();
      reversing_shortening_factor =
          ClampTo((interrupted_progress.value() *
                   interrupted_transition->reversing_shortening_factor) +
                      (1 - interrupted_transition->reversing_shortening_factor),
                  0.0, 1.0);
      timing.iteration_duration.value() *= reversing_shortening_factor;
      if (timing.start_delay < AnimationTimeDelta()) {
        timing.start_delay *= reversing_shortening_factor;
      }
    }
  }

  TransitionKeyframeVector keyframes;

  TransitionKeyframe* start_keyframe =
      MakeGarbageCollected<TransitionKeyframe>(property);
  start_keyframe->SetValue(std::make_unique<TypedInterpolationValue>(
      *transition_type, start.interpolable_value->Clone(),
      start.non_interpolable_value));
  start_keyframe->SetOffset(0);
  keyframes.push_back(start_keyframe);

  TransitionKeyframe* end_keyframe =
      MakeGarbageCollected<TransitionKeyframe>(property);
  end_keyframe->SetValue(std::make_unique<TypedInterpolationValue>(
      *transition_type, end.interpolable_value->Clone(),
      end.non_interpolable_value));
  end_keyframe->SetOffset(1);
  keyframes.push_back(end_keyframe);

  if (property.GetCSSProperty().IsCompositableProperty()) {
    CompositorKeyframeValue* from = CompositorKeyframeValueFactory::Create(
        property, *state.before_change_style, start_keyframe->Offset().value());
    CompositorKeyframeValue* to = CompositorKeyframeValueFactory::Create(
        property, state.style, end_keyframe->Offset().value());
    start_keyframe->SetCompositorValue(from);
    end_keyframe->SetCompositorValue(to);
  }

  auto* model = MakeGarbageCollected<TransitionKeyframeEffectModel>(keyframes);
  if (!state.cloned_style) {
    state.cloned_style = ComputedStyle::Clone(state.style);
  }
  state.update.StartTransition(
      property, state.before_change_style, state.cloned_style,
      reversing_adjusted_start_value, reversing_shortening_factor,
      *MakeGarbageCollected<InertEffect>(model, timing, false,
                                         AnimationTimeDelta(), absl::nullopt,
                                         absl::nullopt, 1.0));
  DCHECK(!state.animating_element.GetElementAnimations() ||
         !state.animating_element.GetElementAnimations()
              ->IsAnimationStyleChange());
}

void CSSAnimations::CalculateTransitionUpdateForProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index,
    const ComputedStyle& style) {
  switch (transition_property.property_type) {
    case CSSTransitionData::kTransitionUnknownProperty:
      CalculateTransitionUpdateForCustomProperty(state, transition_property,
                                                 transition_index);
      break;
    case CSSTransitionData::kTransitionKnownProperty:
      CalculateTransitionUpdateForStandardProperty(state, transition_property,
                                                   transition_index, style);
      break;
    default:
      break;
  }
}

void CSSAnimations::CalculateTransitionUpdateForCustomProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index) {
  DCHECK_EQ(transition_property.property_type,
            CSSTransitionData::kTransitionUnknownProperty);

  if (!CSSVariableParser::IsValidVariableName(
          transition_property.property_string)) {
    return;
  }
  CalculateTransitionUpdateForPropertyHandle(
      state, PropertyHandle(transition_property.property_string),
      transition_index);
}

void CSSAnimations::CalculateTransitionUpdateForStandardProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transition_property,
    size_t transition_index,
    const ComputedStyle& style) {
  DCHECK_EQ(transition_property.property_type,
            CSSTransitionData::kTransitionKnownProperty);

  CSSPropertyID resolved_id =
      ResolveCSSPropertyID(transition_property.unresolved_property);
  bool animate_all = resolved_id == CSSPropertyID::kAll;
  const StylePropertyShorthand& property_list =
      animate_all ? PropertiesForTransitionAll()
                  : shorthandForProperty(resolved_id);
  // If not a shorthand we only execute one iteration of this loop, and
  // refer to the property directly.
  for (unsigned i = 0; !i || i < property_list.length(); ++i) {
    CSSPropertyID longhand_id =
        property_list.length() ? property_list.properties()[i]->PropertyID()
                               : resolved_id;
    DCHECK_GE(longhand_id, kFirstCSSProperty);
    const CSSProperty& property =
        CSSProperty::Get(longhand_id)
            .ResolveDirectionAwareProperty(style.Direction(),
                                           style.GetWritingMode());
    PropertyHandle property_handle = PropertyHandle(property);

    if (!animate_all && !property.IsInterpolable()) {
      continue;
    }

    CalculateTransitionUpdateForPropertyHandle(state, property_handle,
                                               transition_index);
  }
}

void CSSAnimations::CalculateTransitionUpdate(CSSAnimationUpdate& update,
                                              Element& animating_element,
                                              const ComputedStyle& style) {
  if (animating_element.GetDocument().FinishingOrIsPrinting())
    return;

  ElementAnimations* element_animations =
      animating_element.GetElementAnimations();
  const TransitionMap* active_transitions =
      element_animations ? &element_animations->CssAnimations().transitions_
                         : nullptr;
  const CSSTransitionData* transition_data = style.Transitions();

  const bool animation_style_recalc =
      element_animations && element_animations->IsAnimationStyleChange();

  HashSet<PropertyHandle> listed_properties;
  bool any_transition_had_transition_all = false;
  const ComputedStyle* old_style = animating_element.GetComputedStyle();

  if (RuntimeEnabledFeatures::CSSDelayedAnimationUpdatesEnabled()) {
    if (auto* data = CSSAnimationUpdateScope::CurrentData())
      old_style = data->GetOldStyle(animating_element);
  }

  if (!animation_style_recalc && style.Display() != EDisplay::kNone &&
      old_style && !old_style->IsEnsuredInDisplayNone()) {
    TransitionUpdateState state = {update,
                                   animating_element,
                                   *old_style,
                                   style,
                                   /*before_change_style=*/nullptr,
                                   /*cloned_style=*/nullptr,
                                   active_transitions,
                                   listed_properties,
                                   transition_data};

    if (transition_data) {
      for (wtf_size_t transition_index = 0;
           transition_index < transition_data->PropertyList().size();
           ++transition_index) {
        const CSSTransitionData::TransitionProperty& transition_property =
            transition_data->PropertyList()[transition_index];
        if (transition_property.unresolved_property == CSSPropertyID::kAll) {
          any_transition_had_transition_all = true;
        }
        CalculateTransitionUpdateForProperty(state, transition_property,
                                             transition_index, style);
      }
    } else if (active_transitions && active_transitions->size()) {
      // !transition_data implies transition: all 0s
      any_transition_had_transition_all = true;
        CSSTransitionData::TransitionProperty default_property(
            CSSPropertyID::kAll);
        CalculateTransitionUpdateForProperty(state, default_property, 0, style);
    }
  }

  if (active_transitions) {
    for (const auto& entry : *active_transitions) {
      const PropertyHandle& property = entry.key;
      if (!any_transition_had_transition_all && !animation_style_recalc &&
          !listed_properties.Contains(property)) {
        update.CancelTransition(property);
      } else if (entry.value->animation->FinishedInternal()) {
        update.FinishTransition(property);
      }
    }
  }

  CalculateTransitionActiveInterpolations(update, animating_element);
}

scoped_refptr<const ComputedStyle> CSSAnimations::CalculateBeforeChangeStyle(
    Element& animating_element,
    const ComputedStyle& base_style) {
  ActiveInterpolationsMap interpolations_map;
  ElementAnimations* element_animations =
      animating_element.GetElementAnimations();
  if (element_animations) {
    const TransitionMap& transition_map =
        element_animations->CssAnimations().transitions_;

    // Assemble list of animations in composite ordering.
    // TODO(crbug.com/1082401): Per spec, the before change style should include
    // all declarative animations. Currently, only including transitions.
    HeapVector<Member<Animation>> animations;
    for (const auto& entry : transition_map) {
      RunningTransition* transition = entry.value;
      Animation* animation = transition->animation;
      animations.push_back(animation);
    }
    std::sort(animations.begin(), animations.end(),
              [](Animation* a, Animation* b) {
                return Animation::HasLowerCompositeOrdering(
                    a, b, Animation::CompareAnimationsOrdering::kPointerOrder);
              });

    // Sample animations and add to the interpolatzions map.
    for (Animation* animation : animations) {
      V8CSSNumberish* current_time_numberish = animation->currentTime();
      if (!current_time_numberish)
        continue;

        // CSSNumericValue is not yet supported, verify that it is not used
      DCHECK(!current_time_numberish->IsCSSNumericValue());

      absl::optional<AnimationTimeDelta> current_time =
          AnimationTimeDelta::FromMillisecondsD(
              current_time_numberish->GetAsDouble());

      auto* effect = DynamicTo<KeyframeEffect>(animation->effect());
      if (!effect)
        continue;

      auto* inert_animation_for_sampling = MakeGarbageCollected<InertEffect>(
          effect->Model(), effect->SpecifiedTiming(), false, current_time,
          absl::nullopt, absl::nullopt, animation->playbackRate());

      HeapVector<Member<Interpolation>> sample;
      inert_animation_for_sampling->Sample(sample);

      for (const auto& interpolation : sample) {
        PropertyHandle handle = interpolation->GetProperty();
        auto interpolation_map_entry = interpolations_map.insert(
            handle, MakeGarbageCollected<ActiveInterpolations>());
        auto& active_interpolations =
            *interpolation_map_entry.stored_value->value;
        if (!interpolation->DependsOnUnderlyingValue())
          active_interpolations.clear();
        active_interpolations.push_back(interpolation);
      }
    }
  }

  StyleResolver& resolver = animating_element.GetDocument().GetStyleResolver();
  return resolver.BeforeChangeStyleForTransitionUpdate(
      animating_element, base_style, interpolations_map);
}

void CSSAnimations::Cancel() {
  for (const auto& running_animation : running_animations_) {
    running_animation->animation->cancel();
    running_animation->animation->Update(kTimingUpdateOnDemand);
  }

  for (const auto& entry : transitions_) {
    entry.value->animation->cancel();
    entry.value->animation->Update(kTimingUpdateOnDemand);
  }

  running_animations_.clear();
  transitions_.clear();
  ClearPendingUpdate();
}

namespace {

bool IsCustomPropertyHandle(const PropertyHandle& property) {
  return property.IsCSSCustomProperty();
}

bool IsFontAffectingPropertyHandle(const PropertyHandle& property) {
  if (property.IsCSSCustomProperty() || !property.IsCSSProperty())
    return false;
  return property.GetCSSProperty().AffectsFont();
}

// TODO(alancutter): CSS properties and presentation attributes may have
// identical effects. By grouping them in the same set we introduce a bug where
// arbitrary hash iteration will determine the order the apply in and thus which
// one "wins". We should be more deliberate about the order of application in
// the case of effect collisions.
// Example: Both 'color' and 'svg-color' set the color on ComputedStyle but are
// considered distinct properties in the ActiveInterpolationsMap.
bool IsCSSPropertyHandle(const PropertyHandle& property) {
  return property.IsCSSProperty() || property.IsPresentationAttribute();
}

void AdoptActiveAnimationInterpolations(
    EffectStack* effect_stack,
    CSSAnimationUpdate& update,
    const HeapVector<Member<const InertEffect>>* new_animations,
    const HeapHashSet<Member<const Animation>>* suppressed_animations) {
  ActiveInterpolationsMap interpolations(EffectStack::ActiveInterpolations(
      effect_stack, new_animations, suppressed_animations,
      KeyframeEffect::kDefaultPriority, IsCSSPropertyHandle));
  update.AdoptActiveInterpolationsForAnimations(interpolations);
}

}  // namespace

void CSSAnimations::CalculateAnimationActiveInterpolations(
    CSSAnimationUpdate& update,
    const Element& animating_element) {
  ElementAnimations* element_animations =
      animating_element.GetElementAnimations();
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

void CSSAnimations::CalculateTransitionActiveInterpolations(
    CSSAnimationUpdate& update,
    const Element& animating_element) {
  ElementAnimations* element_animations =
      animating_element.GetElementAnimations();
  EffectStack* effect_stack =
      element_animations ? &element_animations->GetEffectStack() : nullptr;

  ActiveInterpolationsMap active_interpolations_for_transitions;
  if (update.NewTransitions().IsEmpty() &&
      update.CancelledTransitions().IsEmpty()) {
    active_interpolations_for_transitions = EffectStack::ActiveInterpolations(
        effect_stack, nullptr, nullptr, KeyframeEffect::kTransitionPriority,
        IsCSSPropertyHandle);
  } else {
    HeapVector<Member<const InertEffect>> new_transitions;
    for (const auto& entry : update.NewTransitions())
      new_transitions.push_back(entry.value->effect.Get());

    HeapHashSet<Member<const Animation>> cancelled_animations =
        CreateCancelledTransitionsSet(element_animations, update);

    active_interpolations_for_transitions = EffectStack::ActiveInterpolations(
        effect_stack, &new_transitions, &cancelled_animations,
        KeyframeEffect::kTransitionPriority, IsCSSPropertyHandle);
  }

  const ActiveInterpolationsMap& animations =
      update.ActiveInterpolationsForAnimations();
  // Properties being animated by animations don't get values from transitions
  // applied.
  if (!animations.IsEmpty() &&
      !active_interpolations_for_transitions.IsEmpty()) {
    for (const auto& entry : animations)
      active_interpolations_for_transitions.erase(entry.key);
  }

  update.AdoptActiveInterpolationsForTransitions(
      active_interpolations_for_transitions);
}

EventTarget* CSSAnimations::AnimationEventDelegate::GetEventTarget() const {
  return &EventPath::EventTargetRespectingTargetRules(*animation_target_);
}

void CSSAnimations::AnimationEventDelegate::MaybeDispatch(
    Document::ListenerType listener_type,
    const AtomicString& event_name,
    const AnimationTimeDelta& elapsed_time) {
  if (animation_target_->GetDocument().HasListenerType(listener_type)) {
    String pseudo_element_name = PseudoElement::PseudoElementNameForEvents(
        animation_target_->GetPseudoId());
    AnimationEvent* event = AnimationEvent::Create(
        event_name, name_, elapsed_time, pseudo_element_name);
    event->SetTarget(GetEventTarget());
    GetDocument().EnqueueAnimationFrameEvent(event);
  }
}

bool CSSAnimations::AnimationEventDelegate::RequiresIterationEvents(
    const AnimationEffect& animation_node) {
  return GetDocument().HasListenerType(Document::kAnimationIterationListener);
}

void CSSAnimations::AnimationEventDelegate::OnEventCondition(
    const AnimationEffect& animation_node,
    Timing::Phase current_phase) {
  const absl::optional<double> current_iteration =
      animation_node.CurrentIteration();

  // See http://drafts.csswg.org/css-animations-2/#event-dispatch
  // When multiple events are dispatched for a single phase transition,
  // the animationstart event is to be dispatched before the animationend
  // event.

  // The following phase transitions trigger an animationstart event:
  //   idle or before --> active or after
  //   after --> active or before
  const bool phase_change = previous_phase_ != current_phase;
  const bool was_idle_or_before = (previous_phase_ == Timing::kPhaseNone ||
                                   previous_phase_ == Timing::kPhaseBefore);
  const bool is_active_or_after = (current_phase == Timing::kPhaseActive ||
                                   current_phase == Timing::kPhaseAfter);
  const bool is_active_or_before = (current_phase == Timing::kPhaseActive ||
                                    current_phase == Timing::kPhaseBefore);
  const bool was_after = (previous_phase_ == Timing::kPhaseAfter);
  if (phase_change && ((was_idle_or_before && is_active_or_after) ||
                       (was_after && is_active_or_before))) {
    AnimationTimeDelta elapsed_time =
        was_after ? IntervalEnd(animation_node) : IntervalStart(animation_node);
    MaybeDispatch(Document::kAnimationStartListener,
                  event_type_names::kAnimationstart, elapsed_time);
  }

  // The following phase transitions trigger an animationend event:
  //   idle, before or active--> after
  //   active or after--> before
  const bool was_active_or_after = (previous_phase_ == Timing::kPhaseActive ||
                                    previous_phase_ == Timing::kPhaseAfter);
  const bool is_after = (current_phase == Timing::kPhaseAfter);
  const bool is_before = (current_phase == Timing::kPhaseBefore);
  if (phase_change && (is_after || (was_active_or_after && is_before))) {
    AnimationTimeDelta elapsed_time =
        is_after ? IntervalEnd(animation_node) : IntervalStart(animation_node);
    MaybeDispatch(Document::kAnimationEndListener,
                  event_type_names::kAnimationend, elapsed_time);
  }

  // The following phase transitions trigger an animationcalcel event:
  //   not idle and not after --> idle
  if (phase_change && current_phase == Timing::kPhaseNone &&
      previous_phase_ != Timing::kPhaseAfter) {
    // TODO(crbug.com/1059968): Determine if animation direction or playback
    // rate factor into the calculation of the elapsed time.
    AnimationTimeDelta cancel_time = animation_node.GetCancelTime();
    MaybeDispatch(Document::kAnimationCancelListener,
                  event_type_names::kAnimationcancel, cancel_time);
  }

  if (!phase_change && current_phase == Timing::kPhaseActive &&
      previous_iteration_ != current_iteration) {
    // We fire only a single event for all iterations that terminate
    // between a single pair of samples. See http://crbug.com/275263. For
    // compatibility with the existing implementation, this event uses
    // the elapsedTime for the first iteration in question.
    DCHECK(previous_iteration_ && current_iteration);
    const AnimationTimeDelta elapsed_time =
        IterationElapsedTime(animation_node, previous_iteration_.value());
    MaybeDispatch(Document::kAnimationIterationListener,
                  event_type_names::kAnimationiteration, elapsed_time);
  }

  previous_iteration_ = current_iteration;
  previous_phase_ = current_phase;
}

void CSSAnimations::AnimationEventDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(animation_target_);
  AnimationEffect::EventDelegate::Trace(visitor);
}

EventTarget* CSSAnimations::TransitionEventDelegate::GetEventTarget() const {
  return &EventPath::EventTargetRespectingTargetRules(*transition_target_);
}

void CSSAnimations::TransitionEventDelegate::OnEventCondition(
    const AnimationEffect& animation_node,
    Timing::Phase current_phase) {
  if (current_phase == previous_phase_)
    return;

  if (GetDocument().HasListenerType(Document::kTransitionRunListener)) {
    if (previous_phase_ == Timing::kPhaseNone) {
      EnqueueEvent(
          event_type_names::kTransitionrun,
          StartTimeFromDelay(animation_node.NormalizedTiming().start_delay));
    }
  }

  if (GetDocument().HasListenerType(Document::kTransitionStartListener)) {
    if ((current_phase == Timing::kPhaseActive ||
         current_phase == Timing::kPhaseAfter) &&
        (previous_phase_ == Timing::kPhaseNone ||
         previous_phase_ == Timing::kPhaseBefore)) {
      EnqueueEvent(
          event_type_names::kTransitionstart,
          StartTimeFromDelay(animation_node.NormalizedTiming().start_delay));
    } else if ((current_phase == Timing::kPhaseActive ||
                current_phase == Timing::kPhaseBefore) &&
               previous_phase_ == Timing::kPhaseAfter) {
      // If the transition is progressing backwards it is considered to have
      // started at the end position.
      EnqueueEvent(event_type_names::kTransitionstart,
                   animation_node.NormalizedTiming().iteration_duration);
    }
  }

  if (GetDocument().HasListenerType(Document::kTransitionEndListener)) {
    if (current_phase == Timing::kPhaseAfter &&
        (previous_phase_ == Timing::kPhaseActive ||
         previous_phase_ == Timing::kPhaseBefore ||
         previous_phase_ == Timing::kPhaseNone)) {
      EnqueueEvent(event_type_names::kTransitionend,
                   animation_node.NormalizedTiming().iteration_duration);
    } else if (current_phase == Timing::kPhaseBefore &&
               (previous_phase_ == Timing::kPhaseActive ||
                previous_phase_ == Timing::kPhaseAfter)) {
      // If the transition is progressing backwards it is considered to have
      // ended at the start position.
      EnqueueEvent(
          event_type_names::kTransitionend,
          StartTimeFromDelay(animation_node.NormalizedTiming().start_delay));
    }
  }

  if (GetDocument().HasListenerType(Document::kTransitionCancelListener)) {
    if (current_phase == Timing::kPhaseNone &&
        previous_phase_ != Timing::kPhaseAfter) {
      // Per the css-transitions-2 spec, transitioncancel is fired with the
      // "active time of the animation at the moment it was cancelled,
      // calculated using a fill mode of both".
      absl::optional<AnimationTimeDelta> cancel_active_time =
          CalculateActiveTime(animation_node.NormalizedTiming(),
                              Timing::FillMode::BOTH,
                              animation_node.LocalTime(), previous_phase_);
      // Being the FillMode::BOTH the only possibility to get a null
      // cancel_active_time is that previous_phase_ is kPhaseNone. This cannot
      // happen because we know that current_phase == kPhaseNone and
      // current_phase != previous_phase_ (see early return at the beginning).
      DCHECK(cancel_active_time);
      EnqueueEvent(event_type_names::kTransitioncancel,
                   cancel_active_time.value());
    }
  }

  previous_phase_ = current_phase;
}

void CSSAnimations::TransitionEventDelegate::EnqueueEvent(
    const WTF::AtomicString& type,
    const AnimationTimeDelta& elapsed_time) {
  String property_name =
      property_.IsCSSCustomProperty()
          ? property_.CustomPropertyName()
          : property_.GetCSSProperty().GetPropertyNameString();
  String pseudo_element =
      PseudoElement::PseudoElementNameForEvents(GetPseudoId());
  TransitionEvent* event = TransitionEvent::Create(
      type, property_name, elapsed_time, pseudo_element);
  event->SetTarget(GetEventTarget());
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void CSSAnimations::TransitionEventDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(transition_target_);
  AnimationEffect::EventDelegate::Trace(visitor);
}

const StylePropertyShorthand& CSSAnimations::PropertiesForTransitionAll() {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  DEFINE_STATIC_LOCAL(StylePropertyShorthand, property_shorthand, ());
  if (properties.IsEmpty()) {
    for (CSSPropertyID id : CSSPropertyIDList()) {
      // Avoid creating overlapping transitions with perspective-origin and
      // transition-origin.
      if (id == CSSPropertyID::kWebkitPerspectiveOriginX ||
          id == CSSPropertyID::kWebkitPerspectiveOriginY ||
          id == CSSPropertyID::kWebkitTransformOriginX ||
          id == CSSPropertyID::kWebkitTransformOriginY ||
          id == CSSPropertyID::kWebkitTransformOriginZ)
        continue;
      const CSSProperty& property = CSSProperty::Get(id);
      if (property.IsInterpolable())
        properties.push_back(&property);
    }
    property_shorthand = StylePropertyShorthand(
        CSSPropertyID::kInvalid, properties.begin(), properties.size());
  }
  return property_shorthand;
}

// Properties that affect animations are not allowed to be affected by
// animations. https://drafts.csswg.org/web-animations/#not-animatable-section
bool CSSAnimations::IsAnimationAffectingProperty(const CSSProperty& property) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kAnimation:
    case CSSPropertyID::kAnimationDelay:
    case CSSPropertyID::kAnimationDirection:
    case CSSPropertyID::kAnimationDuration:
    case CSSPropertyID::kAnimationFillMode:
    case CSSPropertyID::kAnimationIterationCount:
    case CSSPropertyID::kAnimationName:
    case CSSPropertyID::kAnimationPlayState:
    case CSSPropertyID::kAnimationTimeline:
    case CSSPropertyID::kAnimationTimingFunction:
    case CSSPropertyID::kContentVisibility:
    case CSSPropertyID::kContain:
    case CSSPropertyID::kDirection:
    case CSSPropertyID::kDisplay:
    case CSSPropertyID::kTextCombineUpright:
    case CSSPropertyID::kTextOrientation:
    case CSSPropertyID::kTransition:
    case CSSPropertyID::kTransitionDelay:
    case CSSPropertyID::kTransitionDuration:
    case CSSPropertyID::kTransitionProperty:
    case CSSPropertyID::kTransitionTimingFunction:
    case CSSPropertyID::kUnicodeBidi:
    case CSSPropertyID::kWebkitWritingMode:
    case CSSPropertyID::kWillChange:
    case CSSPropertyID::kWritingMode:
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
  return To<ShadowRoot>(tree_scope.RootNode()).host() == element;
}

bool CSSAnimations::IsAnimatingCustomProperties(
    const ElementAnimations* element_animations) {
  return element_animations &&
         element_animations->GetEffectStack().AffectsProperties(
             IsCustomPropertyHandle);
}

bool CSSAnimations::IsAnimatingStandardProperties(
    const ElementAnimations* element_animations,
    const CSSBitset* bitset,
    KeyframeEffect::Priority priority) {
  if (!element_animations || !bitset)
    return false;
  return element_animations->GetEffectStack().AffectsProperties(*bitset,
                                                                priority);
}

bool CSSAnimations::IsAnimatingFontAffectingProperties(
    const ElementAnimations* element_animations) {
  return element_animations &&
         element_animations->GetEffectStack().AffectsProperties(
             IsFontAffectingPropertyHandle);
}

bool CSSAnimations::IsAnimatingRevert(
    const ElementAnimations* element_animations) {
  return element_animations && element_animations->GetEffectStack().HasRevert();
}

void CSSAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(transitions_);
  visitor->Trace(pending_update_);
  visitor->Trace(running_animations_);
  visitor->Trace(previous_active_interpolations_for_animations_);
}

}  // namespace blink
