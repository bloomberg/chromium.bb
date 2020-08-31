// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/back_button.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class BackButtonTest : public AshTestBase,
                       public testing::WithParamInterface<bool> {
 public:
  BackButtonTest() = default;
  ~BackButtonTest() override = default;

  BackButton* back_button() {
    return test_api_->shelf_view()
        ->shelf_widget()
        ->navigation_widget()
        ->GetBackButton();
  }

  ShelfViewTestAPI* test_api() { return test_api_.get(); }

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kShelfHotseat);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kShelfHotseat);
    }

    AshTestBase::SetUp();
    // Set a11y setting to show back button in tablet mode, if the feature to
    // hide it is enabled.
    if (features::IsHideShelfControlsInTabletModeEnabled()) {
      Shell::Get()
          ->accessibility_controller()
          ->SetTabletModeShelfNavigationButtonsEnabled(true);
    }

    test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());

    // Finish all setup tasks. In particular we want to finish the
    // GetSwitchStates post task in (Fake)PowerManagerClient which is triggered
    // by TabletModeController otherwise this will cause tablet mode to exit
    // while we wait for animations in the test.
    base::RunLoop().RunUntilIdle();
  }

  bool IsBackButtonVisible() const {
    return ShelfNavigationWidget::TestApi(
               test_api_->shelf_view()->shelf_widget()->navigation_widget())
        .IsBackButtonVisible();
  }

 protected:
  std::unique_ptr<ShelfViewTestAPI> test_api_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BackButtonTest);
};

enum class TestAccessibilityFeature {
  kTabletModeShelfNavigationButtons,
  kSpokenFeedback,
  kAutoclick,
  kSwitchAccess
};

// Tests back button visibility with number of accessibility setting enabled,
// with kHideControlsInTabletModeFeature.
class BackButtonVisibilityWithAccessibilityFeaturesTest
    : public AshTestBase,
      public ::testing::WithParamInterface<TestAccessibilityFeature> {
 public:
  BackButtonVisibilityWithAccessibilityFeaturesTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kShelfHotseat,
         features::kHideShelfControlsInTabletMode},
        {});
  }
  ~BackButtonVisibilityWithAccessibilityFeaturesTest() override = default;

  void SetTestA11yFeatureEnabled(bool enabled) {
    switch (GetParam()) {
      case TestAccessibilityFeature::kTabletModeShelfNavigationButtons:
        Shell::Get()
            ->accessibility_controller()
            ->SetTabletModeShelfNavigationButtonsEnabled(enabled);
        break;
      case TestAccessibilityFeature::kSpokenFeedback:
        Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
            enabled, A11Y_NOTIFICATION_NONE);
        break;
      case TestAccessibilityFeature::kAutoclick:
        Shell::Get()->accessibility_controller()->SetAutoclickEnabled(enabled);
        break;
      case TestAccessibilityFeature::kSwitchAccess:
        Shell::Get()->accessibility_controller()->SetSwitchAccessEnabled(
            enabled);
        Shell::Get()
            ->accessibility_controller()
            ->no_switch_access_disable_confirmation_dialog_for_testing(true);
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// The parameter indicates whether the kShelfHotseat feature is enabled.
INSTANTIATE_TEST_SUITE_P(All, BackButtonTest, testing::Bool());

// Verify that the back button is visible in tablet mode.
TEST_P(BackButtonTest, Visibility) {
  EXPECT_FALSE(back_button());
  EXPECT_FALSE(IsBackButtonVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  test_api()->RunMessageLoopUntilAnimationsDone();

  // Ensure the back button is not yet visible when hotseat is enabled.
  EXPECT_EQ(!GetParam(), IsBackButtonVisible());
  ASSERT_EQ(GetParam(), !back_button());

  if (GetParam()) {
    // When hotseat is enabled, the back button is only usable in in-app shelf.
    std::unique_ptr<views::Widget> widget = CreateTestWidget();
    test_api()->RunMessageLoopUntilAnimationsDone();
  }

  EXPECT_TRUE(IsBackButtonVisible());
  EXPECT_EQ(1.f, back_button()->layer()->opacity());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  test_api()->RunMessageLoopUntilAnimationsDone();

  EXPECT_FALSE(IsBackButtonVisible());
}

// Verify that the back button is visible in tablet mode, if the initial shelf
// alignment is on the left or right.
TEST_P(BackButtonTest, VisibilityWithVerticalShelf) {
  test_api()->shelf_view()->shelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_FALSE(back_button());
  EXPECT_FALSE(IsBackButtonVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // When hotseat is enabled, the back button is only usable in in-app shelf.
  if (GetParam())
    std::unique_ptr<views::Widget> widget = CreateTestWidget();

  test_api()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(back_button());
  EXPECT_TRUE(IsBackButtonVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  test_api()->RunMessageLoopUntilAnimationsDone();

  EXPECT_FALSE(back_button());
  EXPECT_FALSE(IsBackButtonVisible());
}

TEST_P(BackButtonTest, BackKeySequenceGenerated) {
  // Enter tablet mode; the back button is not visible in non tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  // When hotseat is enabled, the back button is only usable in in-app shelf.
  if (GetParam())
    std::unique_ptr<views::Widget> widget = CreateTestWidget();

  ShelfNavigationWidget::TestApi navigation_widget_test_api(
      GetPrimaryShelf()->navigation_widget());
  // Wait for the navigation widget's animation.
  test_api()->RunMessageLoopUntilAnimationsDone(
      navigation_widget_test_api.GetBoundsAnimator());

  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();

  // Register an accelerator that looks for back presses. Note there is already
  // an accelerator on AppListView, which will handle the accelerator since it
  // is targeted before AcceleratorController (switching to tablet mode with no
  // other windows activates the app list). First remove that accelerator. In
  // release, there's only the AppList's accelerator, so it's always hit when
  // the app list is active. (ash/accelerators.cc has VKEY_BROWSER_BACK, but it
  // also needs Ctrl pressed).
  GetAppListTestHelper()->GetAppListView()->ResetAccelerators();

  ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
  ui::TestAcceleratorTarget target_back_press;
  controller->Register({accelerator_back_press}, &target_back_press);

  // Register an accelerator that looks for back releases.
  ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ui::TestAcceleratorTarget target_back_release;
  controller->Register({accelerator_back_release}, &target_back_release);

  // Verify that by pressing the back button no event is generated on the press,
  // but there is one generated on the release.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(back_button()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  generator->ReleaseLeftButton();
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
}

// Tests that the back button does not show a context menu.
TEST_P(BackButtonTest, NoContextMenuOnBackButton) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Enable tablet mode to show the back button. Wait for tablet mode animations
  // to finish in order for the back button to move out from under the
  // home button.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // When hotseat is enabled, the back button is only usable in in-app shelf.
  if (GetParam())
    std::unique_ptr<views::Widget> widget = CreateTestWidget();

  // Wait for the navigation widget's animation.
  ShelfNavigationWidget::TestApi navigation_widget_test_api(
      GetPrimaryShelf()->navigation_widget());
  test_api()->RunMessageLoopUntilAnimationsDone(
      navigation_widget_test_api.GetBoundsAnimator());

  generator->MoveMouseTo(back_button()->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  EXPECT_FALSE(test_api_->CloseMenu());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackButtonVisibilityWithAccessibilityFeaturesTest,
    ::testing::Values(
        TestAccessibilityFeature::kTabletModeShelfNavigationButtons,
        TestAccessibilityFeature::kSpokenFeedback,
        TestAccessibilityFeature::kAutoclick,
        TestAccessibilityFeature::kSwitchAccess));

TEST_P(BackButtonVisibilityWithAccessibilityFeaturesTest,
       TabletModeSwitchWithA11yFeatureEnabled) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  SetTestA11yFeatureEnabled(true /*enabled*/);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  // Back button is not shown in clamshell.
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // Switch to tablet mode, and verify the back button is now visible.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(test_api.IsBackButtonVisible());

  // The button should be hidden if the feature gets disabled.
  SetTestA11yFeatureEnabled(false /*enabled*/);
  EXPECT_FALSE(test_api.IsBackButtonVisible());
}

TEST_P(BackButtonVisibilityWithAccessibilityFeaturesTest,
       FeatureEnabledWhileInTabletMode) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  // Back button is not shown in clamshell.
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // Switch to tablet mode, and verify the back button is still hidden.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // The button should be shown if the feature gets enabled.
  SetTestA11yFeatureEnabled(true /*enabled*/);
  EXPECT_TRUE(test_api.IsBackButtonVisible());
}

}  // namespace ash
