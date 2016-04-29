// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_hover.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/ink_drop_hover_test_api.h"
#include "ui/views/animation/test/test_ink_drop_hover_observer.h"

namespace views {
namespace test {

class InkDropHoverTest : public testing::Test {
 public:
  InkDropHoverTest();
  ~InkDropHoverTest() override;

 protected:
  // The test target.
  std::unique_ptr<InkDropHover> ink_drop_hover_;

  // Allows privileged access to the the |ink_drop_hover_|.
  InkDropHoverTestApi test_api_;

  // Observer of the test target.
  TestInkDropHoverObserver observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropHoverTest);
};

InkDropHoverTest::InkDropHoverTest()
    : ink_drop_hover_(
          new InkDropHover(gfx::Size(10, 10), 3, gfx::Point(), SK_ColorBLACK)),
      test_api_(ink_drop_hover_.get()) {
  ink_drop_hover_->set_observer(&observer_);

  test_api_.SetDisableAnimationTimers(true);
}

InkDropHoverTest::~InkDropHoverTest() {}

TEST_F(InkDropHoverTest, InitialStateAfterConstruction) {
  EXPECT_FALSE(ink_drop_hover_->IsFadingInOrVisible());
}

TEST_F(InkDropHoverTest, IsHoveredStateTransitions) {
  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(ink_drop_hover_->IsFadingInOrVisible());

  test_api_.CompleteAnimations();
  EXPECT_TRUE(ink_drop_hover_->IsFadingInOrVisible());

  ink_drop_hover_->FadeOut(base::TimeDelta::FromSeconds(1),
                           false /* explode */);
  EXPECT_FALSE(ink_drop_hover_->IsFadingInOrVisible());

  test_api_.CompleteAnimations();
  EXPECT_FALSE(ink_drop_hover_->IsFadingInOrVisible());
}

TEST_F(InkDropHoverTest, VerifyObserversAreNotified) {
  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));

  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_FALSE(observer_.AnimationHasEnded());

  test_api_.CompleteAnimations();

  EXPECT_TRUE(observer_.AnimationHasEnded());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
}

TEST_F(InkDropHoverTest, VerifyObserversAreNotifiedWithCorrectAnimationType) {
  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(observer_.AnimationHasStarted());
  EXPECT_EQ(InkDropHover::FADE_IN, observer_.last_animation_started_context());

  test_api_.CompleteAnimations();
  EXPECT_TRUE(observer_.AnimationHasEnded());
  EXPECT_EQ(InkDropHover::FADE_IN, observer_.last_animation_started_context());

  ink_drop_hover_->FadeOut(base::TimeDelta::FromSeconds(1),
                           false /* explode */);
  EXPECT_EQ(InkDropHover::FADE_OUT, observer_.last_animation_started_context());

  test_api_.CompleteAnimations();
  EXPECT_EQ(InkDropHover::FADE_OUT, observer_.last_animation_started_context());
}

TEST_F(InkDropHoverTest, VerifyObserversAreNotifiedOfSuccessfulAnimations) {
  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));
  test_api_.CompleteAnimations();

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer_.last_animation_ended_reason());
}

TEST_F(InkDropHoverTest, VerifyObserversAreNotifiedOfPreemptedAnimations) {
  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));
  ink_drop_hover_->FadeOut(base::TimeDelta::FromSeconds(1),
                           false /* explode */);

  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropHover::FADE_IN, observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

// Confirms there is no crash.
TEST_F(InkDropHoverTest, NullObserverIsSafe) {
  ink_drop_hover_->set_observer(nullptr);

  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));
  test_api_.CompleteAnimations();

  ink_drop_hover_->FadeOut(base::TimeDelta::FromMilliseconds(0),
                           false /* explode */);
  test_api_.CompleteAnimations();
  EXPECT_FALSE(ink_drop_hover_->IsFadingInOrVisible());
}

// Verify animations are aborted during deletion and the InkDropHoverObservers
// are notified.
TEST_F(InkDropHoverTest, AnimationsAbortedDuringDeletion) {
  ink_drop_hover_->FadeIn(base::TimeDelta::FromSeconds(1));
  ink_drop_hover_.reset();
  EXPECT_EQ(1, observer_.last_animation_started_ordinal());
  EXPECT_EQ(2, observer_.last_animation_ended_ordinal());
  EXPECT_EQ(InkDropHover::FADE_IN, observer_.last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer_.last_animation_ended_reason());
}

}  // namespace test
}  // namespace views
