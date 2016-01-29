// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/ink_drop_animation_test_api.h"

namespace views {
namespace test {

namespace {

// Transforms a copy of |point| with |transform| and returns it.
gfx::Point TransformPoint(const gfx::Transform& transform,
                          const gfx::Point& point) {
  gfx::Point transformed_point = point;
  transform.TransformPoint(&transformed_point);
  return transformed_point;
}

class TestInkDropAnimationObserver : public InkDropAnimationObserver {
 public:
  TestInkDropAnimationObserver();
  ~TestInkDropAnimationObserver() override;

  // Resets all cached observation data.
  void ResetObservations();

  bool animation_started() const { return animation_started_; }

  bool animation_ended() const { return animation_ended_; }

  InkDropState last_animation_state_started() const {
    return last_animation_state_started_;
  }

  InkDropState last_animation_state_ended() const {
    return last_animation_state_ended_;
  }

  InkDropAnimationEndedReason last_animation_ended_reason() const {
    return last_animation_ended_reason_;
  }

  // InkDropAnimation:
  void InkDropAnimationStarted(InkDropState ink_drop_state) override;
  void InkDropAnimationEnded(InkDropState ink_drop_state,
                             InkDropAnimationEndedReason reason) override;

 private:
  // True if InkDropAnimationStarted() has been invoked.
  bool animation_started_;

  // True if InkDropAnimationEnded() has been invoked.
  bool animation_ended_;

  // The |ink_drop_state| parameter used for the last invocation of
  // InkDropAnimationStarted(). Only valid if |animation_started_| is true.
  InkDropState last_animation_state_started_;

  // The |ink_drop_state| parameter used for the last invocation of
  // InkDropAnimationEnded(). Only valid if |animation_ended_| is true.
  InkDropState last_animation_state_ended_;

  // The |reason| parameter used for the last invocation of
  // InkDropAnimationEnded(). Only valid if |animation_ended_| is true.
  InkDropAnimationEndedReason last_animation_ended_reason_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropAnimationObserver);
};

TestInkDropAnimationObserver::TestInkDropAnimationObserver()
    : animation_started_(false),
      animation_ended_(false),
      last_animation_state_started_(InkDropState::HIDDEN),
      last_animation_state_ended_(InkDropState::HIDDEN),
      last_animation_ended_reason_(InkDropAnimationEndedReason::SUCCESS) {
  ResetObservations();
}

TestInkDropAnimationObserver::~TestInkDropAnimationObserver() {}

void TestInkDropAnimationObserver::ResetObservations() {
  animation_started_ = false;
  animation_ended_ = false;
  last_animation_state_ended_ = InkDropState::HIDDEN;
  last_animation_state_started_ = InkDropState::HIDDEN;
  last_animation_ended_reason_ = InkDropAnimationEndedReason::SUCCESS;
}

void TestInkDropAnimationObserver::InkDropAnimationStarted(
    InkDropState ink_drop_state) {
  animation_started_ = true;
  last_animation_state_started_ = ink_drop_state;
}

void TestInkDropAnimationObserver::InkDropAnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  animation_ended_ = true;
  last_animation_state_ended_ = ink_drop_state;
  last_animation_ended_reason_ = reason;
}

}  // namespace

class InkDropAnimationTest : public testing::Test {
 public:
  InkDropAnimationTest();
  ~InkDropAnimationTest() override;

 protected:
  TestInkDropAnimationObserver observer_;

  InkDropAnimation ink_drop_animation_;

  InkDropAnimationTestApi test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationTest);
};

InkDropAnimationTest::InkDropAnimationTest()
    : ink_drop_animation_(gfx::Size(10, 10), 2, gfx::Size(8, 8), 1),
      test_api_(&ink_drop_animation_) {
  ink_drop_animation_.AddObserver(&observer_);
  test_api_.SetDisableAnimationTimers(true);
}

InkDropAnimationTest::~InkDropAnimationTest() {}

TEST_F(InkDropAnimationTest, InitialStateAfterConstruction) {
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop_animation_.ink_drop_state());
}

TEST_F(InkDropAnimationTest, VerifyObserversAreNotified) {
  ink_drop_animation_.AnimateToState(InkDropState::ACTION_PENDING);

  ASSERT_TRUE(test_api_.HasActiveAnimations());
  EXPECT_TRUE(observer_.animation_started());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_state_started());
  EXPECT_FALSE(observer_.animation_ended());

  observer_.ResetObservations();
  test_api_.CompleteAnimations();

  ASSERT_FALSE(test_api_.HasActiveAnimations());
  EXPECT_FALSE(observer_.animation_started());
  EXPECT_TRUE(observer_.animation_ended());
  EXPECT_EQ(InkDropState::ACTION_PENDING,
            observer_.last_animation_state_ended());
}

TEST_F(InkDropAnimationTest, VerifyObserversAreNotifiedOfSuccessfulAnimations) {
  ink_drop_animation_.AnimateToState(InkDropState::ACTION_PENDING);
  test_api_.CompleteAnimations();

  ASSERT_TRUE(observer_.animation_ended());
  EXPECT_EQ(InkDropAnimationObserver::InkDropAnimationEndedReason::SUCCESS,
            observer_.last_animation_ended_reason());
}

TEST_F(InkDropAnimationTest, VerifyObserversAreNotifiedOfPreemptedAnimations) {
  ink_drop_animation_.AnimateToState(InkDropState::ACTION_PENDING);
  observer_.ResetObservations();

  ink_drop_animation_.AnimateToState(InkDropState::SLOW_ACTION_PENDING);

  ASSERT_TRUE(observer_.animation_ended());
  EXPECT_EQ(InkDropAnimationObserver::InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

TEST_F(InkDropAnimationTest, AnimateToHiddenFromInvisibleState) {
  ASSERT_EQ(InkDropState::HIDDEN, ink_drop_animation_.ink_drop_state());

  ink_drop_animation_.AnimateToState(InkDropState::HIDDEN);
  EXPECT_TRUE(observer_.animation_started());
  EXPECT_TRUE(observer_.animation_ended());
}

TEST_F(InkDropAnimationTest, AnimateToHiddenFromVisibleState) {
  ink_drop_animation_.AnimateToState(InkDropState::ACTION_PENDING);
  test_api_.CompleteAnimations();

  observer_.ResetObservations();

  ASSERT_NE(InkDropState::HIDDEN, ink_drop_animation_.ink_drop_state());

  ink_drop_animation_.AnimateToState(InkDropState::HIDDEN);

  EXPECT_TRUE(observer_.animation_started());
  EXPECT_FALSE(observer_.animation_ended());

  test_api_.CompleteAnimations();

  EXPECT_TRUE(observer_.animation_started());
  EXPECT_TRUE(observer_.animation_ended());
}

TEST_F(InkDropAnimationTest, AnimateToActionPending) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTION_PENDING);
  test_api_.CompleteAnimations();

  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            ink_drop_animation_.ink_drop_state());
  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_.GetCurrentOpacity());
}

TEST_F(InkDropAnimationTest, AnimateToQuickAction) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_.AnimateToState(views::InkDropState::QUICK_ACTION);
  test_api_.CompleteAnimations();

  EXPECT_EQ(views::InkDropState::QUICK_ACTION,
            ink_drop_animation_.ink_drop_state());
  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_.GetCurrentOpacity());
}

TEST_F(InkDropAnimationTest, AnimateToSlowActionPending) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_.AnimateToState(views::InkDropState::SLOW_ACTION_PENDING);
  test_api_.CompleteAnimations();

  EXPECT_EQ(views::InkDropState::SLOW_ACTION_PENDING,
            ink_drop_animation_.ink_drop_state());
  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_.GetCurrentOpacity());
}

TEST_F(InkDropAnimationTest, AnimateToSlowAction) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTION_PENDING);
  ink_drop_animation_.AnimateToState(views::InkDropState::SLOW_ACTION_PENDING);
  ink_drop_animation_.AnimateToState(views::InkDropState::SLOW_ACTION);
  test_api_.CompleteAnimations();

  EXPECT_EQ(views::InkDropState::SLOW_ACTION,
            ink_drop_animation_.ink_drop_state());
  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_.GetCurrentOpacity());
}

TEST_F(InkDropAnimationTest, AnimateToActivated) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTIVATED);
  test_api_.CompleteAnimations();

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            ink_drop_animation_.ink_drop_state());
  EXPECT_EQ(InkDropAnimation::kVisibleOpacity, test_api_.GetCurrentOpacity());
}

TEST_F(InkDropAnimationTest, AnimateToDeactivated) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTIVATED);
  ink_drop_animation_.AnimateToState(views::InkDropState::DEACTIVATED);
  test_api_.CompleteAnimations();

  EXPECT_EQ(views::InkDropState::DEACTIVATED,
            ink_drop_animation_.ink_drop_state());
  EXPECT_EQ(InkDropAnimation::kHiddenOpacity, test_api_.GetCurrentOpacity());
}

// Verifies all active animations are aborted and the InkDropState is set to
// HIDDEN after invoking HideImmediately().
TEST_F(InkDropAnimationTest, HideImmediately) {
  ink_drop_animation_.AnimateToState(views::InkDropState::ACTION_PENDING);
  ASSERT_TRUE(test_api_.HasActiveAnimations());
  ASSERT_NE(InkDropState::HIDDEN, ink_drop_animation_.ink_drop_state());
  ASSERT_TRUE(observer_.animation_started());

  observer_.ResetObservations();
  ink_drop_animation_.HideImmediately();

  EXPECT_FALSE(test_api_.HasActiveAnimations());
  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop_animation_.ink_drop_state());
  ASSERT_FALSE(observer_.animation_started());
}

TEST(InkDropAnimationTransformTest,
     TransformedPointsUsingTransformsFromCalculateCircleTransforms) {
  const int kHalfDrawnSize = 5;
  const int kDrawnSize = 2 * kHalfDrawnSize;

  const int kHalfTransformedSize = 100;
  const int kTransformedSize = 2 * kHalfTransformedSize;

  // Constant points in the drawn space that will be transformed.
  const gfx::Point center(kHalfDrawnSize, kHalfDrawnSize);
  const gfx::Point mid_left(0, kHalfDrawnSize);
  const gfx::Point mid_right(kDrawnSize, kHalfDrawnSize);
  const gfx::Point top_mid(kHalfDrawnSize, 0);
  const gfx::Point bottom_mid(kHalfDrawnSize, kDrawnSize);

  scoped_ptr<InkDropAnimation> ink_drop_animation(
      new InkDropAnimation(gfx::Size(kDrawnSize, kDrawnSize), 2,
                           gfx::Size(kHalfDrawnSize, kHalfDrawnSize), 1));
  InkDropAnimationTestApi test_api(ink_drop_animation.get());

  InkDropAnimationTestApi::InkDropTransforms transforms;
  test_api.CalculateCircleTransforms(
      gfx::Size(kTransformedSize, kTransformedSize), &transforms);

  // Transform variables to reduce verbosity of actual verification code.
  const gfx::Transform kTopLeftTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::TOP_LEFT_CIRCLE];
  const gfx::Transform kTopRightTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::TOP_RIGHT_CIRCLE];
  const gfx::Transform kBottomRightTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::BOTTOM_RIGHT_CIRCLE];
  const gfx::Transform kBottomLeftTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::BOTTOM_LEFT_CIRCLE];
  const gfx::Transform kHorizontalTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::HORIZONTAL_RECT];
  const gfx::Transform kVerticalTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::VERTICAL_RECT];

  // Top left circle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kTopLeftTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedSize, 0),
            TransformPoint(kTopLeftTransform, mid_left));
  EXPECT_EQ(gfx::Point(kHalfTransformedSize, 0),
            TransformPoint(kTopLeftTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -kHalfTransformedSize),
            TransformPoint(kTopLeftTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, kHalfTransformedSize),
            TransformPoint(kTopLeftTransform, bottom_mid));

  // Top right circle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kTopRightTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedSize, 0),
            TransformPoint(kTopRightTransform, mid_left));
  EXPECT_EQ(gfx::Point(kHalfTransformedSize, 0),
            TransformPoint(kTopRightTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -kHalfTransformedSize),
            TransformPoint(kTopRightTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, kHalfTransformedSize),
            TransformPoint(kTopRightTransform, bottom_mid));

  // Bottom right circle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kBottomRightTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedSize, 0),
            TransformPoint(kBottomRightTransform, mid_left));
  EXPECT_EQ(gfx::Point(kHalfTransformedSize, 0),
            TransformPoint(kBottomRightTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -kHalfTransformedSize),
            TransformPoint(kBottomRightTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, kHalfTransformedSize),
            TransformPoint(kBottomRightTransform, bottom_mid));

  // Bottom left circle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kBottomLeftTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedSize, 0),
            TransformPoint(kBottomLeftTransform, mid_left));
  EXPECT_EQ(gfx::Point(kHalfTransformedSize, 0),
            TransformPoint(kBottomLeftTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -kHalfTransformedSize),
            TransformPoint(kBottomLeftTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, kHalfTransformedSize),
            TransformPoint(kBottomLeftTransform, bottom_mid));

  // Horizontal rectangle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kHorizontalTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedSize, 0),
            TransformPoint(kHorizontalTransform, mid_left));
  EXPECT_EQ(gfx::Point(kHalfTransformedSize, 0),
            TransformPoint(kHorizontalTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kHorizontalTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kHorizontalTransform, bottom_mid));

  // Vertical rectangle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kVerticalTransform, center));
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kVerticalTransform, mid_left));
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kVerticalTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -kHalfTransformedSize),
            TransformPoint(kVerticalTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, kHalfTransformedSize),
            TransformPoint(kVerticalTransform, bottom_mid));
}

TEST(InkDropAnimationTransformTest,
     TransformedPointsUsingTransformsFromCalculateRectTransforms) {
  const int kHalfDrawnSize = 5;
  const int kDrawnSize = 2 * kHalfDrawnSize;

  const int kTransformedRadius = 10;

  const int kHalfTransformedWidth = 100;
  const int kTransformedWidth = 2 * kHalfTransformedWidth;

  const int kHalfTransformedHeight = 100;
  const int kTransformedHeight = 2 * kHalfTransformedHeight;

  // Constant points in the drawn space that will be transformed.
  const gfx::Point center(kHalfDrawnSize, kHalfDrawnSize);
  const gfx::Point mid_left(0, kHalfDrawnSize);
  const gfx::Point mid_right(kDrawnSize, kHalfDrawnSize);
  const gfx::Point top_mid(kHalfDrawnSize, 0);
  const gfx::Point bottom_mid(kHalfDrawnSize, kDrawnSize);

  scoped_ptr<InkDropAnimation> ink_drop_animation(
      new InkDropAnimation(gfx::Size(kDrawnSize, kDrawnSize), 2,
                           gfx::Size(kHalfDrawnSize, kHalfDrawnSize), 1));
  InkDropAnimationTestApi test_api(ink_drop_animation.get());

  InkDropAnimationTestApi::InkDropTransforms transforms;
  test_api.CalculateRectTransforms(
      gfx::Size(kTransformedWidth, kTransformedHeight), kTransformedRadius,
      &transforms);

  // Transform variables to reduce verbosity of actual verification code.
  const gfx::Transform kTopLeftTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::TOP_LEFT_CIRCLE];
  const gfx::Transform kTopRightTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::TOP_RIGHT_CIRCLE];
  const gfx::Transform kBottomRightTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::BOTTOM_RIGHT_CIRCLE];
  const gfx::Transform kBottomLeftTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::BOTTOM_LEFT_CIRCLE];
  const gfx::Transform kHorizontalTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::HORIZONTAL_RECT];
  const gfx::Transform kVerticalTransform =
      transforms[InkDropAnimationTestApi::PaintedShape::VERTICAL_RECT];

  const int x_offset = kHalfTransformedWidth - kTransformedRadius;
  const int y_offset = kHalfTransformedWidth - kTransformedRadius;

  // Top left circle
  EXPECT_EQ(gfx::Point(-x_offset, -y_offset),
            TransformPoint(kTopLeftTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedWidth, -y_offset),
            TransformPoint(kTopLeftTransform, mid_left));
  EXPECT_EQ(gfx::Point(-x_offset, -kHalfTransformedHeight),
            TransformPoint(kTopLeftTransform, top_mid));

  // Top right circle
  EXPECT_EQ(gfx::Point(x_offset, -y_offset),
            TransformPoint(kTopRightTransform, center));
  EXPECT_EQ(gfx::Point(kHalfTransformedWidth, -y_offset),
            TransformPoint(kTopRightTransform, mid_right));
  EXPECT_EQ(gfx::Point(x_offset, -kHalfTransformedHeight),
            TransformPoint(kTopRightTransform, top_mid));

  // Bottom right circle
  EXPECT_EQ(gfx::Point(x_offset, y_offset),
            TransformPoint(kBottomRightTransform, center));
  EXPECT_EQ(gfx::Point(kHalfTransformedWidth, y_offset),
            TransformPoint(kBottomRightTransform, mid_right));
  EXPECT_EQ(gfx::Point(x_offset, kHalfTransformedHeight),
            TransformPoint(kBottomRightTransform, bottom_mid));

  // Bottom left circle
  EXPECT_EQ(gfx::Point(-x_offset, y_offset),
            TransformPoint(kBottomLeftTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedWidth, y_offset),
            TransformPoint(kBottomLeftTransform, mid_left));
  EXPECT_EQ(gfx::Point(-x_offset, kHalfTransformedHeight),
            TransformPoint(kBottomLeftTransform, bottom_mid));

  // Horizontal rectangle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kHorizontalTransform, center));
  EXPECT_EQ(gfx::Point(-kHalfTransformedWidth, 0),
            TransformPoint(kHorizontalTransform, mid_left));
  EXPECT_EQ(gfx::Point(kHalfTransformedWidth, 0),
            TransformPoint(kHorizontalTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -y_offset),
            TransformPoint(kHorizontalTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, y_offset),
            TransformPoint(kHorizontalTransform, bottom_mid));

  // Vertical rectangle
  EXPECT_EQ(gfx::Point(0, 0), TransformPoint(kVerticalTransform, center));
  EXPECT_EQ(gfx::Point(-x_offset, 0),
            TransformPoint(kVerticalTransform, mid_left));
  EXPECT_EQ(gfx::Point(x_offset, 0),
            TransformPoint(kVerticalTransform, mid_right));
  EXPECT_EQ(gfx::Point(0, -kHalfTransformedHeight),
            TransformPoint(kVerticalTransform, top_mid));
  EXPECT_EQ(gfx::Point(0, kHalfTransformedHeight),
            TransformPoint(kVerticalTransform, bottom_mid));
}

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_
