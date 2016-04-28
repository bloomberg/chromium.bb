// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_

#include "ui/views/animation/ink_drop_animation.h"

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_animation.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/square_ink_drop_animation.h"
#include "ui/views/animation/test/flood_fill_ink_drop_animation_test_api.h"
#include "ui/views/animation/test/ink_drop_animation_test_api.h"
#include "ui/views/animation/test/square_ink_drop_animation_test_api.h"
#include "ui/views/animation/test/test_ink_drop_animation_observer.h"

namespace views {
namespace test {

// Represents all the derivatives of the InkDropAnimation class. To be used with
// the InkDropAnimationTest fixture to test all derviatives.
enum InkDropAnimationTestTypes {
  SQUARE_INK_DROP_ANIMATION,
  FLOOD_FILL_INK_DROP_ANIMATION
};

// Test fixture for all InkDropAnimation class derivatives.
//
// To add a new derivative:
//    1. Add a value to the InkDropAnimationTestTypes enum.
//    2. Implement set up and tear down code for the new enum value in
//       InkDropAnimationTest() and
//      ~InkDropAnimationTest().
//    3. Add the new enum value to the INSTANTIATE_TEST_CASE_P) Values list.
class InkDropAnimationTest
    : public testing::TestWithParam<InkDropAnimationTestTypes> {
 public:
  InkDropAnimationTest();
  ~InkDropAnimationTest() override;

 protected:
  TestInkDropAnimationObserver observer_;

  std::unique_ptr<InkDropAnimation> ink_drop_animation_;

  std::unique_ptr<InkDropAnimationTestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationTest);
};

InkDropAnimationTest::InkDropAnimationTest() {
  switch (GetParam()) {
    case SQUARE_INK_DROP_ANIMATION: {
      SquareInkDropAnimation* square_ink_drop_animation =
          new SquareInkDropAnimation(gfx::Size(10, 10), 2, gfx::Size(8, 8), 1,
                                     gfx::Point(), SK_ColorBLACK);
      ink_drop_animation_.reset(square_ink_drop_animation);
      test_api_.reset(
          new SquareInkDropAnimationTestApi(square_ink_drop_animation));
      break;
    }
    case FLOOD_FILL_INK_DROP_ANIMATION: {
      FloodFillInkDropAnimation* flood_fill_ink_drop_animation =
          new FloodFillInkDropAnimation(gfx::Rect(0, 0, 10, 10), gfx::Point(),
                                        SK_ColorBLACK);
      ink_drop_animation_.reset(flood_fill_ink_drop_animation);
      test_api_.reset(
          new FloodFillInkDropAnimationTestApi(flood_fill_ink_drop_animation));
      break;
    }
  }
  ink_drop_animation_->set_observer(&observer_);
  observer_.set_ink_drop_animation(ink_drop_animation_.get());
  test_api_->SetDisableAnimationTimers(true);
}

InkDropAnimationTest::~InkDropAnimationTest() {}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(,
                        InkDropAnimationTest,
                        testing::Values(SQUARE_INK_DROP_ANIMATION,
                                        FLOOD_FILL_INK_DROP_ANIMATION));

TEST_P(InkDropAnimationTest, InitialStateAfterConstruction) {
  EXPECT_EQ(views::InkDropState::HIDDEN,
            ink_drop_animation_->target_ink_drop_state());
}

// Verify no animations are used when animating from HIDDEN to HIDDEN.
TEST_P(InkDropAnimationTest, AnimateToHiddenFromInvisibleState) {
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_animation_->target_ink_drop_state());

  ink_drop_animation_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
  EXPECT_FALSE(ink_drop_animation_->IsVisible());
}

TEST_P(InkDropAnimationTest, AnimateToHiddenFromVisibleState) {
  ink_drop_animation_->AnimateToState(InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_NE(InkDropState::HIDDEN, ink_drop_animation_->target_ink_drop_state());

  ink_drop_animation_->AnimateToState(InkDropState::HIDDEN);
  test_api_->CompleteAnimations();

  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(4, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropAnimationTest, ActionPendingOpacity) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropAnimationTest, QuickActionOpacity) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropAnimationTest, SlowActionPendingOpacity) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(
      views::InkDropState::ALTERNATE_ACTION_PENDING);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropAnimationTest, SlowActionOpacity) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(
      views::InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_animation_->AnimateToState(
      views::InkDropState::ALTERNATE_ACTION_TRIGGERED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropAnimationTest, ActivatedOpacity) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTIVATED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_->GetCurrentOpacity());
}

TEST_P(InkDropAnimationTest, DeactivatedOpacity) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTIVATED);
  ink_drop_animation_->AnimateToState(views::InkDropState::DEACTIVATED);
  test_api_->CompleteAnimations();

  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
}

// Verify animations are aborted during deletion and the
// InkDropAnimationObservers are notified.
TEST_P(InkDropAnimationTest, AnimationsAbortedDuringDeletion) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_.reset();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

TEST_P(InkDropAnimationTest, VerifyObserversAreNotified) {
  ink_drop_animation_->AnimateToState(InkDropState::ACTION_PENDING);

  EXPECT_TRUE(test_api_->HasActiveAnimations());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_TRUE(observer_.AnimationHasNotEnded());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_started_context());

  test_api_->CompleteAnimations();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_ended_context());
}

TEST_P(InkDropAnimationTest, VerifyObserversAreNotifiedOfSuccessfulAnimations) {
  ink_drop_animation_->AnimateToState(InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer_.last_animation_ended_reason());
}

TEST_P(InkDropAnimationTest, VerifyObserversAreNotifiedOfPreemptedAnimations) {
  ink_drop_animation_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

TEST_P(InkDropAnimationTest, InkDropStatesPersistWhenCallingAnimateToState) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTIVATED);
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_animation_->target_ink_drop_state());
}

TEST_P(InkDropAnimationTest, HideImmediatelyWithoutActiveAnimations) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::HIDDEN, ink_drop_animation_->target_ink_drop_state());

  ink_drop_animation_->HideImmediately();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            ink_drop_animation_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
  EXPECT_FALSE(ink_drop_animation_->IsVisible());
}

// Verifies all active animations are aborted and the InkDropState is set to
// HIDDEN after invoking HideImmediately().
TEST_P(InkDropAnimationTest, HideImmediatelyWithActiveAnimations) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  EXPECT_TRUE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::HIDDEN, ink_drop_animation_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());

  ink_drop_animation_->HideImmediately();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            ink_drop_animation_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());

  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_->GetCurrentOpacity());
  EXPECT_FALSE(ink_drop_animation_->IsVisible());
}

TEST_P(InkDropAnimationTest, SnapToActivatedWithoutActiveAnimations) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_->CompleteAnimations();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::ACTIVATED,
            ink_drop_animation_->target_ink_drop_state());

  ink_drop_animation_->SnapToActivated();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_animation_->target_ink_drop_state());
  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(4, observer_.last_animation_ended_ordinal());

  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_->GetCurrentOpacity());
  EXPECT_TRUE(ink_drop_animation_->IsVisible());
}

// Verifies all active animations are aborted and the InkDropState is set to
// ACTIVATED after invoking SnapToActivated().
TEST_P(InkDropAnimationTest, SnapToActivatedWithActiveAnimations) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  EXPECT_TRUE(test_api_->HasActiveAnimations());
  EXPECT_NE(InkDropState::ACTIVATED,
            ink_drop_animation_->target_ink_drop_state());
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());

  ink_drop_animation_->SnapToActivated();

  EXPECT_FALSE(test_api_->HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_animation_->target_ink_drop_state());
  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(4, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropState::ACTIVATED, observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer_.last_animation_ended_reason());

  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_->GetCurrentOpacity());
  EXPECT_TRUE(ink_drop_animation_->IsVisible());
}

TEST_P(InkDropAnimationTest, AnimateToVisibleFromHidden) {
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_animation_->target_ink_drop_state());
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  EXPECT_TRUE(ink_drop_animation_->IsVisible());
}

// Verifies that the value of InkDropAnimation::target_ink_drop_state() returns
// the most recent value passed to AnimateToState() when notifying observers
// that an animation has started within the AnimateToState() function call.
TEST_P(InkDropAnimationTest, TargetInkDropStateOnAnimationStarted) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(views::InkDropState::HIDDEN);

  EXPECT_EQ(3, observer_.last_animation_started_ordinal());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            observer_.target_state_at_last_animation_started());
}

// Verifies that the value of InkDropAnimation::target_ink_drop_state() returns
// the most recent value passed to AnimateToState() when notifying observers
// that an animation has ended within the AnimateToState() function call.
TEST_P(InkDropAnimationTest, TargetInkDropStateOnAnimationEnded) {
  ink_drop_animation_->AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_->AnimateToState(views::InkDropState::HIDDEN);

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            observer_.target_state_at_last_animation_ended());
}

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_
