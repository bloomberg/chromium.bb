// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
#pragma once

#include <deque>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/gfx/compositor/compositor_export.h"
#include "ui/gfx/compositor/layer_animation_element.h"

namespace gfx {
class Rect;
}

namespace ui {
class Animation;
class Layer;
class LayerAnimationSequence;
class LayerAnimatorDelegate;
class Transform;

// When a property of layer needs to be changed it is set by way of
// LayerAnimator. This enables LayerAnimator to animate property changes.
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

  // Sets the layer animation delegate the animator is associated with. The
  // animator does not own the delegate.
  void SetDelegate(LayerAnimatorDelegate* delegate);

  // Sets the animation preemption strategy. This determines the behaviour if
  // a property is set during an animation. The default is
  // IMMEDIATELY_SET_NEW_TARGET (see ImmediatelySetNewTarget below).
  void set_preemption_strategy(PreemptionStrategy strategy) {
    preemption_strategy_ = strategy;
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

  // These are cover functions that create sequences for you to wrap the given
  // elements. These sequences are then passed to the corresponding function
  // above.
  void StartAnimationElement(LayerAnimationElement* element);
  void ScheduleAnimationElement(LayerAnimationElement* element);
  void ScheduleElementsTogether(
      const std::vector<LayerAnimationElement*>& element);

  // Returns true if there is an animation in the queue (animations remain in
  // the queue until they complete).
  bool is_animating() const { return !animation_queue_.empty(); }

  // Stops animating the given property. No effect if there is no running
  // animation for the given property. Skips to the final state of the
  // animation.
  void StopAnimatingProperty(
      LayerAnimationElement::AnimatableProperty property);

  // Stops all animation and clears any queued animations.
  void StopAnimating();

  // For testing purposes only.
  void set_disable_timer_for_test(bool enabled) {
    disable_timer_for_test_ = enabled;
  }
  base::TimeTicks get_last_step_time_for_test() { return last_step_time_; }

  // Scoped settings allow you to temporarily change the animator's settings and
  // these changes are reverted when the object is destroyed. NOTE: when the
  // settings object is created, it applies the default transition duration
  // (200ms).
  class ScopedSettings {
   public:
    explicit ScopedSettings(LayerAnimator* animator);
    virtual ~ScopedSettings();

    void SetTransitionDuration(base::TimeDelta duration);

   private:
    LayerAnimator* animator_;
    base::TimeDelta old_transition_duration_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSettings);
  };

 protected:
  LayerAnimatorDelegate* delegate() { return delegate_; }

 private:
  friend class TransientSettings;

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
  void RemoveAnimation(LayerAnimationSequence* sequence);

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

  // This is the queue of animations to run.
  AnimationQueue animation_queue_;

  // The target of all layer animations.
  LayerAnimatorDelegate* delegate_;

  // The currently running animations.
  RunningAnimations running_animations_;

  // Determines how animations are replaced.
  PreemptionStrategy preemption_strategy_;

  // The default length of animations.
  base::TimeDelta transition_duration_;

  // Used for coordinating the starting of animations.
  base::TimeTicks last_step_time_;

  // True if we are being stepped by our container.
  bool is_started_;

  // This prevents the animator from automatically stepping through animations
  // and allows for manual stepping.
  bool disable_timer_for_test_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimator);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_ANIMATOR_H_
