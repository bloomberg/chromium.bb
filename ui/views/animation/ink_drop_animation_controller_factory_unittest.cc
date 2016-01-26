// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/animation/ink_drop_animation_controller_factory.h"
#include "ui/views/animation/ink_drop_animation_controller_impl.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/test_ink_drop_host.h"

namespace views {

class InkDropAnimationControllerFactoryTest
    : public testing::TestWithParam<
          testing::tuple<ui::MaterialDesignController::Mode>> {
 public:
  InkDropAnimationControllerFactoryTest();
  ~InkDropAnimationControllerFactoryTest();

 protected:
  // A dummy InkDropHost required to create an InkDropAnimationController.
  TestInkDropHost test_ink_drop_host_;

  // The InkDropAnimationController returned by the
  // InkDropAnimationControllerFactory test target.
  scoped_ptr<InkDropAnimationController> ink_drop_animation_controller_;

 private:
  // Extracts and returns the material design mode from the test parameters.
  ui::MaterialDesignController::Mode GetMaterialMode() const;

  scoped_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  // Required by base::Timer's.
  scoped_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerFactoryTest);
};

InkDropAnimationControllerFactoryTest::InkDropAnimationControllerFactoryTest()
    : ink_drop_animation_controller_(nullptr) {
  // Any call by a previous test to MaterialDesignController::GetMode() will
  // initialize and cache the mode. This ensures that these tests will run from
  // a non-initialized state.
  ui::test::MaterialDesignControllerTestAPI::UninitializeMode();
  ui::test::MaterialDesignControllerTestAPI::SetMode(GetMaterialMode());
  ink_drop_animation_controller_.reset(
      InkDropAnimationControllerFactory::CreateInkDropAnimationController(
          &test_ink_drop_host_)
          .release());
  ink_drop_animation_controller_->SetInkDropSize(gfx::Size(10, 10), 4,
                                                 gfx::Size(8, 8), 2);

  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));

  switch (GetMaterialMode()) {
    case ui::MaterialDesignController::NON_MATERIAL:
      break;
    case ui::MaterialDesignController::MATERIAL_NORMAL:
    case ui::MaterialDesignController::MATERIAL_HYBRID:
      // The Timer's used by the InkDropAnimationControllerImpl class require a
      // base::ThreadTaskRunnerHandle instance.
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner(
          new base::TestMockTimeTaskRunner);
      thread_task_runner_handle_.reset(
          new base::ThreadTaskRunnerHandle(task_runner));
      break;
  }
}

InkDropAnimationControllerFactoryTest::
    ~InkDropAnimationControllerFactoryTest() {
  ui::test::MaterialDesignControllerTestAPI::UninitializeMode();
}

ui::MaterialDesignController::Mode
InkDropAnimationControllerFactoryTest::GetMaterialMode() const {
  return testing::get<0>(GetParam());
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(
    ,
    InkDropAnimationControllerFactoryTest,
    testing::Values(ui::MaterialDesignController::NON_MATERIAL,
                    ui::MaterialDesignController::MATERIAL_NORMAL,
                    ui::MaterialDesignController::MATERIAL_HYBRID));

TEST_P(InkDropAnimationControllerFactoryTest,
       VerifyAllInkDropLayersRemovedAfterDestruction) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_.reset();
  EXPECT_EQ(0, test_ink_drop_host_.num_ink_drop_layers());
}

TEST_P(InkDropAnimationControllerFactoryTest, StateIsHiddenInitially) {
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

TEST_P(InkDropAnimationControllerFactoryTest, HoveredStateAfterAnimateToState) {
  ink_drop_animation_controller_->SetHovered(true);
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);

  EXPECT_FALSE(ink_drop_animation_controller_->IsHovered());
}

TEST_P(InkDropAnimationControllerFactoryTest, TypicalQuickAction) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(InkDropState::QUICK_ACTION);
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

TEST_P(InkDropAnimationControllerFactoryTest, CancelQuickAction) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

TEST_P(InkDropAnimationControllerFactoryTest, TypicalSlowAction) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(
      InkDropState::SLOW_ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(InkDropState::SLOW_ACTION);
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

TEST_P(InkDropAnimationControllerFactoryTest, CancelSlowAction) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(
      InkDropState::SLOW_ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

TEST_P(InkDropAnimationControllerFactoryTest, TypicalQuickActivated) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTIVATED);
  ink_drop_animation_controller_->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

TEST_P(InkDropAnimationControllerFactoryTest, TypicalSlowActivated) {
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(
      InkDropState::SLOW_ACTION_PENDING);
  ink_drop_animation_controller_->AnimateToState(InkDropState::ACTIVATED);
  ink_drop_animation_controller_->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN,
            ink_drop_animation_controller_->GetInkDropState());
}

}  // namespace views
