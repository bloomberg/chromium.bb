// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_factory.h"

#include <memory>

#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/test_ink_drop_host.h"

namespace views {

class InkDropFactoryTest
    : public testing::TestWithParam<
          testing::tuple<ui::MaterialDesignController::Mode>> {
 public:
  InkDropFactoryTest();
  ~InkDropFactoryTest();

 protected:
  // A dummy InkDropHost required to create an InkDrop.
  TestInkDropHost test_ink_drop_host_;

  // The InkDrop returned by the InkDropFactory test target.
  std::unique_ptr<InkDrop> ink_drop_;

 private:
  // Extracts and returns the material design mode from the test parameters.
  ui::MaterialDesignController::Mode GetMaterialMode() const;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  // Required by base::Timer's.
  std::unique_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;

  std::unique_ptr<ui::test::MaterialDesignControllerTestAPI>
      material_design_state_;

  DISALLOW_COPY_AND_ASSIGN(InkDropFactoryTest);
};

InkDropFactoryTest::InkDropFactoryTest() : ink_drop_(nullptr) {
  // Any call by a previous test to MaterialDesignController::GetMode() will
  // initialize and cache the mode. This ensures that these tests will run from
  // a non-initialized state.
  material_design_state_.reset(
      new ui::test::MaterialDesignControllerTestAPI(GetMaterialMode()));
  ink_drop_.reset(
      InkDropFactory::CreateInkDrop(&test_ink_drop_host_).release());

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

InkDropFactoryTest::~InkDropFactoryTest() {
  material_design_state_.reset();
}

ui::MaterialDesignController::Mode InkDropFactoryTest::GetMaterialMode() const {
  return testing::get<0>(GetParam());
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(
    ,
    InkDropFactoryTest,
    testing::Values(ui::MaterialDesignController::NON_MATERIAL,
                    ui::MaterialDesignController::MATERIAL_NORMAL,
                    ui::MaterialDesignController::MATERIAL_HYBRID));

TEST_P(InkDropFactoryTest,
       VerifyInkDropLayersRemovedAfterDestructionWhenRippleIsActive) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_.reset();
  EXPECT_EQ(0, test_ink_drop_host_.num_ink_drop_layers());
}

TEST_P(InkDropFactoryTest,
       VerifyInkDropLayersRemovedAfterDestructionWhenHoverIsActive) {
  test_ink_drop_host_.set_should_show_hover(true);
  ink_drop_->SetHovered(true);
  ink_drop_.reset();
  EXPECT_EQ(0, test_ink_drop_host_.num_ink_drop_layers());
}

TEST_P(InkDropFactoryTest, StateIsHiddenInitially) {
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropFactoryTest, TypicalQuickAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropFactoryTest, CancelQuickAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropFactoryTest, TypicalSlowAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropFactoryTest, CancelSlowAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropFactoryTest, TypicalQuickActivated) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ACTIVATED);
  ink_drop_->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropFactoryTest, TypicalSlowActivated) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ACTIVATED);
  ink_drop_->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

}  // namespace views
