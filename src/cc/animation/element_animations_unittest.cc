// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "cc/animation/transform_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {
namespace {

using base::TimeDelta;
using base::TimeTicks;

static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
}

// An ElementAnimations cannot be ticked at 0.0, since an animation
// with start time 0.0 is treated as an animation whose start time has
// not yet been set.
const TimeTicks kInitialTickTime = TicksFromSecondsF(1.0);

class ElementAnimationsTest : public AnimationTimelinesTest {
 public:
  ElementAnimationsTest() = default;
  ~ElementAnimationsTest() override = default;

  void SetUp() override {
    AnimationTimelinesTest::SetUp();
  }

  void CreateImplTimelineAndAnimation() override {
    AnimationTimelinesTest::CreateImplTimelineAndAnimation();
  }

  std::unique_ptr<AnimationEvents> CreateEventsForTesting() {
    auto mutator_events = host_impl_->CreateEvents();
    return base::WrapUnique(
        static_cast<AnimationEvents*>(mutator_events.release()));
  }
};

// See animation_unittest.cc for integration with Animation.

TEST_F(ElementAnimationsTest, AttachToLayerInActiveTree) {
  // Set up the layer which is in active tree for main thread and not
  // yet passed onto the impl thread.
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);

  EXPECT_TRUE(
      client_.IsElementInPropertyTrees(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(
      client_.IsElementInPropertyTrees(element_id_, ElementListType::PENDING));

  AttachTimelineAnimationLayer();

  EXPECT_TRUE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  PushProperties();

  GetImplTimelineAndAnimationByID();

  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  // Create the layer in the impl active tree.
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  EXPECT_TRUE(client_impl_.IsElementInPropertyTrees(element_id_,
                                                    ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.IsElementInPropertyTrees(element_id_,
                                                    ElementListType::PENDING));

  // kill layer on main thread.
  client_.UnregisterElementId(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_,
            animation_->keyframe_effect()->element_animations());
  EXPECT_FALSE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  // Sync doesn't detach LayerImpl.
  PushProperties();
  EXPECT_EQ(element_animations_impl_,
            animation_impl_->keyframe_effect()->element_animations());
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  // Kill layer on impl thread in pending tree.
  client_impl_.UnregisterElementId(element_id_, ElementListType::PENDING);
  EXPECT_EQ(element_animations_impl_,
            animation_impl_->keyframe_effect()->element_animations());
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Kill layer on impl thread in active tree.
  client_impl_.UnregisterElementId(element_id_, ElementListType::ACTIVE);
  EXPECT_EQ(element_animations_impl_,
            animation_impl_->keyframe_effect()->element_animations());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Sync doesn't change anything.
  PushProperties();
  EXPECT_EQ(element_animations_impl_,
            animation_impl_->keyframe_effect()->element_animations());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  animation_->DetachElement();
  EXPECT_FALSE(animation_->keyframe_effect()->element_animations());

  // Release ptrs now to test the order of destruction.
  ReleaseRefPtrs();
}

TEST_F(ElementAnimationsTest, AttachToNotYetCreatedLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);

  PushProperties();
  GetImplTimelineAndAnimationByID();

  // Perform attachment separately.
  animation_->AttachElement(element_id_);
  element_animations_ = animation_->keyframe_effect()->element_animations();

  EXPECT_FALSE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  PushProperties();
  element_animations_impl_ =
      animation_impl_->keyframe_effect()->element_animations();

  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_pending_list());

  // Create layer.
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_->has_element_in_active_list());
  EXPECT_FALSE(element_animations_->has_element_in_pending_list());

  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());

  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(element_animations_impl_->has_element_in_active_list());
  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());
}

TEST_F(ElementAnimationsTest, AddRemoveAnimations) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);

  scoped_refptr<ElementAnimations> element_animations =
      animation_->keyframe_effect()->element_animations();
  EXPECT_TRUE(element_animations);

  scoped_refptr<Animation> animation1 =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  scoped_refptr<Animation> animation2 =
      Animation::Create(AnimationIdProvider::NextAnimationId());

  timeline_->AttachAnimation(animation1);
  timeline_->AttachAnimation(animation2);

  // Attach animations to the same layer.
  animation1->AttachElement(element_id_);
  animation2->AttachElement(element_id_);

  EXPECT_EQ(element_animations,
            animation1->keyframe_effect()->element_animations());
  EXPECT_EQ(element_animations,
            animation2->keyframe_effect()->element_animations());

  PushProperties();
  GetImplTimelineAndAnimationByID();

  scoped_refptr<ElementAnimations> element_animations_impl =
      animation_impl_->keyframe_effect()->element_animations();
  EXPECT_TRUE(element_animations_impl);

  EXPECT_EQ(3u, element_animations_impl_->CountKeyframesForTesting());

  animation2->DetachElement();
  EXPECT_FALSE(animation2->keyframe_effect()->element_animations());
  EXPECT_EQ(element_animations,
            animation_->keyframe_effect()->element_animations());
  EXPECT_EQ(element_animations,
            animation1->keyframe_effect()->element_animations());

  PushProperties();
  EXPECT_EQ(element_animations_impl,
            animation_impl_->keyframe_effect()->element_animations());

  EXPECT_EQ(2u, element_animations_impl_->CountKeyframesForTesting());
}

TEST_F(ElementAnimationsTest, SyncNewAnimation) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  EXPECT_FALSE(animation_->GetKeyframeModel(TargetProperty::OPACITY));
  EXPECT_FALSE(animation_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0, 1, false);
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));

  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
}

TEST_F(ElementAnimationsTest,
       SyncScrollOffsetAnimationRespectsHasSetInitialValue) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  EXPECT_FALSE(animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));
  EXPECT_FALSE(
      animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset provider_initial_value(150.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);

  client_impl_.SetScrollOffsetForAnimation(provider_initial_value);

  // Animation with initial value set.
  std::unique_ptr<ScrollOffsetAnimationCurve> curve_fixed(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve_fixed->SetInitialValue(initial_value);
  const int animation1_id = 1;
  std::unique_ptr<KeyframeModel> animation_fixed(KeyframeModel::Create(
      std::move(curve_fixed), animation1_id, 0, TargetProperty::SCROLL_OFFSET));
  animation_->AddKeyframeModel(std::move(animation_fixed));
  PushProperties();
  EXPECT_VECTOR2DF_EQ(initial_value, animation_impl_->keyframe_effect()
                                         ->GetKeyframeModelById(animation1_id)
                                         ->curve()
                                         ->ToScrollOffsetAnimationCurve()
                                         ->GetValue(base::TimeDelta()));
  animation_->RemoveKeyframeModel(animation1_id);

  // Animation without initial value set.
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  const int animation2_id = 2;
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), animation2_id, 1, TargetProperty::SCROLL_OFFSET));
  animation_->AddKeyframeModel(std::move(keyframe_model));
  PushProperties();
  EXPECT_VECTOR2DF_EQ(provider_initial_value,
                      animation_impl_->keyframe_effect()
                          ->GetKeyframeModelById(animation2_id)
                          ->curve()
                          ->ToScrollOffsetAnimationCurve()
                          ->GetValue(base::TimeDelta()));
  animation_->RemoveKeyframeModel(animation2_id);
}

class TestAnimationDelegateThatDestroysAnimation
    : public TestAnimationDelegate {
 public:
  TestAnimationDelegateThatDestroysAnimation() = default;

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {
    TestAnimationDelegate::NotifyAnimationStarted(monotonic_time,
                                                  target_property, group);
    // Detaching animation from the timeline ensures that the timeline doesn't
    // hold a reference to the animation and the animation is destroyed.
    timeline_->DetachAnimation(animation_);
  }

  void setTimelineAndAnimation(scoped_refptr<AnimationTimeline> timeline,
                               scoped_refptr<Animation> animation) {
    timeline_ = timeline;
    animation_ = animation;
  }

 private:
  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<Animation> animation_;
};

// Test that we don't crash if a animation is deleted while ElementAnimations is
// iterating through the list of animations (see crbug.com/631052). This test
// passes if it doesn't crash.
TEST_F(ElementAnimationsTest, AddedAnimationIsDestroyed) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  TestAnimationDelegateThatDestroysAnimation delegate;

  const int animation2_id = AnimationIdProvider::NextAnimationId();
  scoped_refptr<Animation> animation2 = Animation::Create(animation2_id);
  delegate.setTimelineAndAnimation(timeline_, animation2);

  timeline_->AttachAnimation(animation2);
  animation2->AttachElement(element_id_);
  animation2->set_animation_delegate(&delegate);

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation2.get(), 1.0, 0.f, 1.f, false);

  PushProperties();

  scoped_refptr<Animation> animation2_impl =
      timeline_impl_->GetAnimationById(animation2_id);
  DCHECK(animation2_impl);

  animation2_impl->ActivateKeyframeModels();
  EXPECT_TRUE(animation2_impl->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));

  animation2_impl->Tick(kInitialTickTime);

  auto events = CreateEventsForTesting();
  animation2_impl->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // The actual detachment happens here, inside the callback
  animation2->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_TRUE(delegate.started());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST_F(ElementAnimationsTest, DoNotClobberStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  EXPECT_FALSE(animation_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0, 1, false);

  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  auto events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_EQ(animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_impl_->UpdateState(true, nullptr);
  EXPECT_EQ(animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());
}

TEST_F(ElementAnimationsTest, UseSpecifiedStartTimes) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  animation_->GetKeyframeModel(TargetProperty::OPACITY)
      ->set_start_time(start_time);

  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  auto events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  EXPECT_EQ(start_time, animation_->keyframe_effect()
                            ->GetKeyframeModelById(keyframe_model_id)
                            ->start_time());
  EXPECT_EQ(animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());

  // Start the animation on the main thread. Should not affect the start time.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, nullptr);
  EXPECT_EQ(start_time, animation_->keyframe_effect()
                            ->GetKeyframeModelById(keyframe_model_id)
                            ->start_time());
  EXPECT_EQ(animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time(),
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->start_time());
}

// Tests that animationss activate and deactivate as expected.
TEST_F(ElementAnimationsTest, Activation) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  AnimationHost* host = client_.host();
  AnimationHost* host_impl = client_impl_.host();

  auto events = CreateEventsForTesting();

  EXPECT_EQ(1u, host->element_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->element_animations_for_testing().size());

  // Initially, both animationss should be inactive.
  EXPECT_EQ(0u, host->ticking_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl->ticking_animations_for_testing().size());

  AddOpacityTransitionToAnimation(animation_.get(), 1, 0, 1, false);
  // The main thread animations should now be active.
  EXPECT_EQ(1u, host->ticking_animations_for_testing().size());

  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  // Both animationss should now be active.
  EXPECT_EQ(1u, host->ticking_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->ticking_animations_for_testing().size());

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  EXPECT_EQ(1u, host->ticking_animations_for_testing().size());
  EXPECT_EQ(1u, host_impl->ticking_animations_for_testing().size());

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, nullptr);
  EXPECT_EQ(1u, host->ticking_animations_for_testing().size());

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);
  EXPECT_EQ(KeyframeModel::FINISHED,
            animation_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(1u, host->ticking_animations_for_testing().size());

  events = CreateEventsForTesting();

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  animation_impl_->UpdateState(true, events.get());

  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_DELETION,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  // The impl thread animations should have de-activated.
  EXPECT_EQ(0u, host_impl->ticking_animations_for_testing().size());

  EXPECT_EQ(1u, events->events_.size());
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  animation_->UpdateState(true, nullptr);

  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            animation_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  // The main thread animations should have de-activated.
  EXPECT_EQ(0u, host->ticking_animations_for_testing().size());

  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(animation_->keyframe_effect()->has_any_keyframe_model());
  EXPECT_FALSE(animation_impl_->keyframe_effect()->has_any_keyframe_model());
  EXPECT_EQ(0u, host->ticking_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl->ticking_animations_for_testing().size());
}

TEST_F(ElementAnimationsTest, SyncPause) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  EXPECT_FALSE(animation_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  // Two steps, three ranges: [0-1) -> 0.2, [1-2) -> 0.3, [2-3] -> 0.4.
  const double duration = 3.0;
  const int keyframe_model_id =
      AddOpacityStepsToAnimation(animation_.get(), duration, 0.2f, 0.4f, 2);

  // Set start offset to be at the beginning of the second range.
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(TimeDelta::FromSecondsD(1.01));

  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  TimeTicks time = kInitialTickTime;

  // Start the animations on each animations.
  auto events = CreateEventsForTesting();
  animation_impl_->Tick(time);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());

  animation_->Tick(time);
  animation_->UpdateState(true, nullptr);
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  EXPECT_EQ(0.3f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.3f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_EQ(kInitialTickTime, animation_->keyframe_effect()
                                  ->GetKeyframeModelById(keyframe_model_id)
                                  ->start_time());
  EXPECT_EQ(kInitialTickTime, animation_impl_->keyframe_effect()
                                  ->GetKeyframeModelById(keyframe_model_id)
                                  ->start_time());

  // Pause the animation at the middle of the second range so the offset
  // delays animation until the middle of the third range.
  animation_->PauseKeyframeModel(keyframe_model_id,
                                 base::TimeDelta::FromMilliseconds(1500));
  EXPECT_EQ(KeyframeModel::PAUSED, animation_->keyframe_effect()
                                       ->GetKeyframeModelById(keyframe_model_id)
                                       ->run_state());

  // The pause run state change should make it to the impl thread animations.
  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  // Advance time so it stays within the first range.
  time += TimeDelta::FromMilliseconds(10);
  animation_->Tick(time);
  animation_impl_->Tick(time);

  EXPECT_EQ(KeyframeModel::PAUSED, animation_impl_->keyframe_effect()
                                       ->GetKeyframeModelById(keyframe_model_id)
                                       ->run_state());

  // Opacity value doesn't depend on time if paused at specified time offset.
  EXPECT_EQ(0.4f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(0.4f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, DoNotSyncFinishedAnimation) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(animation_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0, 1, false);

  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);

  // Notify main thread animations that the animation has started.
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  // Complete animation on impl thread.
  events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromSeconds(1));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  animation_->Tick(kInitialTickTime + TimeDelta::FromSeconds(2));
  animation_->UpdateState(true, nullptr);

  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
}

// Ensure that a finished animation is eventually deleted by both the
// main-thread and the impl-thread animationss.
TEST_F(ElementAnimationsTest, AnimationsAreDeleted) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  AddOpacityTransitionToAnimation(animation_.get(), 1.0, 0.0f, 1.0f, false);
  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());

  PushProperties();
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(host_impl_->needs_push_properties());

  animation_impl_->ActivateKeyframeModels();

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_impl_->UpdateState(true, events.get());

  // There should be a STARTED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);

  EXPECT_FALSE(host_->needs_push_properties());
  EXPECT_FALSE(host_impl_->needs_push_properties());

  events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(host_impl_->needs_push_properties());

  // There should be a FINISHED event for the animation.
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);

  // Neither animations should have deleted the animation yet.
  EXPECT_TRUE(animation_->GetKeyframeModel(TargetProperty::OPACITY));
  EXPECT_TRUE(animation_impl_->GetKeyframeModel(TargetProperty::OPACITY));

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();

  // Both animationss should now have deleted the animation. The impl animations
  // should have deleted the animation even though activation has not occurred,
  // since the animation was already waiting for deletion when
  // PushPropertiesTo was called.
  EXPECT_FALSE(animation_->keyframe_effect()->has_any_keyframe_model());
  EXPECT_FALSE(animation_impl_->keyframe_effect()->has_any_keyframe_model());
}

// Tests that transitioning opacity from 0 to 1 works as expected.

static std::unique_ptr<KeyframeModel> CreateKeyframeModel(
    std::unique_ptr<AnimationCurve> curve,
    int group_id,
    TargetProperty::Type property) {
  return KeyframeModel::Create(std::move(curve), 0, group_id, property);
}

TEST_F(ElementAnimationsTest, TrivialTransition) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  int keyframe_model_id = to_add->id();

  EXPECT_FALSE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  animation_->AddKeyframeModel(std::move(to_add));
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  animation_->Tick(kInitialTickTime);
  EXPECT_EQ(KeyframeModel::STARTING,
            animation_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, FilterTransition) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateBrightnessFilter(1.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                            end_filters, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(
      KeyframeModel::Create(std::move(curve), 1, 0, TargetProperty::FILTER));
  animation_->AddKeyframeModel(std::move(keyframe_model));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(start_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(1u,
            client_.GetFilters(element_id_, ElementListType::ACTIVE).size());
  EXPECT_EQ(FilterOperation::CreateBrightnessFilter(1.5f),
            client_.GetFilters(element_id_, ElementListType::ACTIVE).at(0));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(end_filters,
            client_.GetFilters(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, BackdropFilterTransition) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  FilterOperations start_filters;
  start_filters.Append(FilterOperation::CreateInvertFilter(0.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  FilterOperations end_filters;
  end_filters.Append(FilterOperation::CreateInvertFilter(1.f));
  curve->AddKeyframe(FilterKeyframe::Create(base::TimeDelta::FromSecondsD(1.0),
                                            end_filters, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::BACKDROP_FILTER));
  animation_->AddKeyframeModel(std::move(keyframe_model));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(start_filters,
            client_.GetBackdropFilters(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(
      1u,
      client_.GetBackdropFilters(element_id_, ElementListType::ACTIVE).size());
  EXPECT_EQ(
      FilterOperation::CreateInvertFilter(0.5f),
      client_.GetBackdropFilters(element_id_, ElementListType::ACTIVE).at(0));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(end_filters,
            client_.GetBackdropFilters(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransition) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_needs_synchronized_start_time(true);
  animation_->AddKeyframeModel(std::move(keyframe_model));

  client_impl_.SetScrollOffsetForAnimation(initial_value);
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration =
      animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
          ->curve()
          ->Duration();
  EXPECT_EQ(duration,
            animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->curve()
                ->Duration());

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_impl_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  animation_->Tick(kInitialTickTime + duration / 2);
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + duration / 2);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + duration);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->HasTickingKeyframeModel());

  animation_->Tick(kInitialTickTime + duration);
  animation_->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, ScrollOffsetTransitionOnImplOnly) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve->SetInitialValue(initial_value);
  double duration_in_seconds = curve->Duration().InSecondsF();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->SetIsImplOnly();
  animation_impl_->AddKeyframeModel(std::move(keyframe_model));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_impl_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  TimeDelta duration = TimeDelta::FromMicroseconds(
      duration_in_seconds * base::Time::kMicrosecondsPerSecond);

  animation_impl_->Tick(kInitialTickTime + duration / 2);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(200.f, 250.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + duration);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->HasTickingKeyframeModel());
}

// This test verifies that if an animation is added after a layer is animated,
// it doesn't get promoted to be in the RUNNING state. This prevents cases where
// a start time gets set on an animation using the stale value of
// last_tick_time_.
TEST_F(ElementAnimationsTest, UpdateStateWithoutAnimate) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  // Add first scroll offset animation.
  AddScrollOffsetAnimationToAnimation(animation_impl_.get(),
                                      gfx::ScrollOffset(100.f, 300.f),
                                      gfx::ScrollOffset(100.f, 200.f));

  // Calling UpdateState after Animate should promote the animation to running
  // state.
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(nullptr,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));

  // Add second scroll offset animation.
  AddScrollOffsetAnimationToAnimation(animation_impl_.get(),
                                      gfx::ScrollOffset(100.f, 200.f),
                                      gfx::ScrollOffset(100.f, 100.f));

  // Calling UpdateState without Animate should NOT promote the animation to
  // running state.
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());
  EXPECT_VECTOR2DF_EQ(
      gfx::ScrollOffset(100.f, 200.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
}

// Ensure that when the impl animations doesn't have a value provider,
// the main-thread animations's value provider is used to obtain the intial
// scroll offset.
TEST_F(ElementAnimationsTest, ScrollOffsetTransitionNoImplProvider) {
  CreateTestLayer(false, false);
  CreateTestImplLayer(ElementListType::PENDING);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  EXPECT_TRUE(element_animations_impl_->has_element_in_pending_list());
  EXPECT_FALSE(element_animations_impl_->has_element_in_active_list());

  auto events = CreateEventsForTesting();

  gfx::ScrollOffset initial_value(500.f, 100.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_needs_synchronized_start_time(true);
  animation_->AddKeyframeModel(std::move(keyframe_model));

  client_.SetScrollOffsetForAnimation(initial_value);
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));
  TimeDelta duration =
      animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
          ->curve()
          ->Duration();
  EXPECT_EQ(duration,
            animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->curve()
                ->Duration());

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, nullptr);

  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value,
            client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));
  EXPECT_EQ(gfx::ScrollOffset(), client_impl_.GetScrollOffset(
                                     element_id_, ElementListType::PENDING));

  animation_impl_->Tick(kInitialTickTime);

  EXPECT_TRUE(animation_impl_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(initial_value, client_impl_.GetScrollOffset(
                               element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);

  animation_impl_->UpdateState(true, events.get());
  DCHECK_EQ(1UL, events->events_.size());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  animation_->Tick(kInitialTickTime + duration / 2);
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_.GetScrollOffset(element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + duration / 2);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(400.f, 150.f),
      client_impl_.GetScrollOffset(element_id_, ElementListType::PENDING));

  animation_impl_->Tick(kInitialTickTime + duration);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_VECTOR2DF_EQ(target_value, client_impl_.GetScrollOffset(
                                        element_id_, ElementListType::PENDING));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->HasTickingKeyframeModel());

  animation_->Tick(kInitialTickTime + duration);
  animation_->UpdateState(true, nullptr);
  EXPECT_VECTOR2DF_EQ(target_value, client_.GetScrollOffset(
                                        element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

TEST_F(ElementAnimationsTest, ScrollOffsetRemovalClearsScrollDelta) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  // First test the 1-argument version of RemoveKeyframeModel.
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));

  int keyframe_model_id = 1;
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), keyframe_model_id, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_needs_synchronized_start_time(true);
  animation_->AddKeyframeModel(std::move(keyframe_model));
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_TRUE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  // Now, test the 2-argument version of RemoveKeyframeModel.
  curve = ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
      target_value);
  keyframe_model = KeyframeModel::Create(std::move(curve), keyframe_model_id, 0,
                                         TargetProperty::SCROLL_OFFSET);
  keyframe_model->set_needs_synchronized_start_time(true);
  animation_->AddKeyframeModel(std::move(keyframe_model));
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_TRUE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  // Check that removing non-scroll-offset animations does not cause
  // scroll_offset_animation_was_interrupted() to get set.
  keyframe_model_id =
      AddAnimatedTransformToAnimation(animation_.get(), 1.0, 1, 2);
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  keyframe_model_id =
      AddAnimatedFilterToAnimation(animation_.get(), 1.0, 0.1f, 0.2f);
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  PushProperties();
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());
  EXPECT_FALSE(
      animation_->keyframe_effect()->scroll_offset_animation_was_interrupted());

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->scroll_offset_animation_was_interrupted());
}

// Tests that impl-only animations lead to start and finished notifications
// on the impl thread animations's animation delegate.
TEST_F(ElementAnimationsTest,
       NotificationsForImplOnlyAnimationsAreSentToImplThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  TestAnimationDelegate delegate;
  animation_impl_->set_animation_delegate(&delegate);

  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve->SetInitialValue(initial_value);
  TimeDelta duration = curve->Duration();
  std::unique_ptr<KeyframeModel> to_add(KeyframeModel::Create(
      std::move(curve), 1, 0, TargetProperty::SCROLL_OFFSET));
  to_add->SetIsImplOnly();
  animation_impl_->AddKeyframeModel(std::move(to_add));

  EXPECT_FALSE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_FALSE(delegate.finished());

  events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime + duration);
  EXPECT_EQ(duration,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->curve()
                ->Duration());
  animation_impl_->UpdateState(true, events.get());

  EXPECT_TRUE(delegate.started());
  EXPECT_TRUE(delegate.finished());
}

// Tests that specified start times are sent to the main thread delegate
TEST_F(ElementAnimationsTest, SpecifiedStartTimesAreSentToMainThreadDelegate) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  TestAnimationDelegate delegate;
  animation_->set_animation_delegate(&delegate);

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0, 1, false);

  const TimeTicks start_time = TicksFromSecondsF(123);
  animation_->GetKeyframeModel(TargetProperty::OPACITY)
      ->set_start_time(start_time);

  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  auto events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  // Synchronize the start times.
  EXPECT_EQ(1u, events->events_.size());
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);

  // Validate start time on the main thread delegate.
  EXPECT_EQ(start_time, delegate.start_time());
}

// Tests animations that are waiting for a synchronized start time do not
// finish.
TEST_F(ElementAnimationsTest,
       AnimationsWaitingForStartTimeDoNotFinishIfTheyOutwaitTheirFinish) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);
  int keyframe_model_id = to_add->id();

  // We should pause at the first keyframe indefinitely waiting for that
  // animation to start.
  animation_->AddKeyframeModel(std::move(to_add));
  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Send the synchronized start time.
  animation_->DispatchAndDelegateAnimationEvent(
      AnimationEvent(AnimationEvent::STARTED,
                     {animation_->animation_timeline()->id(), animation_->id(),
                      keyframe_model_id},
                     1, TargetProperty::OPACITY,
                     kInitialTickTime + TimeDelta::FromMilliseconds(2000)));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(5000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST_F(ElementAnimationsTest, TrivialQueuing) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  int animation1_id = 1;
  int animation2_id = 2;
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      animation1_id, 1, TargetProperty::OPACITY));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      animation2_id, 2, TargetProperty::OPACITY));

  animation_->Tick(kInitialTickTime);

  // The first animation should have been started.
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(animation1_id));
  EXPECT_EQ(KeyframeModel::STARTING, animation_->keyframe_effect()
                                         ->GetKeyframeModelById(animation1_id)
                                         ->run_state());

  // The second animation still needs to be started.
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(animation2_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_->keyframe_effect()
                ->GetKeyframeModelById(animation2_id)
                ->run_state());

  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());

  // Now the first should be finished, and the second started.
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(animation1_id));
  EXPECT_EQ(KeyframeModel::FINISHED, animation_->keyframe_effect()
                                         ->GetKeyframeModelById(animation1_id)
                                         ->run_state());
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(animation2_id));
  EXPECT_EQ(KeyframeModel::RUNNING, animation_->keyframe_effect()
                                        ->GetKeyframeModelById(animation2_id)
                                        ->run_state());

  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests interrupting a transition with another transition.
TEST_F(ElementAnimationsTest, Interrupt) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  animation_->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY, false);
  animation_->AddKeyframeModel(std::move(to_add));

  // Since the previous animation was aborted, the new animation should start
  // right in this call to animate.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1500));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests scheduling two animations to run together when only one property is
// free.
TEST_F(ElementAnimationsTest, ScheduleTogetherWhenAPropertyIsBlocked) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 2,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The float animation should have started at time 1 and should be done.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests scheduling two animations to run together with different lengths and
// another animation queued to start when the shorter animation finishes (should
// wait for both to finish).
TEST_F(ElementAnimationsTest, ScheduleTogetherWithAnAnimWaiting) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2)), 1,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));

  // Animations with id 1 should both start now.
  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  // The opacity animation should have finished at time 1, but the group
  // of animations with id 1 don't finish until time 2 because of the length
  // of the transform animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_->UpdateState(true, events.get());
  // Should not have started the float transition yet.
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The second opacity animation should start at time 2 and should be done by
  // time 3.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST_F(ElementAnimationsTest, TrivialLooping) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(3);
  animation_->AddKeyframeModel(std::move(to_add));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1250));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1750));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2250));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2750));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animation_->UpdateState(true, events.get());
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Just be extra sure.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animation_->UpdateState(true, events.get());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST_F(ElementAnimationsTest, InfiniteLooping) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  to_add->set_iterations(std::numeric_limits<double>::infinity());
  animation_->AddKeyframeModel(std::move(to_add));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1250));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1750));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime +
                   TimeDelta::FromMilliseconds(1073741824250));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.25f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime +
                   TimeDelta::FromMilliseconds(1073741824750));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animation_->GetKeyframeModel(TargetProperty::OPACITY));
  animation_->GetKeyframeModel(TargetProperty::OPACITY)
      ->SetRunState(KeyframeModel::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(750));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Test that pausing and resuming work as expected.
TEST_F(ElementAnimationsTest, PauseResume) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animation_->GetKeyframeModel(TargetProperty::OPACITY));
  animation_->GetKeyframeModel(TargetProperty::OPACITY)
      ->SetRunState(KeyframeModel::PAUSED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(500));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animation_->GetKeyframeModel(TargetProperty::OPACITY));
  animation_->GetKeyframeModel(TargetProperty::OPACITY)
      ->SetRunState(KeyframeModel::RUNNING,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1024000));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1024250));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1024500));
  animation_->UpdateState(true, events.get());
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, AbortAGroupedAnimation) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  const int keyframe_model_id = 2;
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1, 1,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      keyframe_model_id, 1, TargetProperty::OPACITY));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.75f)),
      3, 2, TargetProperty::OPACITY));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.5f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->SetRunState(KeyframeModel::ABORTED,
                    kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(!animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.75f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushUpdatesWhenSynchronizedStartTimeNeeded) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> to_add(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      0, TargetProperty::OPACITY));
  to_add->set_needs_synchronized_start_time(true);
  animation_->AddKeyframeModel(std::move(to_add));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  KeyframeModel* active_keyframe_model =
      animation_->GetKeyframeModel(TargetProperty::OPACITY);
  EXPECT_TRUE(active_keyframe_model);
  EXPECT_TRUE(active_keyframe_model->needs_synchronized_start_time());

  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());
  PushProperties();
  animation_impl_->ActivateKeyframeModels();

  active_keyframe_model =
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY);
  EXPECT_TRUE(active_keyframe_model);
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            active_keyframe_model->run_state());
}

// Tests that skipping a call to UpdateState works as expected.
TEST_F(ElementAnimationsTest, SkipUpdateState) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();

  auto events = CreateEventsForTesting();

  std::unique_ptr<KeyframeModel> first_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  first_keyframe_model->set_is_controlling_instance_for_test(true);
  animation_->AddKeyframeModel(std::move(first_keyframe_model));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, events.get());

  std::unique_ptr<KeyframeModel> second_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, TargetProperty::OPACITY));
  second_keyframe_model->set_is_controlling_instance_for_test(true);
  animation_->AddKeyframeModel(std::move(second_keyframe_model));

  // Animate but don't UpdateState.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  events = CreateEventsForTesting();
  animation_->UpdateState(true, events.get());

  // Should have one STARTED event and one FINISHED event.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_NE(events->events_[0].type, events->events_[1].type);

  // The float transition should still be at its starting point.
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animation_->UpdateState(true, events.get());

  // The float tranisition should now be done.
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->HasTickingKeyframeModel());
}

// Tests that an animation animations with only a pending observer gets ticked
// but doesn't progress animations past the STARTING state.
TEST_F(ElementAnimationsTest, InactiveObserverGetsTicked) {
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  const int id = 1;
  animation_impl_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.5f, 1.f)),
      id, TargetProperty::OPACITY));
  animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)
      ->set_affects_active_elements(false);

  // Without an observer, the animation shouldn't progress to the STARTING
  // state.
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  CreateTestImplLayer(ElementListType::PENDING);

  // With only a pending observer, the animation should progress to the
  // STARTING state and get ticked at its starting point, but should not
  // progress to RUNNING.
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::STARTING,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  // Even when already in the STARTING state, the animation should stay
  // there, and shouldn't be ticked past its starting point.
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::STARTING,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));

  CreateTestImplLayer(ElementListType::ACTIVE);
  animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)
      ->set_affects_active_elements(true);

  // Now that an active observer has been added, the animation should still
  // initially tick at its starting point, but should now progress to RUNNING.
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // The animation should now tick past its starting point.
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(3500));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_NE(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

// Tests that AbortKeyframeModelsWithProperty aborts all animations targeting
// the specified property.
TEST_F(ElementAnimationsTest, AbortKeyframeModelsWithProperty) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  // Start with several animations, and allow some of them to reach the finished
  // state.
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1, 1,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, 2, TargetProperty::OPACITY));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 3, 3,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 4, 4,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      5, 5, TargetProperty::OPACITY));

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, nullptr);
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);

  EXPECT_EQ(
      KeyframeModel::FINISHED,
      animation_->keyframe_effect()->GetKeyframeModelById(1)->run_state());
  EXPECT_EQ(
      KeyframeModel::FINISHED,
      animation_->keyframe_effect()->GetKeyframeModelById(2)->run_state());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      animation_->keyframe_effect()->GetKeyframeModelById(3)->run_state());
  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
      animation_->keyframe_effect()->GetKeyframeModelById(4)->run_state());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      animation_->keyframe_effect()->GetKeyframeModelById(5)->run_state());

  animation_->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM, false);

  // Only un-finished TRANSFORM animations should have been aborted.
  EXPECT_EQ(
      KeyframeModel::FINISHED,
      animation_->keyframe_effect()->GetKeyframeModelById(1)->run_state());
  EXPECT_EQ(
      KeyframeModel::FINISHED,
      animation_->keyframe_effect()->GetKeyframeModelById(2)->run_state());
  EXPECT_EQ(
      KeyframeModel::ABORTED,
      animation_->keyframe_effect()->GetKeyframeModelById(3)->run_state());
  EXPECT_EQ(
      KeyframeModel::ABORTED,
      animation_->keyframe_effect()->GetKeyframeModelById(4)->run_state());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      animation_->keyframe_effect()->GetKeyframeModelById(5)->run_state());
}

// An animation aborted on the main thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, MainThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1.0, 0.f, 1.f, false);
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();

  animation_impl_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_FALSE(host_->needs_push_properties());

  animation_->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY, false);
  EXPECT_EQ(KeyframeModel::ABORTED,
            animation_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(host_->needs_push_properties());

  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, nullptr);
  EXPECT_EQ(KeyframeModel::ABORTED,
            animation_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());
  EXPECT_TRUE(host_->needs_push_properties());

  PushProperties();
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());
  EXPECT_FALSE(host_->needs_push_properties());

  EXPECT_FALSE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
}

// An animation aborted on the impl thread should get deleted on both threads.
TEST_F(ElementAnimationsTest, ImplThreadAbortedAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  TestAnimationDelegate delegate;
  animation_->set_animation_delegate(&delegate);

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1.0, 0.f, 1.f, false);

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));

  animation_impl_->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY,
                                                   false);
  EXPECT_EQ(
      KeyframeModel::ABORTED,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_TRUE(animation_impl_->keyframe_effect()->needs_push_properties());

  auto events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[0].type);
  EXPECT_EQ(
      KeyframeModel::WAITING_FOR_DELETION,
      animation_impl_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_EQ(KeyframeModel::ABORTED,
            animation_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());
  EXPECT_TRUE(delegate.aborted());

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(host_->needs_push_properties());
  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            animation_->GetKeyframeModel(TargetProperty::OPACITY)->run_state());

  PushProperties();

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
}

// Test that an impl-only scroll offset animation that needs to be completed on
// the main thread gets deleted.
TEST_F(ElementAnimationsTest, ImplThreadTakeoverAnimationGetsDeleted) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  TestAnimationDelegate delegate_impl;
  animation_impl_->set_animation_delegate(&delegate_impl);
  TestAnimationDelegate delegate;
  animation_->set_animation_delegate(&delegate);

  // Add impl-only scroll offset animation.
  const int keyframe_model_id = 1;
  gfx::ScrollOffset initial_value(100.f, 300.f);
  gfx::ScrollOffset target_value(300.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve->SetInitialValue(initial_value);
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), keyframe_model_id, 0, TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_start_time(TicksFromSecondsF(123));
  keyframe_model->SetIsImplOnly();
  animation_impl_->AddKeyframeModel(std::move(keyframe_model));

  PushProperties();
  EXPECT_FALSE(host_->needs_push_properties());

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));

  animation_impl_->AbortKeyframeModelsWithProperty(
      TargetProperty::SCROLL_OFFSET, true);
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(KeyframeModel::ABORTED_BUT_NEEDS_COMPLETION,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
                ->run_state());

  auto events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_TRUE(delegate_impl.finished());
  EXPECT_TRUE(host_impl_->needs_push_properties());
  EXPECT_EQ(1u, events->events_.size());
  EXPECT_EQ(AnimationEvent::TAKEOVER, events->events_[0].type);
  EXPECT_EQ(TicksFromSecondsF(123), events->events_[0].animation_start_time);
  EXPECT_EQ(
      target_value,
      events->events_[0].curve->ToScrollOffsetAnimationCurve()->target_value());
  EXPECT_EQ(nullptr,
            animation_impl_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET));

  // MT receives the event to take over.
  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_TRUE(delegate.takeover());

  // Animation::NotifyAnimationTakeover requests SetNeedsPushProperties to purge
  // CT animations marked for deletion.
  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());

  // ElementAnimations::PurgeAnimationsMarkedForDeletion call happens only in
  // ElementAnimations::PushPropertiesTo.
  PushProperties();

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
}

// Ensure that we only generate FINISHED events for animations in a group
// once all animations in that group are finished.
TEST_F(ElementAnimationsTest, FinishedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  const int group_id = 1;

  // Add two animations with the same group id but different durations.
  std::unique_ptr<KeyframeModel> first_keyframe_model(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(2.0)), 1,
      group_id, TargetProperty::TRANSFORM));
  first_keyframe_model->set_is_controlling_instance_for_test(true);
  animation_impl_->AddKeyframeModel(std::move(first_keyframe_model));

  std::unique_ptr<KeyframeModel> second_keyframe_model(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      2, group_id, TargetProperty::OPACITY));
  second_keyframe_model->set_is_controlling_instance_for_test(true);
  animation_impl_->AddKeyframeModel(std::move(second_keyframe_model));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());

  // The opacity animation should be finished, but should not have generated
  // a FINISHED event yet.
  EXPECT_EQ(0u, events->events_.size());
  EXPECT_EQ(
      KeyframeModel::FINISHED,
      animation_impl_->keyframe_effect()->GetKeyframeModelById(2)->run_state());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      animation_impl_->keyframe_effect()->GetKeyframeModelById(1)->run_state());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  // Both animations should have generated FINISHED events.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[1].type);
}

// Ensure that when a group has a mix of aborted and finished animations,
// we generate a FINISHED event for the finished animation and an ABORTED
// event for the aborted animation.
TEST_F(ElementAnimationsTest, FinishedAndAbortedEventsForGroup) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  // Add two animations with the same group id.
  std::unique_ptr<KeyframeModel> first_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1,
      TargetProperty::TRANSFORM));
  first_keyframe_model->set_is_controlling_instance_for_test(true);
  animation_impl_->AddKeyframeModel(std::move(first_keyframe_model));

  std::unique_ptr<KeyframeModel> second_keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  second_keyframe_model->set_is_controlling_instance_for_test(true);
  animation_impl_->AddKeyframeModel(std::move(second_keyframe_model));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  // Both animations should have started.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[0].type);
  EXPECT_EQ(AnimationEvent::STARTED, events->events_[1].type);

  animation_impl_->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY,
                                                   false);

  events = CreateEventsForTesting();
  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());

  // We should have exactly 2 events: a FINISHED event for the tranform
  // animation, and an ABORTED event for the opacity animation.
  EXPECT_EQ(2u, events->events_.size());
  EXPECT_EQ(AnimationEvent::FINISHED, events->events_[0].type);
  EXPECT_EQ(TargetProperty::TRANSFORM, events->events_[0].target_property);
  EXPECT_EQ(AnimationEvent::ABORTED, events->events_[1].type);
  EXPECT_EQ(TargetProperty::OPACITY, events->events_[1].target_property);
}

TEST_F(ElementAnimationsTest, GetAnimationScalesNotScaled) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  float max_scale = 999;
  float start_scale = 999;
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);

  animation_impl_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));

  // Opacity animations aren't non-translation transforms.
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  operations1.AppendTranslate(10.0, 15.0, 0.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve1), 2, 2, TargetProperty::TRANSFORM));
  animation_impl_->AddKeyframeModel(std::move(keyframe_model));

  // The only transform animation we've added is a translation.
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);
}

TEST_F(ElementAnimationsTest, GetAnimationScales) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1a;
  operations1a.AppendScale(2.0, 3.0, 4.0);
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1a, nullptr));
  TransformOperations operations1b;
  operations1b.AppendScale(5.0, 4.0, 3.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations1b, nullptr));
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  keyframe_model->set_affects_active_elements(false);
  animation_impl_->AddKeyframeModel(std::move(keyframe_model));

  float max_scale = kNotScaled;
  float start_scale = kNotScaled;
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(5.f, max_scale);
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(5.f, max_scale);
  EXPECT_EQ(4.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(5.f, max_scale);
  EXPECT_EQ(4.f, start_scale);

  std::unique_ptr<KeyframedTransformAnimationCurve> curve2(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations2a;
  operations2a.AppendScale(1.0, 2.0, 3.0);
  curve2->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations2a, nullptr));
  TransformOperations operations2b;
  operations2b.AppendScale(6.0, 5.0, 4.0);
  curve2->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2b, nullptr));

  animation_impl_->RemoveKeyframeModel(1);
  keyframe_model =
      KeyframeModel::Create(std::move(curve2), 2, 2, TargetProperty::TRANSFORM);

  // Reverse Direction
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  keyframe_model->set_affects_active_elements(false);
  animation_impl_->AddKeyframeModel(std::move(keyframe_model));

  std::unique_ptr<KeyframedTransformAnimationCurve> curve3(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations3a;
  operations3a.AppendScale(5.0, 3.0, 1.0);
  curve3->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations3a, nullptr));
  TransformOperations operations3b;
  operations3b.AppendScale(1.5, 2.5, 3.5);
  curve3->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations3b, nullptr));

  keyframe_model =
      KeyframeModel::Create(std::move(curve3), 3, 3, TargetProperty::TRANSFORM);
  keyframe_model->set_affects_active_elements(false);
  animation_impl_->AddKeyframeModel(std::move(keyframe_model));

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.5f, max_scale);
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(kNotScaled, max_scale);
  EXPECT_EQ(kNotScaled, start_scale);

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.5f, max_scale);
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(3.5f, max_scale);
  EXPECT_EQ(6.f, start_scale);

  animation_impl_->keyframe_effect()->GetKeyframeModelById(2)->SetRunState(
      KeyframeModel::FINISHED, TicksFromSecondsF(0.0));

  // Only unfinished animations should be considered by GetAnimationScales.
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.5f, max_scale);
  EXPECT_EQ(5.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(3.5f, max_scale);
  EXPECT_EQ(5.f, start_scale);
}

TEST_F(ElementAnimationsTest, GetAnimationScalesWithDirection) {
  CreateTestLayer(true, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  std::unique_ptr<KeyframedTransformAnimationCurve> curve1(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendScale(1.0, 2.0, 3.0);
  curve1->AddKeyframe(
      TransformKeyframe::Create(base::TimeDelta(), operations1, nullptr));
  TransformOperations operations2;
  operations2.AppendScale(4.0, 5.0, 6.0);
  curve1->AddKeyframe(TransformKeyframe::Create(
      base::TimeDelta::FromSecondsD(1.0), operations2, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model_owned(KeyframeModel::Create(
      std::move(curve1), 1, 1, TargetProperty::TRANSFORM));
  KeyframeModel* keyframe_model = keyframe_model_owned.get();
  animation_impl_->AddKeyframeModel(std::move(keyframe_model_owned));

  float max_scale = 999;
  float start_scale = 999;

  EXPECT_GT(keyframe_model->playback_rate(), 0.0);

  // NORMAL direction with positive playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::NORMAL);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);

  // ALTERNATE direction with positive playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);

  // REVERSE direction with positive playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);

  // ALTERNATE reverse direction.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);

  keyframe_model->set_playback_rate(-1.0);

  // NORMAL direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::NORMAL);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);

  // ALTERNATE direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::ALTERNATE_NORMAL);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(3.f, max_scale);
  EXPECT_EQ(6.f, start_scale);

  // REVERSE direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);

  // ALTERNATE reverse direction with negative playback rate.
  keyframe_model->set_direction(KeyframeModel::Direction::REVERSE);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::PENDING, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetAnimationScales(
      ElementListType::ACTIVE, &max_scale, &start_scale));
  EXPECT_EQ(6.f, max_scale);
  EXPECT_EQ(3.f, start_scale);
}

TEST_F(ElementAnimationsTest, NewlyPushedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0.5f, 1.f, false);
  EXPECT_TRUE(
      animation_->keyframe_effect()->GetKeyframeModelById(keyframe_model_id));
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));

  PushProperties();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(keyframe_model_id)
                   ->affects_active_elements());

  animation_impl_->Tick(kInitialTickTime);
  EXPECT_EQ(KeyframeModel::STARTING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  animation_impl_->UpdateState(true, events.get());

  // Since the animation hasn't been activated, it should still be STARTING
  // rather than RUNNING.
  EXPECT_EQ(KeyframeModel::STARTING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state and the active observer should start to get ticked.
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ActivationBetweenAnimateAndUpdateState) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  const int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0.5f, 1.f, true);

  PushProperties();

  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(
      keyframe_model_id));
  EXPECT_EQ(KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(keyframe_model_id)
                   ->affects_active_elements());

  animation_impl_->Tick(kInitialTickTime);

  // Since the animation hasn't been activated, only the pending observer
  // should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  animation_impl_->UpdateState(true, events.get());

  // Since the animation has been activated, it should have reached the
  // RUNNING state.
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));

  // Both elements should have been ticked.
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenTransformAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedTransformToAnimation(animation_.get(), 1.0, 1, 1);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id =
      AddAnimatedTransformToAnimation(animation_.get(), 10.0, 2, 2);
  int animation2_id =
      AddAnimatedTransformToAnimation(animation_.get(), 10.0, 2, 1);
  animation_->keyframe_effect()
      ->GetKeyframeModelById(animation2_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animation_->keyframe_effect()
      ->GetKeyframeModelById(animation2_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  // animation1 is in effect currently and animation2 isn't. As the element has
  // atleast one animation that's in effect currently, client should be notified
  // that the transform is currently animating.
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_->RemoveKeyframeModel(keyframe_model_id);
  animation_->RemoveKeyframeModel(animation2_id);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id =
      AddAnimatedTransformToAnimation(animation_.get(), 10.0, 3, 3);
  EXPECT_TRUE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_impl_->AbortKeyframeModelsWithProperty(TargetProperty::TRANSFORM,
                                                   false);
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id =
      AddAnimatedTransformToAnimation(animation_.get(), 1.0, 1, 6);
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialTransformAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetTransformIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenOpacityAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddOpacityTransitionToAnimation(animation_.get(), 1.0, 0.f, 1.f,
                                  false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id = AddOpacityTransitionToAnimation(
      animation_.get(), 10.0, 0.f, 1.f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id = AddOpacityTransitionToAnimation(
      animation_.get(), 10.0, 0.f, 0.5f, false /*use_timing_function*/);
  EXPECT_TRUE(client_.GetHasPotentialOpacityAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_impl_->AbortKeyframeModelsWithProperty(TargetProperty::OPACITY,
                                                   false);
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetOpacityIsCurrentlyAnimating(element_id_,
                                                      ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id = AddOpacityTransitionToAnimation(
      animation_.get(), 1.0, 0.f, 0.5f, false /*use_timing_function*/);
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialOpacityAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetOpacityIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ObserverNotifiedWhenFilterAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedFilterToAnimation(animation_.get(), 1.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id =
      AddAnimatedFilterToAnimation(animation_.get(), 10.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id =
      AddAnimatedFilterToAnimation(animation_.get(), 10.0, 0.f, 0.5f);
  EXPECT_TRUE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                     ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                    ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_impl_->AbortKeyframeModelsWithProperty(TargetProperty::FILTER,
                                                   false);
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialFilterAnimation(element_id_,
                                                      ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetFilterIsCurrentlyAnimating(element_id_,
                                                     ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id =
      AddAnimatedFilterToAnimation(animation_.get(), 1.0, 0.f, 0.5f);
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest,
       ObserverNotifiedWhenBackdropFilterAnimationChanges) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  EXPECT_FALSE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 1: An animation that's allowed to run until its finish point.
  AddAnimatedBackdropFilterToAnimation(animation_.get(), 1.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  // Finish the animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);
  EXPECT_FALSE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();

  // Finished animations are pushed, but animations_impl hasn't yet ticked
  // at/past the end of the animation.
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 2: An animation that's removed before it finishes.
  int keyframe_model_id =
      AddAnimatedBackdropFilterToAnimation(animation_.get(), 10.0, 0.f, 1.f);
  EXPECT_TRUE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_FALSE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 3: An animation that's aborted before it finishes.
  keyframe_model_id =
      AddAnimatedBackdropFilterToAnimation(animation_.get(), 10.0, 0.f, 0.5f);
  EXPECT_TRUE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_TRUE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  events->events_.clear();

  animation_impl_->AbortKeyframeModelsWithProperty(
      TargetProperty::BACKDROP_FILTER, false);
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animation_impl_->UpdateState(true, events.get());

  animation_->DispatchAndDelegateAnimationEvent(events->events_[0]);
  EXPECT_FALSE(client_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  // Case 4 : An animation that's not in effect.
  keyframe_model_id =
      AddAnimatedBackdropFilterToAnimation(animation_.get(), 1.0, 0.f, 0.5f);
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_time_offset(base::TimeDelta::FromMilliseconds(-10000));
  animation_->keyframe_effect()
      ->GetKeyframeModelById(keyframe_model_id)
      ->set_fill_mode(KeyframeModel::FillMode::NONE);

  PushProperties();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::PENDING));
  EXPECT_FALSE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  EXPECT_TRUE(client_impl_.GetHasPotentialBackdropFilterAnimation(
      element_id_, ElementListType::ACTIVE));
  EXPECT_FALSE(client_impl_.GetBackdropFilterIsCurrentlyAnimating(
      element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  AddOpacityTransitionToAnimation(animation_.get(), 1, 1.f, 2.f, true);

  animation_->Tick(kInitialTickTime);
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, ClippedNegativeOpacityValues) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  AddOpacityTransitionToAnimation(animation_.get(), 1, 0.f, -2.f, true);

  animation_->Tick(kInitialTickTime);
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Opacity values are clipped [0,1]
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, PushedDeletedAnimationWaitsForActivation) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  const int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0.5f, 1.f, true);
  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  // Delete the animation on the main-thread animations.
  animation_->RemoveKeyframeModel(
      animation_->GetKeyframeModel(TargetProperty::OPACITY)->id());
  PushProperties();

  // The animation should no longer affect pending elements.
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(keyframe_model_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(keyframe_model_id)
                  ->affects_active_elements());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_impl_->UpdateState(true, events.get());

  // Only the active observer should have been ticked.
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.75f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();
  events = CreateEventsForTesting();
  animation_impl_->UpdateState(true, events.get());

  // After Activation the animation doesn't affect neither active nor pending
  // thread. UpdateState for this animation would put the animation to wait for
  // deletion state.
  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(keyframe_model_id)
                ->run_state());
  EXPECT_EQ(1u, events->events_.size());

  // The animation is finished on impl thread, and main thread will delete it
  // during commit.
  animation_->animation_host()->SetAnimationEvents(std::move(events));
  PushProperties();
  EXPECT_FALSE(animation_impl_->keyframe_effect()->has_any_keyframe_model());
}

// Tests that an animation that affects only active elements won't block
// an animation that affects only pending elements from starting.
TEST_F(ElementAnimationsTest, StartAnimationsAffectingDifferentObservers) {
  CreateTestLayer(true, true);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  auto events = CreateEventsForTesting();

  const int first_keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 0.f, 1.f, true);

  PushProperties();
  animation_impl_->ActivateKeyframeModels();
  animation_impl_->Tick(kInitialTickTime);
  animation_impl_->UpdateState(true, events.get());

  // Remove the first animation from the main-thread animations, and add a
  // new animation affecting the same property.
  animation_->RemoveKeyframeModel(
      animation_->GetKeyframeModel(TargetProperty::OPACITY)->id());
  const int second_keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1, 1.f, 0.5f, true);
  PushProperties();

  // The original animation should only affect active elements, and the new
  // animation should only affect pending elements.
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(first_keyframe_model_id)
                   ->affects_pending_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(first_keyframe_model_id)
                  ->affects_active_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(second_keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(second_keyframe_model_id)
                   ->affects_active_elements());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(500));
  animation_impl_->UpdateState(true, events.get());

  // The original animation should still be running, and the new animation
  // should be starting.
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(first_keyframe_model_id)
                ->run_state());
  EXPECT_EQ(KeyframeModel::STARTING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(second_keyframe_model_id)
                ->run_state());

  // The active observer should have been ticked by the original animation,
  // and the pending observer should have been ticked by the new animation.
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(0.5f,
            client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));

  animation_impl_->ActivateKeyframeModels();

  // The original animation no longer affect either elements, and the new
  // animation should now affect both elements.
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(first_keyframe_model_id)
                   ->affects_pending_elements());
  EXPECT_FALSE(animation_impl_->keyframe_effect()
                   ->GetKeyframeModelById(first_keyframe_model_id)
                   ->affects_active_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(second_keyframe_model_id)
                  ->affects_pending_elements());
  EXPECT_TRUE(animation_impl_->keyframe_effect()
                  ->GetKeyframeModelById(second_keyframe_model_id)
                  ->affects_active_elements());

  animation_impl_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_impl_->UpdateState(true, events.get());

  // The original animation should be marked for waiting for deletion.
  EXPECT_EQ(KeyframeModel::WAITING_FOR_DELETION,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(first_keyframe_model_id)
                ->run_state());

  // The new animation should be running, and the active observer should have
  // been ticked at the new animation's starting point.
  EXPECT_EQ(KeyframeModel::RUNNING,
            animation_impl_->keyframe_effect()
                ->GetKeyframeModelById(second_keyframe_model_id)
                ->run_state());
  EXPECT_EQ(1.f,
            client_impl_.GetOpacity(element_id_, ElementListType::PENDING));
  EXPECT_EQ(1.f, client_impl_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsCurrentlyAnimatingProperty) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  // Create an animation that initially affects only pending elements.
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  keyframe_model->set_affects_active_elements(false);

  animation_->AddKeyframeModel(std::move(keyframe_model));
  animation_->Tick(kInitialTickTime);
  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());

  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animation_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(10));
  animation_->UpdateState(true, nullptr);

  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  EXPECT_EQ(0.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));

  // Tick past the end of the animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1100));
  animation_->UpdateState(true, nullptr);

  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  EXPECT_EQ(1.f, client_.GetOpacity(element_id_, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, TestIsAnimatingPropertyTimeOffsetFillMode) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  // Create an animation that initially affects only pending elements, and has
  // a start delay of 2 seconds.
  std::unique_ptr<KeyframeModel> keyframe_model(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 0.f, 1.f)),
      1, TargetProperty::OPACITY));
  keyframe_model->set_fill_mode(KeyframeModel::FillMode::NONE);
  keyframe_model->set_time_offset(TimeDelta::FromMilliseconds(-2000));
  keyframe_model->set_affects_active_elements(false);

  animation_->AddKeyframeModel(std::move(keyframe_model));

  animation_->Tick(kInitialTickTime);

  // Since the animation has a start delay, the elements it affects have a
  // potentially running transform animation but aren't currently animating
  // transform.
  EXPECT_TRUE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animation_->ActivateKeyframeModels();

  EXPECT_TRUE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(animation_->keyframe_effect()->HasTickingKeyframeModel());
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::FILTER, ElementListType::ACTIVE));

  animation_->UpdateState(true, nullptr);

  // Tick past the start delay.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(2000));
  animation_->UpdateState(true, nullptr);
  EXPECT_TRUE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_TRUE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));

  // After the animaton finishes, the elements it affects have neither a
  // potentially running transform animation nor a currently running transform
  // animation.
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(4000));
  animation_->UpdateState(true, nullptr);
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsPotentiallyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::PENDING));
  EXPECT_FALSE(animation_->keyframe_effect()->IsCurrentlyAnimatingProperty(
      TargetProperty::OPACITY, ElementListType::ACTIVE));
}

TEST_F(ElementAnimationsTest, DestroyTestMainLayerBeforePushProperties) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();
  EXPECT_EQ(0u, host_->ticking_animations_for_testing().size());

  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  EXPECT_EQ(1u, host_->ticking_animations_for_testing().size());

  DestroyTestMainLayer();
  host_->UpdateAnimationState(true, nullptr);
  EXPECT_EQ(0u, host_->ticking_animations_for_testing().size());

  PushProperties();
  host_impl_->ActivateAnimations(nullptr);
  EXPECT_EQ(0u, host_->ticking_animations_for_testing().size());
  EXPECT_EQ(0u, host_impl_->ticking_animations_for_testing().size());
}

TEST_F(ElementAnimationsTest, RemoveAndReAddAnimationToTicking) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();
  EXPECT_EQ(0u, host_->ticking_animations_for_testing().size());

  // Add an animation and ensure the animation is in the host's ticking
  // animations. Remove the animation using RemoveFromTicking().
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      1, TargetProperty::OPACITY));
  ASSERT_EQ(1u, host_->ticking_animations_for_testing().size());
  animation_->keyframe_effect()->RemoveFromTicking();
  ASSERT_EQ(0u, host_->ticking_animations_for_testing().size());

  // Ensure that adding a new animation will correctly update the ticking
  // animations list.
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  EXPECT_EQ(1u, host_->ticking_animations_for_testing().size());
}

TEST_F(ElementAnimationsTest, TickingKeyframeModelsCount) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();

  // Add an animation and ensure the animation is in the host's ticking
  // animations.
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(1.0, 1.f, 0.5f)),
      2, TargetProperty::OPACITY));
  EXPECT_EQ(1u, animation_->TickingKeyframeModelsCount());
  EXPECT_EQ(1u, host_->CompositedAnimationsCount());
  animation_->AddKeyframeModel(CreateKeyframeModel(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1)), 1,
      TargetProperty::TRANSFORM));
  EXPECT_EQ(2u, animation_->TickingKeyframeModelsCount());
  EXPECT_EQ(2u, host_->CompositedAnimationsCount());
  animation_->keyframe_effect()->RemoveFromTicking();
  EXPECT_EQ(0u, host_->CompositedAnimationsCount());
}

// This test verifies that finished keyframe models don't get copied over to
// impl thread.
TEST_F(ElementAnimationsTest, FinishedKeyframeModelsNotCopiedToImpl) {
  CreateTestLayer(false, false);
  AttachTimelineAnimationLayer();
  CreateImplTimelineAndAnimation();

  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeTransformTransition(1.0)), 1, 1,
      TargetProperty::TRANSFORM));
  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::unique_ptr<AnimationCurve>(new FakeFloatTransition(2.0, 0.f, 1.f)),
      2, 2, TargetProperty::OPACITY));

  // Finish the first keyframe model.
  animation_->Tick(kInitialTickTime);
  animation_->UpdateState(true, nullptr);
  animation_->Tick(kInitialTickTime + TimeDelta::FromMilliseconds(1000));
  animation_->UpdateState(true, nullptr);

  EXPECT_EQ(
      KeyframeModel::FINISHED,
      animation_->keyframe_effect()->GetKeyframeModelById(1)->run_state());
  EXPECT_EQ(
      KeyframeModel::RUNNING,
      animation_->keyframe_effect()->GetKeyframeModelById(2)->run_state());

  PushProperties();

  // Finished keyframe model doesn't get copied to impl thread.
  EXPECT_FALSE(animation_impl_->keyframe_effect()->GetKeyframeModelById(1));
  EXPECT_TRUE(animation_impl_->keyframe_effect()->GetKeyframeModelById(2));
}

}  // namespace
}  // namespace cc
