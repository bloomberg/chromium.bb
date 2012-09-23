// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animator.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_layer_animation_observer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

class TestImplicitAnimationObserver : public ImplicitAnimationObserver {
 public:
  explicit TestImplicitAnimationObserver(bool notify_when_animator_destructed)
    : animations_completed_(false),
      notify_when_animator_destructed_(notify_when_animator_destructed) {
  }

  bool animations_completed() const { return animations_completed_; }
  void set_animations_completed(bool completed) {
    animations_completed_ = completed;
  }

 private:
  // ImplicitAnimationObserver implementation
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    animations_completed_ = true;
  }

  virtual bool RequiresNotificationWhenAnimatorDestroyed() const OVERRIDE {
    return notify_when_animator_destructed_;
  }

  bool animations_completed_;
  bool notify_when_animator_destructed_;

  DISALLOW_COPY_AND_ASSIGN(TestImplicitAnimationObserver);
};

// When notified that an animation has ended, stops all other animations.
class DeletingLayerAnimationObserver : public LayerAnimationObserver {
 public:
  DeletingLayerAnimationObserver(LayerAnimator* animator,
                                 LayerAnimationSequence* sequence)
    : animator_(animator),
      sequence_(sequence) {
  }

  virtual void OnLayerAnimationEnded(LayerAnimationSequence* sequence) {
    animator_->StopAnimating();
  }

  virtual void OnLayerAnimationAborted(LayerAnimationSequence* sequence) {}

  virtual void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) {}

 private:
  LayerAnimator* animator_;
  LayerAnimationSequence* sequence_;

  DISALLOW_COPY_AND_ASSIGN(DeletingLayerAnimationObserver);
};

class TestLayerAnimator : public LayerAnimator {
 public:
  TestLayerAnimator() : LayerAnimator(base::TimeDelta::FromSeconds(0)) {}

 protected:
  virtual ~TestLayerAnimator() {}

  virtual void ProgressAnimation(LayerAnimationSequence* sequence,
                                 base::TimeDelta delta) OVERRIDE {
    EXPECT_TRUE(HasAnimation(sequence));
    LayerAnimator::ProgressAnimation(sequence, delta);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimator);
};

// The test layer animation sequence updates a live instances count when it is
// created and destroyed.
class TestLayerAnimationSequence : public LayerAnimationSequence {
 public:
  TestLayerAnimationSequence(LayerAnimationElement* element,
                             int* num_live_instances)
      : LayerAnimationSequence(element),
        num_live_instances_(num_live_instances) {
    (*num_live_instances_)++;
  }

  virtual ~TestLayerAnimationSequence() {
    (*num_live_instances_)--;
  }

 private:
  int* num_live_instances_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationSequence);
};

} // namespace

// Checks that setting a property on an implicit animator causes an animation to
// happen.
TEST(LayerAnimatorTest, ImplicitAnimation) {
  scoped_refptr<LayerAnimator> animator(
      LayerAnimator::CreateImplicitAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  base::TimeTicks now = base::TimeTicks::Now();
  animator->SetOpacity(0.5);
  EXPECT_TRUE(animator->is_animating());
  element->Step(now + base::TimeDelta::FromSeconds(1));
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
}

// Checks that if the animator is a default animator, that implicit animations
// are not started.
TEST(LayerAnimatorTest, NoImplicitAnimation) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  animator->SetOpacity(0.5);
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
}

// Checks that StopAnimatingProperty stops animation for that property, and also
// skips the stopped animation to the end.
TEST(LayerAnimatorTest, StopAnimatingProperty) {
  scoped_refptr<LayerAnimator> animator(
      LayerAnimator::CreateImplicitAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  double target_opacity(0.5);
  gfx::Rect target_bounds(0, 0, 50, 50);
  animator->SetOpacity(target_opacity);
  animator->SetBounds(target_bounds);
  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);
  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
  animator->StopAnimatingProperty(LayerAnimationElement::BOUNDS);
  EXPECT_FALSE(animator->is_animating());
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Checks that multiple running animation for separate properties can be stopped
// simultaneously and that all animations are advanced to their target values.
TEST(LayerAnimatorTest, StopAnimating) {
  scoped_refptr<LayerAnimator> animator(
      LayerAnimator::CreateImplicitAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  double target_opacity(0.5);
  gfx::Rect target_bounds(0, 0, 50, 50);
  animator->SetOpacity(target_opacity);
  animator->SetBounds(target_bounds);
  EXPECT_TRUE(animator->is_animating());
  animator->StopAnimating();
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Schedule an animation that can run immediately. This is the trivial case and
// should result in the animation being started immediately.
TEST(LayerAnimatorTest, ScheduleAnimationThatCanRunImmediately) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
}

// Schedule two animations on separate properties. Both animations should
// start immediately and should progress in lock step.
TEST(LayerAnimatorTest, ScheduleTwoAnimationsThatCanRunImmediately) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  gfx::Rect start_bounds, target_bounds, middle_bounds;
  start_bounds = target_bounds = middle_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);
  delegate.SetBoundsFromAnimation(start_bounds);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), middle_bounds);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Schedule two animations on the same property. In this case, the two
// animations should run one after another.
TEST(LayerAnimatorTest, ScheduleTwoAnimationsOnSameProperty) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Schedule [{o}, {o,b}, {b}] and ensure that {b} doesn't run right away. That
// is, ensure that all animations targetting a particular property are run in
// order.
TEST(LayerAnimatorTest, ScheduleBlockedAnimation) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  gfx::Rect start_bounds, target_bounds, middle_bounds;
  start_bounds = target_bounds = middle_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);
  delegate.SetBoundsFromAnimation(start_bounds);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  scoped_ptr<LayerAnimationSequence> bounds_and_opacity(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  bounds_and_opacity->AddElement(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta));

  animator->ScheduleAnimation(bounds_and_opacity.release());

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateBoundsElement(start_bounds, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(3000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(4000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);
}

// Schedule {o} and then schedule {o} and {b} together. In this case, since
// ScheduleTogether is being used, the bounds animation should not start until
// the second opacity animation starts.
TEST(LayerAnimatorTest, ScheduleTogether) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double target_opacity(1.0);

  gfx::Rect start_bounds, target_bounds, middle_bounds;
  start_bounds = target_bounds = gfx::Rect(0, 0, 50, 50);
  start_bounds.set_x(-90);
  target_bounds.set_x(90);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);
  delegate.SetBoundsFromAnimation(start_bounds);

  animator->ScheduleAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  std::vector<LayerAnimationSequence*> sequences;
  sequences.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));
  sequences.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  animator->ScheduleTogether(sequences);

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), start_bounds);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Start animation (that can run immediately). This is the trivial case (see
// the trival case for ScheduleAnimation).
TEST(LayerAnimatorTest, StartAnimationThatCanRunImmediately) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
}

// Preempt by immediately setting new target.
TEST(LayerAnimatorTest, PreemptBySettingNewTarget) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->set_preemption_strategy(LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Preempt by animating to new target.
TEST(LayerAnimatorTest, PreemptByImmediatelyAnimatingToNewTarget) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(),
                  0.5 * (start_opacity + middle_opacity));

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Preempt by enqueuing the new animation.
TEST(LayerAnimatorTest, PreemptEnqueueNewAnimation) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  EXPECT_TRUE(animator->is_animating());

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Start an animation when there are sequences waiting in the queue. In this
// case, all pending and running animations should be finished, and the new
// animation started.
TEST(LayerAnimatorTest, PreemptyByReplacingQueuedAnimations) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double middle_opacity(0.5);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  animator->set_preemption_strategy(LayerAnimator::REPLACE_QUEUED_ANIMATIONS);

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(middle_opacity, delta)));

  // Queue should now have two animations. Starting a third should replace the
  // second.
  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(start_opacity, delta)));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);
}

// Test that cyclic sequences continue to animate.
TEST(LayerAnimatorTest, CyclicSequences) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  scoped_ptr<LayerAnimationSequence> sequence(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  sequence->AddElement(
      LayerAnimationElement::CreateOpacityElement(start_opacity, delta));

  sequence->set_is_cyclic(true);

  animator->StartAnimation(sequence.release());

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(2000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(3000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  // Skip ahead by a lot.
  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000000000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), start_opacity);

  // Skip ahead by a lot.
  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000001000));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);

  animator->StopAnimatingProperty(LayerAnimationElement::OPACITY);

  EXPECT_FALSE(animator->is_animating());
}

TEST(LayerAnimatorTest, AddObserverExplicit) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  animator->AddObserver(&observer);
  observer.set_requires_notification_when_animator_destroyed(true);

  EXPECT_TRUE(!observer.last_ended_sequence());

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(0.0f);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ(observer.last_ended_sequence(), sequence);

  // |sequence| has been destroyed. Recreate it to test abort.
  sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  animator->StartAnimation(sequence);

  animator = NULL;

  EXPECT_EQ(observer.last_aborted_sequence(), sequence);
}

// Tests that an observer added to a scoped settings object is still notified
// when the object goes out of scope.
TEST(LayerAnimatorTest, ImplicitAnimationObservers) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer(false);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  EXPECT_FALSE(observer.animations_completed());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  base::TimeTicks start_time = animator->last_step_time();
  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_FLOAT_EQ(0.0f, delegate.GetOpacityForAnimation());
}

// Tests that an observer added to a scoped settings object is still notified
// when the object goes out of scope due to the animation being interrupted.
TEST(LayerAnimatorTest, InterruptedImplicitAnimationObservers) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer(false);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  EXPECT_FALSE(observer.animations_completed());
  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer.animations_completed());
  // This should interrupt the implicit animation causing the observer to be
  // notified immediately.
  animator->SetOpacity(1.0f);
  EXPECT_TRUE(observer.animations_completed());
  EXPECT_FLOAT_EQ(1.0f, delegate.GetOpacityForAnimation());
}

// Tests that an observer added to a scoped settings object is not notified
// when the animator is destroyed unless explicitly requested.
TEST(LayerAnimatorTest, ImplicitObserversAtAnimatorDestruction) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  TestImplicitAnimationObserver observer_notify(true);
  TestImplicitAnimationObserver observer_do_not_notify(false);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  EXPECT_FALSE(observer_notify.animations_completed());
  EXPECT_FALSE(observer_do_not_notify.animations_completed());

  animator->SetOpacity(1.0f);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    settings.AddObserver(&observer_notify);
    settings.AddObserver(&observer_do_not_notify);
    animator->SetOpacity(0.0f);
  }

  EXPECT_FALSE(observer_notify.animations_completed());
  EXPECT_FALSE(observer_do_not_notify.animations_completed());
  animator = NULL;
  EXPECT_TRUE(observer_notify.animations_completed());
  EXPECT_FALSE(observer_do_not_notify.animations_completed());
}


TEST(LayerAnimatorTest, RemoveObserverShouldRemoveFromSequences) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationObserver observer;
  TestLayerAnimationObserver removed_observer;
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  sequence->AddObserver(&observer);
  sequence->AddObserver(&removed_observer);

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);
  EXPECT_TRUE(!observer.last_ended_sequence());
  EXPECT_EQ(removed_observer.last_scheduled_sequence(), sequence);
  EXPECT_TRUE(!removed_observer.last_ended_sequence());

  // This should stop the observer from observing sequence.
  animator->RemoveObserver(&removed_observer);

  base::TimeTicks start_time = animator->last_step_time();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ(observer.last_ended_sequence(), sequence);
  EXPECT_TRUE(!removed_observer.last_ended_sequence());
}

TEST(LayerAnimatorTest, ObserverReleasedBeforeAnimationSequenceEnds) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);

  scoped_ptr<TestLayerAnimationObserver> observer(
      new TestLayerAnimationObserver);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  animator->AddObserver(observer.get());

  delegate.SetOpacityFromAnimation(0.0f);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  animator->StartAnimation(sequence);

  // |observer| should be attached to |sequence|.
  EXPECT_EQ(static_cast<size_t>(1), sequence->observers_.size());

  // Now, release |observer|
  observer.reset();

  // And |sequence| should no longer be attached to |observer|.
  EXPECT_EQ(static_cast<size_t>(0), sequence->observers_.size());
}

TEST(LayerAnimatorTest, ObserverAttachedAfterAnimationStarted) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);

  TestImplicitAnimationObserver observer(false);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f);

  {
    ScopedLayerAnimationSettings setter(animator.get());

    base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
    LayerAnimationSequence* sequence = new LayerAnimationSequence(
        LayerAnimationElement::CreateOpacityElement(1.0f, delta));

    animator->StartAnimation(sequence);
    base::TimeTicks start_time = animator->last_step_time();
    element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

    setter.AddObserver(&observer);

    // Start observing an in-flight animation.
    sequence->AddObserver(&observer);

    element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  }

  EXPECT_TRUE(observer.animations_completed());
}

TEST(LayerAnimatorTest, ObserverDetachedBeforeAnimationFinished) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);

  TestImplicitAnimationObserver observer(false);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f);
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  {
    ScopedLayerAnimationSettings setter(animator.get());
    setter.AddObserver(&observer);

    animator->StartAnimation(sequence);
    base::TimeTicks start_time = animator->last_step_time();
    element->Step(start_time + base::TimeDelta::FromMilliseconds(500));
  }

  EXPECT_FALSE(observer.animations_completed());

  // Stop observing an in-flight animation.
  sequence->RemoveObserver(&observer);

  EXPECT_TRUE(observer.animations_completed());
}

// This checks that if an animation is deleted due to a callback, that the
// animator does not try to use the deleted animation. For example, if we have
// two running animations, and the first finishes and the resulting callback
// causes the second to be deleted, we should not attempt to animate the second
// animation.
TEST(LayerAnimatorTest, ObserverDeletesAnimations) {
  LayerAnimator::set_disable_animations_for_test(false);
  scoped_refptr<LayerAnimator> animator(new TestLayerAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double target_opacity(1.0);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(5, 5, 5, 5);

  delegate.SetOpacityFromAnimation(start_opacity);
  delegate.SetBoundsFromAnimation(start_bounds);

  base::TimeDelta opacity_delta = base::TimeDelta::FromSeconds(1);
  base::TimeDelta halfway_delta = base::TimeDelta::FromSeconds(2);
  base::TimeDelta bounds_delta = base::TimeDelta::FromSeconds(3);

  LayerAnimationSequence* to_delete = new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, bounds_delta));

  scoped_ptr<DeletingLayerAnimationObserver> observer(
      new DeletingLayerAnimationObserver(animator.get(), to_delete));

  animator->AddObserver(observer.get());

  animator->StartAnimation(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(
              target_opacity, opacity_delta)));

  animator->StartAnimation(to_delete);

  base::TimeTicks start_time = animator->last_step_time();
  element->Step(start_time + halfway_delta);

  animator->RemoveObserver(observer.get());
}

// Check that setting a property during an animation with a default animator
// cancels the original animation.
TEST(LayerAnimatorTest, SettingPropertyDuringAnAnimation) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  double start_opacity(0.0);
  double target_opacity(1.0);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(start_opacity);

  scoped_ptr<LayerAnimationSequence> sequence(
      new LayerAnimationSequence(
          LayerAnimationElement::CreateOpacityElement(target_opacity, delta)));

  animator->StartAnimation(sequence.release());

  animator->SetOpacity(0.5);

  EXPECT_FALSE(animator->is_animating());
  EXPECT_EQ(0.5, animator->GetTargetOpacity());
}

// Tests that the preemption mode IMMEDIATELY_SET_NEW_TARGET, doesn't cause the
// second sequence to be leaked.
TEST(LayerAnimatorTest, ImmediatelySettingNewTargetDoesNotLeak) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_preemption_strategy(LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect middle_bounds(10, 10, 100, 100);
  gfx::Rect target_bounds(5, 5, 5, 5);

  delegate.SetBoundsFromAnimation(start_bounds);

  {
    // start an implicit bounds animation.
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetBounds(middle_bounds);
  }

  EXPECT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));

  int num_live_instances = 0;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  scoped_ptr<TestLayerAnimationSequence> sequence(
      new TestLayerAnimationSequence(
          LayerAnimationElement::CreateBoundsElement(target_bounds, delta),
          &num_live_instances));

  EXPECT_EQ(1, num_live_instances);

  // This should interrupt the running sequence causing us to immediately set
  // the target value. The sequence should alse be destructed.
  animator->StartAnimation(sequence.release());

  EXPECT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
  EXPECT_EQ(0, num_live_instances);
  CheckApproximatelyEqual(delegate.GetBoundsForAnimation(), target_bounds);
}

// Verifies GetTargetOpacity() works when multiple sequences are scheduled.
TEST(LayerAnimatorTest, GetTargetOpacity) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetOpacity(0.5);
    EXPECT_EQ(0.5, animator->GetTargetOpacity());

    // Because the strategy is ENQUEUE_NEW_ANIMATION the target should now be 1.
    animator->SetOpacity(1.0);
    EXPECT_EQ(1.0, animator->GetTargetOpacity());
  }
}

// Verifies GetTargetBrightness() works when multiple sequences are scheduled.
TEST(LayerAnimatorTest, GetTargetBrightness) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetBrightnessFromAnimation(0.0);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetBrightness(0.5);
    EXPECT_EQ(0.5, animator->GetTargetBrightness());

    // Because the strategy is ENQUEUE_NEW_ANIMATION the target should now be 1.
    animator->SetBrightness(1.0);
    EXPECT_EQ(1.0, animator->GetTargetBrightness());
  }
}

// Verifies GetTargetGrayscale() works when multiple sequences are scheduled.
TEST(LayerAnimatorTest, GetTargetGrayscale) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetGrayscaleFromAnimation(0.0);

  {
    ScopedLayerAnimationSettings settings(animator.get());
    animator->SetGrayscale(0.5);
    EXPECT_EQ(0.5, animator->GetTargetGrayscale());

    // Because the strategy is ENQUEUE_NEW_ANIMATION the target should now be 1.
    animator->SetGrayscale(1.0);
    EXPECT_EQ(1.0, animator->GetTargetGrayscale());
  }
}

// Verifies SchedulePauseForProperties().
TEST(LayerAnimatorTest, SchedulePauseForProperties) {
  scoped_refptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_preemption_strategy(LayerAnimator::ENQUEUE_NEW_ANIMATION);
  animator->SchedulePauseForProperties(base::TimeDelta::FromMilliseconds(100),
                                       LayerAnimationElement::TRANSFORM,
                                       LayerAnimationElement::BOUNDS, -1);
  EXPECT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::TRANSFORM));
  EXPECT_TRUE(animator->IsAnimatingProperty(LayerAnimationElement::BOUNDS));
  EXPECT_FALSE(animator->IsAnimatingProperty(LayerAnimationElement::OPACITY));
}


class AnimatorOwner {
public:
  AnimatorOwner()
      : animator_(LayerAnimator::CreateDefaultAnimator()) {
  }

  LayerAnimator* animator() { return animator_.get(); }

private:
  scoped_refptr<LayerAnimator> animator_;

  DISALLOW_COPY_AND_ASSIGN(AnimatorOwner);
};

class DeletingObserver : public LayerAnimationObserver {
public:
  DeletingObserver(bool* was_deleted)
      : animator_owner_(new AnimatorOwner),
        delete_on_animation_ended_(false),
        delete_on_animation_aborted_(false),
        delete_on_animation_scheduled_(false),
        was_deleted_(was_deleted) {
    animator()->AddObserver(this);
  }

  ~DeletingObserver() {
    animator()->RemoveObserver(this);
    *was_deleted_ = true;
  }

  LayerAnimator* animator() { return animator_owner_->animator(); }

  bool delete_on_animation_ended() const {
    return delete_on_animation_ended_;
  }
  void set_delete_on_animation_ended(bool enabled) {
    delete_on_animation_ended_ = enabled;
  }

  bool delete_on_animation_aborted() const {
    return delete_on_animation_aborted_;
  }
  void set_delete_on_animation_aborted(bool enabled) {
    delete_on_animation_aborted_ = enabled;
  }

  bool delete_on_animation_scheduled() const {
    return delete_on_animation_scheduled_;
  }
  void set_delete_on_animation_scheduled(bool enabled) {
    delete_on_animation_scheduled_ = enabled;
  }

  // LayerAnimationObserver implementation.
  virtual void OnLayerAnimationEnded(
      LayerAnimationSequence* sequence) OVERRIDE {
    if (delete_on_animation_ended_)
      delete this;
  }

  virtual void OnLayerAnimationAborted(
      LayerAnimationSequence* sequence) OVERRIDE {
    if (delete_on_animation_aborted_)
      delete this;
  }

  virtual void OnLayerAnimationScheduled(
      LayerAnimationSequence* sequence) {
    if (delete_on_animation_scheduled_)
      delete this;
  }

private:
  scoped_ptr<AnimatorOwner> animator_owner_;
  bool delete_on_animation_ended_;
  bool delete_on_animation_aborted_;
  bool delete_on_animation_scheduled_;
  bool* was_deleted_;

  DISALLOW_COPY_AND_ASSIGN(DeletingObserver);
};

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterFinishingAnimation) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_ended(true);
  observer->set_delete_on_animation_aborted(true);
  LayerAnimator* animator = observer->animator();
  AnimationContainerElement* element = observer->animator();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* opacity_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));
  animator->StartAnimation(opacity_sequence);

  delta = base::TimeDelta::FromSeconds(2);
  LayerAnimationSequence* bounds_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta));
  animator->StartAnimation(bounds_sequence);

  base::TimeTicks start_time = animator->last_step_time();
  element->Step(start_time + base::TimeDelta::FromMilliseconds(1500));

  EXPECT_TRUE(observer_was_deleted);
}

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterStoppingAnimating) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_ended(true);
  observer->set_delete_on_animation_aborted(true);
  LayerAnimator* animator = observer->animator();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence* opacity_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));
  animator->StartAnimation(opacity_sequence);

  delta = base::TimeDelta::FromSeconds(2);
  LayerAnimationSequence* bounds_sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta));
  animator->StartAnimation(bounds_sequence);

  animator->StopAnimating();

  EXPECT_TRUE(observer_was_deleted);
}

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterScheduling) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_scheduled(true);
  LayerAnimator* animator = observer->animator();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds);

  std::vector<LayerAnimationSequence*> to_start;

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta)));

  delta = base::TimeDelta::FromSeconds(2);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  animator->ScheduleTogether(to_start);

  EXPECT_TRUE(observer_was_deleted);
}

TEST(LayerAnimatorTest, ObserverDeletesAnimatorAfterAborted) {
  bool observer_was_deleted = false;
  DeletingObserver* observer = new DeletingObserver(&observer_was_deleted);
  observer->set_delete_on_animation_aborted(true);
  LayerAnimator* animator = observer->animator();
  animator->set_preemption_strategy(
      LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);

  delegate.SetOpacityFromAnimation(0.0f);

  gfx::Rect start_bounds(0, 0, 50, 50);
  gfx::Rect target_bounds(10, 10, 100, 100);

  delegate.SetBoundsFromAnimation(start_bounds);

  std::vector<LayerAnimationSequence*> to_start;

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta)));

  delta = base::TimeDelta::FromSeconds(2);
  to_start.push_back(new LayerAnimationSequence(
      LayerAnimationElement::CreateBoundsElement(target_bounds, delta)));

  animator->ScheduleTogether(to_start);

  EXPECT_FALSE(observer_was_deleted);

  animator->StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta)));

  EXPECT_TRUE(observer_was_deleted);
}

}  // namespace ui
