// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATOR_H_
#define UI_COMPOSITOR_LAYER_ANIMATOR_H_
#pragma once

#include <deque>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/base/animation/tween.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_animation_element.h"

namespace gfx {
class Rect;
}

namespace ui {
class Animation;
class Layer;
class LayerAnimationSequence;
class LayerAnimationDelegate;
class LayerAnimationObserver;
class ScopedLayerAnimationSettings;
class Transform;

// When a property of layer needs to be changed it is set by way of
// LayerAnimator. This enables LayerAnimator to animate property changes.
// NB: during many tests, set_disable_animations_for_test is used and causes
// all animations to complete immediately.
class COMPOSITOR_EXPORT LayerAnimator : public AnimationContainerElement {
 public:
  enum PreemptionStrategy {
    IMMEDIATELY_SET_NEW_TARGET,
    IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
    ENQUEUE_NEW_ANIMATION,
    REPLACE_QUEUED_ANIMATIONS,
    BLEND_WITH_CURRENT_ANIMATION
  };

  explicit LayerAnimator(base::TimeDelta transition_duration);
  virtual ~LayerAnimator();

  // No implicit animations when properties are set.
  static LayerAnimator* CreateDefaultAnimator();

  // Implicitly animates when properties are set.
  static LayerAnimator* CreateImplicitAnimator();

  // Sets the transform on the delegate. May cause an implicit animation.
  virtual void SetTransform(const Transform& transform);
  Transform GetTargetTransform() const;

  // Sets the bounds on the delegate. May cause an implicit animation.
  virtual void SetBounds(const gfx::Rect& bounds);
  gfx::Rect GetTargetBounds() const;

  // Sets the opacity on the delegate. May cause an implicit animation.
  virtual void SetOpacity(float opacity);
  float GetTargetOpacity() const;

  // Sets the visibility of the delegate. May cause an implicit animation.
  virtual void SetVisibility(bool visibility);
  bool GetTargetVisibility() const;

  // Sets the layer animation delegate the animator is associated with. The
  // animator does not own the delegate.
  void SetDelegate(LayerAnimationDelegate* delegate);

  // Sets the animation preemption strategy. This determines the behaviour if
  // a property is set during an animation. The default is
  // IMMEDIATELY_SET_NEW_TARGET (see ImmediatelySetNewTarget below).
  void set_preemption_strategy(PreemptionStrategy strategy) {
    preemption_strategy_ = strategy;
  }

  PreemptionStrategy preemption_strategy() const {
    return preemption_strategy_;
  }

  // Start an animation sequence. If an animation for the same property is in
  // progress, it needs to be interrupted with the new animation. The animator
  // takes ownership of this animation sequence.
  void StartAnimation(LayerAnimationSequence* animation);

  // Schedule an animation to be run when possible. The animator takes ownership
  // of this animation sequence.
  void ScheduleAnimation(LayerAnimationSequence* animation);

  // Schedules the animations to be run together. Obviously will no work if
  // they animate any common properties. The animator takes ownership of the
  // animation sequences.
  void ScheduleTogether(const std::vector<LayerAnimationSequence*>& animations);

  // Returns true if there is an animation in the queue (animations remain in
  // the queue until they complete, so this includes running animations).
  bool is_animating() const { return !animation_queue_.empty(); }

  // Returns true if there is an animation in the queue that animates the given
  // property (animations remain in the queue until they complete, so this
  // includes running animations).
  bool IsAnimatingProperty(
      LayerAnimationElement::AnimatableProperty property) const;

  // Stops animating the given property. No effect if there is no running
  // animation for the given property. Skips to the final state of the
  // animation.
  void StopAnimatingProperty(
      LayerAnimationElement::AnimatableProperty property);

  // Stops all animation and clears any queued animations.
  void StopAnimating();

  // These functions are used for adding or removing observers from the observer
  // list. The observers are notified when animations end.
  void AddObserver(LayerAnimationObserver* observer);
  void RemoveObserver(LayerAnimationObserver* observer);

  // This determines how implicit animations will be tweened. This has no
  // effect on animations that are explicitly started or scheduled. The default
  // is Tween::LINEAR.
  void set_tween_type(Tween::Type tween_type) { tween_type_ = tween_type; }
  Tween::Type tween_type() const { return tween_type_; }

  // For testing purposes only.
  void set_disable_timer_for_test(bool disable_timer) {
    disable_timer_for_test_ = disable_timer;
  }
  base::TimeTicks last_step_time() const { return last_step_time_; }

  // When set to true, all animations complete immediately.
  static void set_disable_animations_for_test(bool disable_animations) {
    disable_animations_for_test_ = disable_animations;
  }

  static bool disable_animations_for_test() {
    return disable_animations_for_test_;
  }

 protected:
  LayerAnimationDelegate* delegate() { return delegate_; }
  const LayerAnimationDelegate* delegate() const { return delegate_; }

  // Virtual for testing.
  virtual bool ProgressAnimation(LayerAnimationSequence* sequence,
                                 base::TimeDelta delta);

  // Returns true if the sequence is owned by this animator.
  bool HasAnimation(LayerAnimationSequence* sequence) const;

 private:
  friend class ScopedLayerAnimationSettings;

  // We need to keep track of the start time of every running animation.
  struct RunningAnimation {
    RunningAnimation(LayerAnimationSequence* sequence,
                     base::TimeTicks start_time)
        : sequence(sequence),
          start_time(start_time) {
    }
    LayerAnimationSequence* sequence;
    base::TimeTicks start_time;
  };

  typedef std::vector<RunningAnimation> RunningAnimations;
  typedef std::deque<linked_ptr<LayerAnimationSequence> > AnimationQueue;

  // Implementation of AnimationContainerElement
  virtual void SetStartTime(base::TimeTicks start_time) OVERRIDE;
  virtual void Step(base::TimeTicks time_now) OVERRIDE;
  virtual base::TimeDelta GetTimerInterval() const OVERRIDE;

  // Starts or stops stepping depending on whether thare are running animations.
  void UpdateAnimationState();

  // Removes the sequences from both the running animations and the queue.
  // Returns a pointer to the removed animation, if any. NOTE: the caller is
  // responsible for deleting the returned pointer.
  LayerAnimationSequence* RemoveAnimation(
      LayerAnimationSequence* sequence) WARN_UNUSED_RESULT;

  // Progresses to the end of the sequence before removing it.
  void FinishAnimation(LayerAnimationSequence* sequence);

  // Finishes any running animation with zero duration.
  void FinishAnyAnimationWithZeroDuration();

  // Clears the running animations and the queue. No sequences are progressed.
  void ClearAnimations();

  // Returns the running animation animating the given property, if any.
  RunningAnimation* GetRunningAnimation(
      LayerAnimationElement::AnimatableProperty property);

  // Checks if the sequence has already been added to the queue and adds it
  // to the front if note.
  void AddToQueueIfNotPresent(LayerAnimationSequence* sequence);

  // Any running or queued animation that affects a property in common with
  // |sequence| is either finished or aborted depending on |abort|.
  void RemoveAllAnimationsWithACommonProperty(LayerAnimationSequence* sequence,
                                              bool abort);

  // Preempts a running animation by progressing both the running animation and
  // the given sequence to the end.
  void ImmediatelySetNewTarget(LayerAnimationSequence* sequence);

  // Preempts by aborting the running animation, and starts the given animation.
  void ImmediatelyAnimateToNewTarget(LayerAnimationSequence* sequence);

  // Preempts by adding the new animation to the queue.
  void EnqueueNewAnimation(LayerAnimationSequence* sequence);

  // Preempts by wiping out any unstarted animation in the queue and then
  // enqueuing this animation.
  void ReplaceQueuedAnimations(LayerAnimationSequence* sequence);

  // If there's an animation in the queue that doesn't animate the same property
  // as a running animation, or an animation schedule to run before it, start it
  // up. Repeat until there are no such animations.
  void ProcessQueue();

  // Attempts to add the sequence to the list of running animations. Returns
  // false if there is an animation running that already animates one of the
  // properties affected by |sequence|.
  bool StartSequenceImmediately(LayerAnimationSequence* sequence);

  // Sets the value of target as if all the running and queued animations were
  // allowed to finish.
  void GetTargetValue(LayerAnimationElement::TargetValue* target) const;

  // Called whenever an animation is added to the animation queue. Either by
  // starting the animation or adding to the queue.
  void OnScheduled(LayerAnimationSequence* sequence);

  // This is the queue of animations to run.
  AnimationQueue animation_queue_;

  // The target of all layer animations.
  LayerAnimationDelegate* delegate_;

  // The currently running animations.
  RunningAnimations running_animations_;

  // Determines how animations are replaced.
  PreemptionStrategy preemption_strategy_;

  // The default length of animations.
  base::TimeDelta transition_duration_;

  // The default tween type for implicit transitions
  Tween::Type tween_type_;

  // Used for coordinating the starting of animations.
  base::TimeTicks last_step_time_;

  // True if we are being stepped by our container.
  bool is_started_;

  // This prevents the animator from automatically stepping through animations
  // and allows for manual stepping.
  bool disable_timer_for_test_;

  // This causes all animations to complete immediately.
  static bool disable_animations_for_test_;

  // Observers are notified when layer animations end, are scheduled or are
  // aborted.
  ObserverList<LayerAnimationObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimator);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATOR_H_
