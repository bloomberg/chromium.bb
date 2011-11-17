// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer_animator.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/compositor/layer_animation_delegate.h"
#include "ui/gfx/compositor/layer_animation_element.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/test_layer_animation_delegate.h"
#include "ui/gfx/compositor/test_layer_animation_observer.h"
#include "ui/gfx/compositor/test_utils.h"

namespace ui {

namespace {

// Checks that setting a property on an implicit animator causes an animation to
// happen.
TEST(LayerAnimatorTest, ImplicitAnimation) {
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateImplicitAnimator());
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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  base::TimeTicks now = base::TimeTicks::Now();
  animator->SetOpacity(0.5);
  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), 0.5);
}

// Checks that StopAnimatingProperty stops animation for that property, and also
// skips the stopped animation to the end.
TEST(LayerAnimatorTest, StopAnimatingProperty) {
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateImplicitAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  base::TimeTicks now = base::TimeTicks::Now();
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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateImplicitAnimator());
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  base::TimeTicks now = base::TimeTicks::Now();
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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_TRUE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), middle_opacity);

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(animator->is_animating());
  EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(), target_opacity);
}

// Preempt by immediately setting new target.
TEST(LayerAnimatorTest, PreemptBySettingNewTarget) {
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

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
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  animator->AddObserver(&observer);

  EXPECT_TRUE(!observer.last_ended_sequence());

  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);

  delegate.SetOpacityFromAnimation(0.0f);

  LayerAnimationSequence* sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  animator->StartAnimation(sequence);

  EXPECT_EQ(observer.last_scheduled_sequence(), sequence);

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ(observer.last_ended_sequence(), sequence);

  // |sequence| has been destroyed. Recreate it to test abort.
  sequence = new LayerAnimationSequence(
      LayerAnimationElement::CreateOpacityElement(1.0f, delta));

  animator->StartAnimation(sequence);

  animator.reset();

  EXPECT_EQ(observer.last_aborted_sequence(), sequence);
}

TEST(LayerAnimatorTest, AddObserverImplicit) {
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
  AnimationContainerElement* element = animator.get();
  animator->set_disable_timer_for_test(true);
  TestLayerAnimationObserver observer;
  TestLayerAnimationDelegate delegate;
  animator->SetDelegate(&delegate);
  animator->AddObserver(&observer);

  // Should end a sequence with the default animator.
  EXPECT_TRUE(!observer.last_ended_sequence());
  animator->SetOpacity(1.0f);
  base::TimeTicks start_time = base::TimeTicks::Now();
  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(observer.last_ended_sequence());

  TestLayerAnimationObserver scoped_observer;
  {
    LayerAnimator::ScopedSettings settings(animator.get());
    settings.AddObserver(&scoped_observer);
    for (int i = 0; i < 2; ++i) {
      // reset the observer
      scoped_observer = TestLayerAnimationObserver();
      EXPECT_TRUE(!scoped_observer.last_ended_sequence());
      animator->SetOpacity(1.0f);
      start_time = animator->get_last_step_time_for_test();
      element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
      EXPECT_FALSE(!scoped_observer.last_ended_sequence());
    }
  }

  scoped_observer = TestLayerAnimationObserver();
  EXPECT_TRUE(!scoped_observer.last_ended_sequence());
  animator->SetOpacity(1.0f);
  start_time = base::TimeTicks::Now();
  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(!scoped_observer.last_ended_sequence());
}

TEST(LayerAnimatorTest, RemoveObserverShouldRemoveFromSequences) {
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

  base::TimeTicks start_time = animator->get_last_step_time_for_test();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ(observer.last_ended_sequence(), sequence);
  EXPECT_TRUE(!removed_observer.last_ended_sequence());
}

// Check that setting a property during an animation with a default animator
// cancels the original animation.
TEST(LayerAnimatorTest, SettingPropertyDuringAnAnimation) {
  scoped_ptr<LayerAnimator> animator(LayerAnimator::CreateDefaultAnimator());
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

} // namespace

} // namespace ui
