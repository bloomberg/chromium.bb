// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_animation_controller_impl_test_api.h"
#include "ui/views/animation/test/test_ink_drop_host.h"

namespace views {

// NOTE: The InkDropAnimationControllerImpl class is also tested by the
// InkDropAnimationControllerFactoryTest tests.
class InkDropAnimationControllerImplTest : public testing::Test {
 public:
  InkDropAnimationControllerImplTest();
  ~InkDropAnimationControllerImplTest() override;

 protected:
  TestInkDropHost ink_drop_host_;

  // The test target.
  InkDropAnimationControllerImpl ink_drop_animation_controller_;

  // Allows privileged access to the the |ink_drop_hover_|.
  test::InkDropAnimationControllerImplTestApi test_api_;

  // Used to control the tasks scheduled by the InkDropAnimationControllerImpl's
  // Timer.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // Required by base::Timer's.
  std::unique_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerImplTest);
};

InkDropAnimationControllerImplTest::InkDropAnimationControllerImplTest()
    : ink_drop_animation_controller_(&ink_drop_host_),
      test_api_(&ink_drop_animation_controller_),
      task_runner_(new base::TestSimpleTaskRunner),
      thread_task_runner_handle_(
          new base::ThreadTaskRunnerHandle(task_runner_)) {
  ink_drop_host_.set_disable_timers_for_test(true);
}

InkDropAnimationControllerImplTest::~InkDropAnimationControllerImplTest() {}

TEST_F(InkDropAnimationControllerImplTest, SetHoveredIsFadingInOrVisible) {
  ink_drop_host_.set_should_show_hover(true);

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_TRUE(test_api_.IsHoverFadingInOrVisible());

  test_api_.CompleteAnimations();

  ink_drop_animation_controller_.SetHovered(false);
  EXPECT_FALSE(test_api_.IsHoverFadingInOrVisible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoverDoesntFadeInAfterAnimationIfHoverNotSet) {
  ink_drop_host_.set_should_show_hover(true);
  ink_drop_animation_controller_.SetHovered(false);
  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api_.CompleteAnimations();

  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_FALSE(test_api_.IsHoverFadingInOrVisible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoverFadesInAfterAnimationWhenHostIsHovered) {
  ink_drop_host_.set_should_show_hover(true);
  ink_drop_animation_controller_.SetHovered(true);
  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api_.CompleteAnimations();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunPendingTasks();

  EXPECT_TRUE(test_api_.IsHoverFadingInOrVisible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoverDoesntFadeInAfterAnimationWhenHostIsNotHovered) {
  ink_drop_host_.set_should_show_hover(false);
  ink_drop_animation_controller_.SetHovered(true);
  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api_.CompleteAnimations();

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunPendingTasks();

  EXPECT_FALSE(test_api_.IsHoverFadingInOrVisible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoveredStateNotVisibleOrFadingInAfterAnimateToState) {
  ink_drop_host_.set_should_show_hover(true);

  ink_drop_animation_controller_.SetHovered(true);
  test_api_.CompleteAnimations();
  EXPECT_TRUE(test_api_.IsHoverFadingInOrVisible());

  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_FALSE(test_api_.IsHoverFadingInOrVisible());
}

// Verifies that there is not a crash when setting hovered state and the host
// returns null for the hover.
TEST_F(InkDropAnimationControllerImplTest,
       SetHoveredFalseWorksWhenNoInkDropHoverExists) {
  ink_drop_host_.set_should_show_hover(false);
  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_FALSE(test_api_.hover());
  ink_drop_animation_controller_.SetHovered(false);
  EXPECT_FALSE(test_api_.hover());
}

TEST_F(InkDropAnimationControllerImplTest, HoverFadesOutOnSnapToActivated) {
  ink_drop_host_.set_should_show_hover(true);
  ink_drop_animation_controller_.SetHovered(true);
  test_api_.CompleteAnimations();

  EXPECT_TRUE(test_api_.IsHoverFadingInOrVisible());

  ink_drop_animation_controller_.SnapToActivated();

  EXPECT_FALSE(test_api_.IsHoverFadingInOrVisible());
}

TEST_F(InkDropAnimationControllerImplTest, LayersRemovedFromHostAfterHover) {
  ink_drop_host_.set_should_show_hover(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();

  ink_drop_animation_controller_.SetHovered(false);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropAnimationControllerImplTest, LayersRemovedFromHostAfterInkDrop) {
  ink_drop_host_.set_should_show_hover(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropAnimationControllerImplTest,
       LayersAddedToHostWhenHoverOrInkDropVisible) {
  ink_drop_host_.set_should_show_hover(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunPendingTasks();

  // Hover should be fading back in.
  EXPECT_TRUE(test_api_.HasActiveAnimations());
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropAnimationControllerImplTest,
       LayersNotAddedToHostWhenHoverTimeFires) {
  ink_drop_host_.set_should_show_hover(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.AnimateToState(InkDropState::HIDDEN);
  test_api_.CompleteAnimations();
  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_host_.set_should_show_hover(false);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunPendingTasks();

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());
}

TEST_F(InkDropAnimationControllerImplTest,
       LayersArentRemovedWhenPreemptingFadeOut) {
  ink_drop_host_.set_should_show_hover(true);

  EXPECT_EQ(0, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  test_api_.CompleteAnimations();

  ink_drop_animation_controller_.SetHovered(false);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_EQ(1, ink_drop_host_.num_ink_drop_layers());
}

}  // namespace views
