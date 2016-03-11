// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
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

  bool is_hover_fading_in_or_visible() {
    return ink_drop_animation_controller_.IsHoverFadingInOrVisible();
  }

  const InkDropHover* hover() const {
    return ink_drop_animation_controller_.hover_.get();
  }

  // The test target.
  InkDropAnimationControllerImpl ink_drop_animation_controller_;

  // Used to control the tasks scheduled by the InkDropAnimationControllerImpl's
  // Timer.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // Required by base::Timer's.
  scoped_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;

 private:
  // Ensures all animations complete immediately.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerImplTest);
};

InkDropAnimationControllerImplTest::InkDropAnimationControllerImplTest()
    : ink_drop_animation_controller_(&ink_drop_host_),
      task_runner_(new base::TestSimpleTaskRunner),
      thread_task_runner_handle_(
          new base::ThreadTaskRunnerHandle(task_runner_)) {
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
}

InkDropAnimationControllerImplTest::~InkDropAnimationControllerImplTest() {}

TEST_F(InkDropAnimationControllerImplTest, SetHoveredIsFadingInOrVisible) {
  ink_drop_host_.set_should_show_hover(true);

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_TRUE(is_hover_fading_in_or_visible());

  ink_drop_animation_controller_.SetHovered(false);
  EXPECT_FALSE(is_hover_fading_in_or_visible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoverDoesntFadeInAfterAnimationIfHoverNotSet) {
  ink_drop_host_.set_should_show_hover(true);
  ink_drop_animation_controller_.SetHovered(false);
  ink_drop_animation_controller_.AnimateToState(InkDropState::QUICK_ACTION);

  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_FALSE(is_hover_fading_in_or_visible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoverFadesInAfterAnimationWhenHostIsHovered) {
  ink_drop_host_.set_should_show_hover(true);
  ink_drop_animation_controller_.SetHovered(true);
  ink_drop_animation_controller_.AnimateToState(InkDropState::QUICK_ACTION);

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunPendingTasks();

  EXPECT_TRUE(is_hover_fading_in_or_visible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoverDoesntFadeInAfterAnimationWhenHostIsNotHovered) {
  ink_drop_host_.set_should_show_hover(false);
  ink_drop_animation_controller_.SetHovered(true);
  ink_drop_animation_controller_.AnimateToState(InkDropState::QUICK_ACTION);

  EXPECT_TRUE(task_runner_->HasPendingTask());

  task_runner_->RunPendingTasks();

  EXPECT_FALSE(is_hover_fading_in_or_visible());
}

TEST_F(InkDropAnimationControllerImplTest,
       HoveredStateNotVisibleOrFadingInAfterAnimateToState) {
  ink_drop_host_.set_should_show_hover(true);

  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_TRUE(is_hover_fading_in_or_visible());

  ink_drop_animation_controller_.AnimateToState(InkDropState::QUICK_ACTION);
  EXPECT_FALSE(is_hover_fading_in_or_visible());
}

// Verifies that there is not a crash when setting hovered state and the host
// returns null for the hover.
TEST_F(InkDropAnimationControllerImplTest,
       SetHoveredFalseWorksWhenNoInkDropHoverExists) {
  ink_drop_host_.set_should_show_hover(false);
  ink_drop_animation_controller_.SetHovered(true);
  EXPECT_FALSE(hover());
  ink_drop_animation_controller_.SetHovered(false);
  EXPECT_FALSE(hover());
}

TEST_F(InkDropAnimationControllerImplTest, HoverFadesOutOnSnapToActivated) {
  ink_drop_host_.set_should_show_hover(true);
  ink_drop_animation_controller_.SetHovered(true);

  EXPECT_TRUE(is_hover_fading_in_or_visible());

  ink_drop_animation_controller_.SnapToActivated();

  EXPECT_FALSE(is_hover_fading_in_or_visible());
}

}  // namespace views
