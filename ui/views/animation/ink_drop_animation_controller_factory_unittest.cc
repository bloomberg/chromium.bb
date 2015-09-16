// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_FACTORY_UNITTEST_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_FACTORY_UNITTEST_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/animation/ink_drop_animation_controller_factory.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/test_ink_drop_host.h"

namespace views {

// Macro that will execute |test_code| against all derivatives of the
// InkDropAnimationController returned by the InkDropAnimationControllerFactory.
#define TEST_ALL_INK_DROPS(test_name, test_code)           \
  TEST_F(InkDropAnimationControllerFactoryTest, test_name) \
  test_code TEST_F(MDInkDropAnimationControllerFactoryTest, test_name) test_code

// Test fixture to test the non material design InkDropAnimationController.
class InkDropAnimationControllerFactoryTest : public testing::Test {
 public:
  InkDropAnimationControllerFactoryTest() {}
  ~InkDropAnimationControllerFactoryTest() override {}

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  // A dummy InkDropHost required to create an InkDropAnimationController.
  TestInkDropHost test_ink_drop_host_;

  // The InkDropAnimationController returned by the
  // InkDropAnimationControllerFactory test target.
  scoped_ptr<InkDropAnimationController> ink_drop_animation_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerFactoryTest);
};

void InkDropAnimationControllerFactoryTest::SetUp() {
  // Any call by a previous test to MaterialDesignController::GetMode() will
  // initialize and cache the mode. This ensures that these tests will run from
  // a non-initialized state.
  ui::test::MaterialDesignControllerTestAPI::UninitializeMode();

  ink_drop_animation_controller_ =
      InkDropAnimationControllerFactory::CreateInkDropAnimationController(
          &test_ink_drop_host_);
}

void InkDropAnimationControllerFactoryTest::TearDown() {
  ui::test::MaterialDesignControllerTestAPI::UninitializeMode();
}

// Test fixture to test the material design InkDropAnimationController.
class MDInkDropAnimationControllerFactoryTest
    : public InkDropAnimationControllerFactoryTest {
 public:
  MDInkDropAnimationControllerFactoryTest() {}

  // InkDropAnimationControllerFactoryTest:
  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MDInkDropAnimationControllerFactoryTest);
};

void MDInkDropAnimationControllerFactoryTest::SetUp() {
  ui::test::MaterialDesignControllerTestAPI::SetMode(
      ui::MaterialDesignController::MATERIAL_NORMAL);
  InkDropAnimationControllerFactoryTest::SetUp();
}

TEST_ALL_INK_DROPS(StateIsHiddenInitially,
                   {
                     EXPECT_EQ(
                         InkDropState::HIDDEN,
                         ink_drop_animation_controller_->GetInkDropState());
                   })

TEST_ALL_INK_DROPS(TypicalQuickAction,
                   {
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::ACTION_PENDING);
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::QUICK_ACTION);
                     EXPECT_EQ(
                         InkDropState::HIDDEN,
                         ink_drop_animation_controller_->GetInkDropState());
                   })

TEST_ALL_INK_DROPS(CancelQuickAction,
                   {
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::ACTION_PENDING);
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::HIDDEN);
                     EXPECT_EQ(
                         InkDropState::HIDDEN,
                         ink_drop_animation_controller_->GetInkDropState());
                   })

TEST_ALL_INK_DROPS(TypicalSlowAction,
                   {
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::ACTION_PENDING);
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::SLOW_ACTION_PENDING);
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::SLOW_ACTION);
                     EXPECT_EQ(
                         InkDropState::HIDDEN,
                         ink_drop_animation_controller_->GetInkDropState());
                   })

TEST_ALL_INK_DROPS(CancelSlowAction,
                   {
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::ACTION_PENDING);
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::SLOW_ACTION_PENDING);
                     ink_drop_animation_controller_->AnimateToState(
                         InkDropState::HIDDEN);
                     EXPECT_EQ(
                         InkDropState::HIDDEN,
                         ink_drop_animation_controller_->GetInkDropState());
                   })

TEST_ALL_INK_DROPS(
    TypicalQuickActivated,
    {
      ink_drop_animation_controller_->AnimateToState(
          InkDropState::ACTION_PENDING);
      ink_drop_animation_controller_->AnimateToState(InkDropState::ACTIVATED);
      ink_drop_animation_controller_->AnimateToState(InkDropState::DEACTIVATED);
      EXPECT_EQ(InkDropState::HIDDEN,
                ink_drop_animation_controller_->GetInkDropState());
    })

TEST_ALL_INK_DROPS(
    TypicalSlowActivated,
    {
      ink_drop_animation_controller_->AnimateToState(
          InkDropState::ACTION_PENDING);
      ink_drop_animation_controller_->AnimateToState(
          InkDropState::SLOW_ACTION_PENDING);
      ink_drop_animation_controller_->AnimateToState(InkDropState::ACTIVATED);
      ink_drop_animation_controller_->AnimateToState(InkDropState::DEACTIVATED);
      EXPECT_EQ(InkDropState::HIDDEN,
                ink_drop_animation_controller_->GetInkDropState());
    })

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_FACTORY_UNITTEST_H_
