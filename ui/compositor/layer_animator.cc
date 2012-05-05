// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animator.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_container.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"

namespace ui {

class LayerAnimator;

namespace {

static const base::TimeDelta kDefaultTransitionDuration =
    base::TimeDelta::FromMilliseconds(120);

static const base::TimeDelta kTimerInterval =
    base::TimeDelta::FromMilliseconds(10);

} // namespace;

// LayerAnimator public --------------------------------------------------------

LayerAnimator::LayerAnimator(base::TimeDelta transition_duration)
    : delegate_(NULL),
      preemption_strategy_(IMMEDIATELY_SET_NEW_TARGET),
      transition_duration_(transition_duration),
      tween_type_(Tween::LINEAR),
      is_started_(false),
      disable_timer_for_test_(false) {
}

LayerAnimator::~LayerAnimator() {
  for (size_t i = 0; i < running_animations_.size(); ++i)
    running_animations_[i].sequence->OnAnimatorDestroyed();
  ClearAnimations();
}

// static
bool LayerAnimator::disable_animations_for_test_ = false;

// static
LayerAnimator* LayerAnimator::CreateDefaultAnimator() {
  return new LayerAnimator(base::TimeDelta::FromMilliseconds(0));
}

// static
LayerAnimator* LayerAnimator::CreateImplicitAnimator() {
  return new LayerAnimator(kDefaultTransitionDuration);
}

void LayerAnimator::SetTransform(const Transform& transform) {
  base::TimeDelta duration = transition_duration_;
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateTransformElement(transform, duration));
  element->set_tween_type(tween_type_);
  StartAnimation(new LayerAnimationSequence(element.release()));
}

Transform LayerAnimator::GetTargetTransform() const {
  LayerAnimationElement::TargetValue target(delegate());
  GetTargetValue(&target);
  return target.transform;
}

void LayerAnimator::SetBounds(const gfx::Rect& bounds) {
  base::TimeDelta duration = transition_duration_;
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateBoundsElement(bounds, duration));
  element->set_tween_type(tween_type_);
  StartAnimation(new LayerAnimationSequence(element.release()));
}

gfx::Rect LayerAnimator::GetTargetBounds() const {
  LayerAnimationElement::TargetValue target(delegate());
  GetTargetValue(&target);
  return target.bounds;
}

void LayerAnimator::SetOpacity(float opacity) {
  base::TimeDelta duration = transition_duration_;
  scoped_ptr<LayerAnimationElement> element(
      LayerAnimationElement::CreateOpacityElement(opacity, duration));
  element->set_tween_type(tween_type_);
  StartAnimation(new LayerAnimationSequence(element.release()));
}

float LayerAnimator::GetTargetOpacity() const {
  LayerAnimationElement::TargetValue target(delegate());
  GetTargetValue(&target);
  return target.opacity;
}

void LayerAnimator::SetVisibility(bool visibility) {
  base::TimeDelta duration = transition_duration_;

  // Tween type doesn't matter for visibility.
  StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateVisibilityElement(
          visibility, duration)));
}

bool LayerAnimator::GetTargetVisibility() const {
  LayerAnimationElement::TargetValue target(delegate());
  GetTargetValue(&target);
  return target.visibility;
}

void LayerAnimator::SetDelegate(LayerAnimationDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void LayerAnimator::StartAnimation(LayerAnimationSequence* animation) {
  OnScheduled(animation);
  if (!StartSequenceImmediately(animation)) {
    // Attempt to preempt a running animation.
    switch (preemption_strategy_) {
      case IMMEDIATELY_SET_NEW_TARGET:
        ImmediatelySetNewTarget(animation);
        break;
      case IMMEDIATELY_ANIMATE_TO_NEW_TARGET:
        ImmediatelyAnimateToNewTarget(animation);
        break;
      case ENQUEUE_NEW_ANIMATION:
        EnqueueNewAnimation(animation);
        break;
      case REPLACE_QUEUED_ANIMATIONS:
        ReplaceQueuedAnimations(animation);
        break;
      case BLEND_WITH_CURRENT_ANIMATION: {
        // TODO(vollick) Add support for blended sequences and use them here.
        NOTIMPLEMENTED();
        break;
      }
    }
  }
  FinishAnyAnimationWithZeroDuration();
  UpdateAnimationState();
}

void LayerAnimator::ScheduleAnimation(LayerAnimationSequence* animation) {
  OnScheduled(animation);
  if (is_animating()) {
    animation_queue_.push_back(make_linked_ptr(animation));
    ProcessQueue();
  } else {
    StartSequenceImmediately(animation);
  }
  UpdateAnimationState();
}

void LayerAnimator::ScheduleTogether(
    const std::vector<LayerAnimationSequence*>& animations) {
  // Collect all the affected properties.
  LayerAnimationElement::AnimatableProperties animated_properties;
  std::vector<LayerAnimationSequence*>::const_iterator iter;
  for (iter = animations.begin(); iter != animations.end(); ++iter) {
    animated_properties.insert((*iter)->properties().begin(),
                               (*iter)->properties().end());
  }

  // Scheduling a zero duration pause that affects all the animated properties
  // will prevent any of the sequences from animating until there are no
  // running animations that affect any of these properties.
  ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreatePauseElement(animated_properties,
                                                base::TimeDelta())));

  // These animations (provided they don't animate any common properties) will
  // now animate together if trivially scheduled.
  for (iter = animations.begin(); iter != animations.end(); ++iter) {
    ScheduleAnimation(*iter);
  }

  UpdateAnimationState();
}

bool LayerAnimator::IsAnimatingProperty(
    LayerAnimationElement::AnimatableProperty property) const {
  for (AnimationQueue::const_iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if ((*queue_iter)->properties().find(property) !=
        (*queue_iter)->properties().end()) {
      return true;
    }
  }
  return false;
}

void LayerAnimator::StopAnimatingProperty(
    LayerAnimationElement::AnimatableProperty property) {
  while (true) {
    RunningAnimation* running = GetRunningAnimation(property);
    if (!running)
      break;
    FinishAnimation(running->sequence);
  }
}

void LayerAnimator::StopAnimating() {
  while (is_animating())
    FinishAnimation(running_animations_[0].sequence);
}

void LayerAnimator::AddObserver(LayerAnimationObserver* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void LayerAnimator::RemoveObserver(LayerAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
  // Remove the observer from all sequences as well.
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    (*queue_iter)->RemoveObserver(observer);
  }
}

// LayerAnimator protected -----------------------------------------------------

bool LayerAnimator::ProgressAnimation(LayerAnimationSequence* sequence,
                                      base::TimeDelta delta) {
  return sequence->Progress(delta, delegate());
}


bool LayerAnimator::HasAnimation(LayerAnimationSequence* sequence) const {
  for (AnimationQueue::const_iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if ((*queue_iter).get() == sequence)
      return true;
  }
  return false;
}

// LayerAnimator private -------------------------------------------------------

void LayerAnimator::Step(base::TimeTicks now) {
  TRACE_EVENT0("ui", "LayerAnimator::Step");

  last_step_time_ = now;
  // We need to make a copy of the running animations because progressing them
  // and finishing them may indirectly affect the collection of running
  // animations.
  RunningAnimations running_animations_copy = running_animations_;
  bool needs_redraw = false;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!HasAnimation(running_animations_copy[i].sequence))
      continue;

    base::TimeDelta delta = now - running_animations_copy[i].start_time;
    if (delta >= running_animations_copy[i].sequence->duration() &&
        !running_animations_copy[i].sequence->is_cyclic()) {
      FinishAnimation(running_animations_copy[i].sequence);
    } else if (ProgressAnimation(running_animations_copy[i].sequence, delta))
      needs_redraw = true;
  }

  if (needs_redraw)
    delegate()->ScheduleDrawForAnimation();
}

void LayerAnimator::SetStartTime(base::TimeTicks start_time) {
  // Do nothing.
}

base::TimeDelta LayerAnimator::GetTimerInterval() const {
  return kTimerInterval;
}

void LayerAnimator::UpdateAnimationState() {
  if (disable_timer_for_test_)
    return;

  static ui::AnimationContainer* container = NULL;
  if (!container) {
    container = new AnimationContainer();
    container->AddRef();
  }

  const bool should_start = is_animating();
  if (should_start && !is_started_)
    container->Start(this);
  else if (!should_start && is_started_)
    container->Stop(this);

  is_started_ = should_start;
}

LayerAnimationSequence* LayerAnimator::RemoveAnimation(
    LayerAnimationSequence* sequence) {
  linked_ptr<LayerAnimationSequence> to_return;

  // First remove from running animations
  for (RunningAnimations::iterator iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    if ((*iter).sequence == sequence) {
      running_animations_.erase(iter);
      break;
    }
  }

  // Then remove from the queue
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if ((*queue_iter) == sequence) {
      to_return = *queue_iter;
      animation_queue_.erase(queue_iter);
      break;
    }
  }

  return to_return.release();
}

void LayerAnimator::FinishAnimation(LayerAnimationSequence* sequence) {
  scoped_ptr<LayerAnimationSequence> removed(RemoveAnimation(sequence));
  sequence->Progress(sequence->duration(), delegate());
  ProcessQueue();
  UpdateAnimationState();
}

void LayerAnimator::FinishAnyAnimationWithZeroDuration() {
  // Special case: if we've started a 0 duration animation, just finish it now
  // and get rid of it. We need to make a copy because Progress may indirectly
  // cause new animations to start running.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!HasAnimation(running_animations_copy[i].sequence))
      continue;

    if (running_animations_copy[i].sequence->duration() == base::TimeDelta()) {
      running_animations_copy[i].sequence->Progress(
          running_animations_copy[i].sequence->duration(), delegate());
      scoped_ptr<LayerAnimationSequence> removed(
          RemoveAnimation(running_animations_copy[i].sequence));
    }
  }
  ProcessQueue();
  UpdateAnimationState();
}

void LayerAnimator::ClearAnimations() {
  // Abort should never affect the set of running animations, but just in case
  // clients are badly behaved, we will use a copy of the running animations.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!HasAnimation(running_animations_copy[i].sequence))
      continue;

    scoped_ptr<LayerAnimationSequence> removed(
        RemoveAnimation(running_animations_copy[i].sequence));
    if (removed.get())
      removed->Abort();
  }
  // This *should* have cleared the list of running animations.
  DCHECK(running_animations_.empty());
  running_animations_.clear();
  animation_queue_.clear();
  UpdateAnimationState();
}

LayerAnimator::RunningAnimation* LayerAnimator::GetRunningAnimation(
    LayerAnimationElement::AnimatableProperty property) {
  for (RunningAnimations::iterator iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    if ((*iter).sequence->properties().find(property) !=
        (*iter).sequence->properties().end())
      return &(*iter);
  }
  return NULL;
}

void LayerAnimator::AddToQueueIfNotPresent(LayerAnimationSequence* animation) {
  // If we don't have the animation in the queue yet, add it.
  bool found_sequence = false;
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if ((*queue_iter) == animation) {
      found_sequence = true;
      break;
    }
  }

  if (!found_sequence)
    animation_queue_.push_front(make_linked_ptr(animation));
}

void LayerAnimator::RemoveAllAnimationsWithACommonProperty(
    LayerAnimationSequence* sequence, bool abort) {
  // For all the running animations, if they animate the same property,
  // progress them to the end and remove them. Note, Aborting or Progressing
  // animations may affect the collection of running animations, so we need to
  // operate on a copy.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!HasAnimation(running_animations_copy[i].sequence))
      continue;

    if (running_animations_copy[i].sequence->HasCommonProperty(
            sequence->properties())) {
      scoped_ptr<LayerAnimationSequence> removed(
          RemoveAnimation(running_animations_copy[i].sequence));
      if (abort)
        running_animations_copy[i].sequence->Abort();
      else
        running_animations_copy[i].sequence->Progress(
            running_animations_copy[i].sequence->duration(), delegate());
    }
  }

  // Same for the queued animations that haven't been started. Again, we'll
  // need to operate on a copy.
  std::vector<LayerAnimationSequence*> sequences;
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter)
    sequences.push_back((*queue_iter).get());

  for (size_t i = 0; i < sequences.size(); ++i) {
    if (!HasAnimation(sequences[i]))
      continue;

    if (sequences[i]->HasCommonProperty(sequence->properties())) {
      scoped_ptr<LayerAnimationSequence> removed(
          RemoveAnimation(sequences[i]));
      if (abort)
        sequences[i]->Abort();
      else
        sequences[i]->Progress(sequences[i]->duration(), delegate());
    }
  }
}

void LayerAnimator::ImmediatelySetNewTarget(LayerAnimationSequence* sequence) {
  // Ensure that sequence is disposed of when this function completes.
  scoped_ptr<LayerAnimationSequence> to_dispose(sequence);
  const bool abort = false;
  RemoveAllAnimationsWithACommonProperty(sequence, abort);
  LayerAnimationSequence* removed = RemoveAnimation(sequence);
  DCHECK(removed == NULL || removed == sequence);
  sequence->Progress(sequence->duration(), delegate());
}

void LayerAnimator::ImmediatelyAnimateToNewTarget(
    LayerAnimationSequence* sequence) {
  const bool abort = true;
  RemoveAllAnimationsWithACommonProperty(sequence, abort);
  AddToQueueIfNotPresent(sequence);
  StartSequenceImmediately(sequence);
}

void LayerAnimator::EnqueueNewAnimation(LayerAnimationSequence* sequence) {
  // It is assumed that if there was no conflicting animation, we would
  // not have been called. No need to check for a collision; just
  // add to the queue.
  animation_queue_.push_back(make_linked_ptr(sequence));
  ProcessQueue();
}

void LayerAnimator::ReplaceQueuedAnimations(LayerAnimationSequence* sequence) {
  // Remove all animations that aren't running. Note: at each iteration i is
  // incremented or an element is removed from the queue, so
  // animation_queue_.size() - i is always decreasing and we are always making
  // progress towards the loop terminating.
  for (size_t i = 0; i < animation_queue_.size();) {
    bool is_running = false;
    for (RunningAnimations::const_iterator iter = running_animations_.begin();
         iter != running_animations_.end(); ++iter) {
      if ((*iter).sequence == animation_queue_[i]) {
        is_running = true;
        break;
      }
    }
    if (!is_running)
      scoped_ptr<LayerAnimationSequence>(
          RemoveAnimation(animation_queue_[i].get()));
    else
      ++i;
  }
  animation_queue_.push_back(make_linked_ptr(sequence));
  ProcessQueue();
}

void LayerAnimator::ProcessQueue() {
  bool started_sequence = false;
  do {
    started_sequence = false;
    // Build a list of all currently animated properties.
    LayerAnimationElement::AnimatableProperties animated;
    for (RunningAnimations::const_iterator iter = running_animations_.begin();
         iter != running_animations_.end(); ++iter) {
      animated.insert((*iter).sequence->properties().begin(),
                      (*iter).sequence->properties().end());
    }

    // Try to find an animation that doesn't conflict with an animated
    // property or a property that will be animated before it. Note: starting
    // the animation may indirectly cause more animations to be started, so we
    // need to operate on a copy.
    std::vector<LayerAnimationSequence*> sequences;
    for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
         queue_iter != animation_queue_.end(); ++queue_iter)
      sequences.push_back((*queue_iter).get());

    for (size_t i = 0; i < sequences.size(); ++i) {
      if (!sequences[i]->HasCommonProperty(animated)) {
        StartSequenceImmediately(sequences[i]);
        started_sequence = true;
        break;
      }

      // Animation couldn't be started. Add its properties to the collection so
      // that we don't start a conflicting animation. For example, if our queue
      // has the elements { {T,B}, {B} } (that is, an element that animates both
      // the transform and the bounds followed by an element that animates the
      // bounds), and we're currently animating the transform, we can't start
      // the first element because it animates the transform, too. We cannot
      // start the second element, either, because the first element animates
      // bounds too, and needs to go first.
      animated.insert(sequences[i]->properties().begin(),
                      sequences[i]->properties().end());
    }

    // If we started a sequence, try again. We may be able to start several.
  } while (started_sequence);
}

bool LayerAnimator::StartSequenceImmediately(LayerAnimationSequence* sequence) {
  // Ensure that no one is animating one of the sequence's properties already.
  for (RunningAnimations::const_iterator iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    if ((*iter).sequence->HasCommonProperty(sequence->properties()))
      return false;
  }

  // All clear, actually start the sequence. Note: base::TimeTicks::Now has
  // a resolution that can be as bad as 15ms. If this causes glitches in the
  // animations, this can be switched to HighResNow() (animation uses Now()
  // internally).
  base::TimeTicks start_time = is_animating()
      ? last_step_time_
      : base::TimeTicks::Now();

  running_animations_.push_back(RunningAnimation(sequence, start_time));

  // Need to keep a reference to the animation.
  AddToQueueIfNotPresent(sequence);

  // Ensure that animations get stepped at their start time.
  Step(start_time);

  return true;
}

void LayerAnimator::GetTargetValue(
    LayerAnimationElement::TargetValue* target) const {
  for (AnimationQueue::const_iterator iter = animation_queue_.begin();
       iter != animation_queue_.end(); ++iter) {
    (*iter)->GetTargetValue(target);
  }
}

void LayerAnimator::OnScheduled(LayerAnimationSequence* sequence) {
  if (observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationObserver>::Iterator it(observers_);
    LayerAnimationObserver* obs;
    while ((obs = it.GetNext()) != NULL) {
      sequence->AddObserver(obs);
    }
  }
  sequence->OnScheduled();
}

}  // namespace ui
