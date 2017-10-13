// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAnimationUpdate_h
#define CSSAnimationUpdate_h

#include "core/animation/EffectStack.h"
#include "core/animation/InertEffect.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSPropertyEquality.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Animation;
class ComputedStyle;

class NewCSSAnimation {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  NewCSSAnimation(AtomicString name,
                  size_t name_index,
                  const InertEffect& effect,
                  Timing timing,
                  StyleRuleKeyframes* style_rule)
      : name(name),
        name_index(name_index),
        effect(effect),
        timing(timing),
        style_rule(style_rule),
        style_rule_version(this->style_rule->Version()) {}

  DEFINE_INLINE_TRACE() {
    visitor->Trace(effect);
    visitor->Trace(style_rule);
  }

  AtomicString name;
  size_t name_index;
  Member<const InertEffect> effect;
  Timing timing;
  Member<StyleRuleKeyframes> style_rule;
  unsigned style_rule_version;
};

class UpdatedCSSAnimation {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  UpdatedCSSAnimation(size_t index,
                      Animation* animation,
                      const InertEffect& effect,
                      Timing specified_timing,
                      StyleRuleKeyframes* style_rule)
      : index(index),
        animation(animation),
        effect(&effect),
        specified_timing(specified_timing),
        style_rule(style_rule),
        style_rule_version(this->style_rule->Version()) {}

  DEFINE_INLINE_TRACE() {
    visitor->Trace(animation);
    visitor->Trace(effect);
    visitor->Trace(style_rule);
  }

  size_t index;
  Member<Animation> animation;
  Member<const InertEffect> effect;
  Timing specified_timing;
  Member<StyleRuleKeyframes> style_rule;
  unsigned style_rule_version;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::NewCSSAnimation);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::UpdatedCSSAnimation);

namespace blink {

// This class stores the CSS Animations/Transitions information we use during a
// style recalc. This includes updates to animations/transitions as well as the
// Interpolations to be applied.
class CSSAnimationUpdate final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CSSAnimationUpdate);

 public:
  CSSAnimationUpdate();
  ~CSSAnimationUpdate();

  void Copy(const CSSAnimationUpdate&);
  void Clear();

  void StartAnimation(const AtomicString& animation_name,
                      size_t name_index,
                      const InertEffect& effect,
                      const Timing& timing,
                      StyleRuleKeyframes* style_rule) {
    new_animations_.push_back(NewCSSAnimation(animation_name, name_index,
                                              effect, timing, style_rule));
  }
  void CancelAnimation(size_t index, const Animation& animation) {
    cancelled_animation_indices_.push_back(index);
    suppressed_animations_.insert(&animation);
  }
  void ToggleAnimationIndexPaused(size_t index) {
    animation_indices_with_pause_toggled_.push_back(index);
  }
  void UpdateAnimation(size_t index,
                       Animation* animation,
                       const InertEffect& effect,
                       const Timing& specified_timing,
                       StyleRuleKeyframes* style_rule) {
    animations_with_updates_.push_back(UpdatedCSSAnimation(
        index, animation, effect, specified_timing, style_rule));
    suppressed_animations_.insert(animation);
  }
  void UpdateCompositorKeyframes(Animation* animation) {
    updated_compositor_keyframes_.push_back(animation);
  }

  void StartTransition(
      const PropertyHandle&,
      RefPtr<const ComputedStyle> from,
      RefPtr<const ComputedStyle> to,
      RefPtr<const ComputedStyle> reversing_adjusted_start_value,
      double reversing_shortening_factor,
      const InertEffect&);
  void UnstartTransition(const PropertyHandle&);
  void CancelTransition(const PropertyHandle& property) {
    cancelled_transitions_.insert(property);
  }
  void FinishTransition(const PropertyHandle& property) {
    finished_transitions_.insert(property);
  }

  const HeapVector<NewCSSAnimation>& NewAnimations() const {
    return new_animations_;
  }
  const Vector<size_t>& CancelledAnimationIndices() const {
    return cancelled_animation_indices_;
  }
  const HeapHashSet<Member<const Animation>>& SuppressedAnimations() const {
    return suppressed_animations_;
  }
  const Vector<size_t>& AnimationIndicesWithPauseToggled() const {
    return animation_indices_with_pause_toggled_;
  }
  const HeapVector<UpdatedCSSAnimation>& AnimationsWithUpdates() const {
    return animations_with_updates_;
  }
  const HeapVector<Member<Animation>>& UpdatedCompositorKeyframes() const {
    return updated_compositor_keyframes_;
  }

  struct NewTransition {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    NewTransition();
    ~NewTransition();
    DEFINE_INLINE_TRACE() { visitor->Trace(effect); }

    PropertyHandle property = HashTraits<blink::PropertyHandle>::EmptyValue();
    RefPtr<const ComputedStyle> from;
    RefPtr<const ComputedStyle> to;
    RefPtr<const ComputedStyle> reversing_adjusted_start_value;
    double reversing_shortening_factor;
    Member<const InertEffect> effect;
  };
  using NewTransitionMap = HeapHashMap<PropertyHandle, NewTransition>;
  const NewTransitionMap& NewTransitions() const { return new_transitions_; }
  const HashSet<PropertyHandle>& CancelledTransitions() const {
    return cancelled_transitions_;
  }
  const HashSet<PropertyHandle>& FinishedTransitions() const {
    return finished_transitions_;
  }

  void AdoptActiveInterpolationsForCustomAnimations(
      ActiveInterpolationsMap& new_map) {
    new_map.swap(active_interpolations_for_custom_animations_);
  }
  void AdoptActiveInterpolationsForStandardAnimations(
      ActiveInterpolationsMap& new_map) {
    new_map.swap(active_interpolations_for_standard_animations_);
  }
  void AdoptActiveInterpolationsForCustomTransitions(
      ActiveInterpolationsMap& new_map) {
    new_map.swap(active_interpolations_for_custom_transitions_);
  }
  void AdoptActiveInterpolationsForStandardTransitions(
      ActiveInterpolationsMap& new_map) {
    new_map.swap(active_interpolations_for_standard_transitions_);
  }
  const ActiveInterpolationsMap& ActiveInterpolationsForCustomAnimations()
      const {
    return active_interpolations_for_custom_animations_;
  }
  ActiveInterpolationsMap& ActiveInterpolationsForCustomAnimations() {
    return active_interpolations_for_custom_animations_;
  }
  const ActiveInterpolationsMap& ActiveInterpolationsForStandardAnimations()
      const {
    return active_interpolations_for_standard_animations_;
  }
  ActiveInterpolationsMap& ActiveInterpolationsForStandardAnimations() {
    return active_interpolations_for_standard_animations_;
  }
  const ActiveInterpolationsMap& ActiveInterpolationsForCustomTransitions()
      const {
    return active_interpolations_for_custom_transitions_;
  }
  const ActiveInterpolationsMap& ActiveInterpolationsForStandardTransitions()
      const {
    return active_interpolations_for_standard_transitions_;
  }

  bool IsEmpty() const {
    return new_animations_.IsEmpty() &&
           cancelled_animation_indices_.IsEmpty() &&
           suppressed_animations_.IsEmpty() &&
           animation_indices_with_pause_toggled_.IsEmpty() &&
           animations_with_updates_.IsEmpty() && new_transitions_.IsEmpty() &&
           cancelled_transitions_.IsEmpty() &&
           finished_transitions_.IsEmpty() &&
           active_interpolations_for_custom_animations_.IsEmpty() &&
           active_interpolations_for_standard_animations_.IsEmpty() &&
           active_interpolations_for_custom_transitions_.IsEmpty() &&
           active_interpolations_for_standard_transitions_.IsEmpty() &&
           updated_compositor_keyframes_.IsEmpty();
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(new_transitions_);
    visitor->Trace(new_animations_);
    visitor->Trace(suppressed_animations_);
    visitor->Trace(animations_with_updates_);
    visitor->Trace(updated_compositor_keyframes_);
  }

 private:
  // Order is significant since it defines the order in which new animations
  // will be started. Note that there may be multiple animations present
  // with the same name, due to the way in which we split up animations with
  // incomplete keyframes.
  HeapVector<NewCSSAnimation> new_animations_;
  Vector<size_t> cancelled_animation_indices_;
  HeapHashSet<Member<const Animation>> suppressed_animations_;
  Vector<size_t> animation_indices_with_pause_toggled_;
  HeapVector<UpdatedCSSAnimation> animations_with_updates_;
  HeapVector<Member<Animation>> updated_compositor_keyframes_;

  NewTransitionMap new_transitions_;
  HashSet<PropertyHandle> cancelled_transitions_;
  HashSet<PropertyHandle> finished_transitions_;

  ActiveInterpolationsMap active_interpolations_for_custom_animations_;
  ActiveInterpolationsMap active_interpolations_for_standard_animations_;
  ActiveInterpolationsMap active_interpolations_for_custom_transitions_;
  ActiveInterpolationsMap active_interpolations_for_standard_transitions_;

  friend class PendingAnimationUpdate;
};

}  // namespace blink

#endif
