// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <utility>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {
namespace {

class AppListBubbleAppsPageTest : public AshTestBase {
 public:
  void OnReorderAnimationDone(base::OnceClosure closure,
                              bool aborted,
                              AppListReorderAnimationStatus status) {
    EXPECT_FALSE(aborted);
    EXPECT_EQ(AppListReorderAnimationStatus::kFadeInAnimation, status);
    std::move(closure).Run();
  }

 private:
  base::test::ScopedFeatureList features_{features::kProductivityLauncher};
};

TEST_F(AppListBubbleAppsPageTest, SlideViewIntoPositionCleansUpLayers) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Recent apps view starts without a layer.
  auto* recent_apps = helper->GetBubbleRecentAppsView();
  ASSERT_FALSE(recent_apps->layer());

  // Trigger a slide animation.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  constexpr int kVerticalOffset = 20;
  constexpr base::TimeDelta kSlideDuration = base::Milliseconds(100);
  helper->StartSlideAnimationOnBubbleAppsPage(recent_apps, kVerticalOffset,
                                              kSlideDuration);
  ASSERT_TRUE(recent_apps->layer());
  EXPECT_TRUE(recent_apps->layer()->GetAnimator()->is_animating());

  // While that animation is running, run another animation.
  helper->StartSlideAnimationOnBubbleAppsPage(recent_apps, kVerticalOffset,
                                              kSlideDuration);
  auto* compositor = recent_apps->layer()->GetCompositor();
  while (recent_apps->layer() &&
         recent_apps->layer()->GetAnimator()->is_animating()) {
    EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  }

  // At the end of the animation, the recent apps layer is still destroyed,
  // even though the layer existed at the start of the second animation.
  EXPECT_FALSE(recent_apps->layer());
}

// Regression test for https://crbug.com/1295794
TEST_F(AppListBubbleAppsPageTest, AppsPageVisibleAfterQuicklyClearingSearch) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_TRUE(apps_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Type a key to trigger the animation to transition to the search page.
  PressAndReleaseKey(ui::VKEY_A);
  ASSERT_TRUE(
      apps_page->GetPageAnimationLayerForTest()->GetAnimator()->is_animating());

  // Before the animation completes, delete the search. This should abort
  // animations, animate back to the apps page, and leave the apps page visible.
  PressAndReleaseKey(ui::VKEY_BACK);
  helper->WaitForLayerAnimation(apps_page->GetPageAnimationLayerForTest());
  EXPECT_TRUE(apps_page->GetVisible());
  EXPECT_EQ(1.0f, apps_page->scroll_view()->contents()->layer()->opacity());
}

TEST_F(AppListBubbleAppsPageTest, AnimateHidePage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_TRUE(apps_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histograms;

  // Type a key to trigger the animation to transition to the search page.
  PressAndReleaseKey(ui::VKEY_A);
  helper->WaitForLayerAnimation(apps_page->GetPageAnimationLayerForTest());

  // Apps page is not visible.
  EXPECT_FALSE(apps_page->GetVisible());

  // Smoothness was recorded.
  histograms.ExpectTotalCount(
      "Apps.ClamshellLauncher.AnimationSmoothness.HideAppsPage", 1);
}

TEST_F(AppListBubbleAppsPageTest, AnimateShowPage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Type a key switch to the search page.
  PressAndReleaseKey(ui::VKEY_A);

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_FALSE(apps_page->GetVisible());

  // Enable animations. NON_ZERO_DURATION does not work here. The animation
  // end callback is not called, for reasons I don't understand. It works fine
  // in production, and in tests with NORMAL_DURATION.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  base::HistogramTester histograms;

  // Press escape to trigger animation back to the apps page.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  helper->WaitForLayerAnimation(apps_page->GetPageAnimationLayerForTest());

  // Apps page is visible.
  EXPECT_TRUE(apps_page->GetVisible());

  // Smoothness was recorded.
  histograms.ExpectTotalCount(
      "Apps.ClamshellLauncher.AnimationSmoothness.ShowAppsPage", 1);
}

TEST_F(AppListBubbleAppsPageTest, ScrollPositionResetOnShow) {
  // Show an app list with enough apps to allow scrolling.
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(50);
  helper->ShowAppList();

  // Press the up arrow, which will scroll the view to select an app in the
  // last row.
  PressAndReleaseKey(ui::VKEY_UP);
  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_GT(apps_page->scroll_view()->vertical_scroll_bar()->GetPosition(), 0);

  // Hide the launcher, then show it again.
  helper->Dismiss();
  helper->ShowAppList();

  // Scroll position is reset to top.
  EXPECT_EQ(apps_page->scroll_view()->vertical_scroll_bar()->GetPosition(), 0);
}

TEST_F(AppListBubbleAppsPageTest, SortAppsMakesA11yAnnouncement) {
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  auto* apps_page = helper->GetBubbleAppsPage();
  views::View* announcement_view = apps_page->toast_container_for_test()
                                       ->a11y_announcer_for_test()
                                       ->announcement_view_for_test();
  ASSERT_TRUE(announcement_view);

  // Add a callback to wait for an accessibility event.
  ax::mojom::Event event = ax::mojom::Event::kNone;
  base::RunLoop run_loop;
  announcement_view->GetViewAccessibility().set_accessibility_events_callback(
      base::BindLambdaForTesting([&](const ui::AXPlatformNodeDelegate* unused,
                                     const ax::mojom::Event event_in) {
        event = event_in;
        run_loop.Quit();
      }));

  // Simulate sorting the apps.
  apps_page->UpdateForNewSortingOrder(AppListSortOrder::kNameAlphabetical,
                                      /*animate=*/false,
                                      /*update_position_closure=*/{});
  run_loop.Run();

  // An alert fired with a message.
  EXPECT_EQ(event, ax::mojom::Event::kAlert);
  ui::AXNodeData node_data;
  announcement_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Apps are sorted by name");
}

class AppListBubbleAppsReorderTest
    : public AppListBubbleAppsPageTest,
      public testing::WithParamInterface</*is_separator_visible=*/bool> {
 public:
  AppListBubbleAppsReorderTest() = default;
  AppListBubbleAppsReorderTest(AppListBubbleAppsReorderTest&) = delete;
  AppListBubbleAppsReorderTest& operator=(const AppListBubbleAppsReorderTest&) =
      delete;
  ~AppListBubbleAppsReorderTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All, AppListBubbleAppsReorderTest, testing::Bool());

// Verifies that after sorting the undo toast should fully show.
TEST_P(AppListBubbleAppsReorderTest, ScrollToShowUndoToastWhenSorting) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show an app list with enough apps to allow scrolling.
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(50);

  const bool is_separator_visible = GetParam();
  if (is_separator_visible)
    GetAppListTestHelper()->AddRecentApps(5);
  helper->ShowAppList();

  auto* apps_page = helper->GetBubbleAppsPage();
  AppListToastContainerView* reorder_undo_toast_container =
      apps_page->toast_container_for_test();

  // Verify the separator's visibility.
  EXPECT_EQ(is_separator_visible,
            apps_page->separator_for_test()->GetVisible());

  // Before sorting, the undo toast should be invisible.
  EXPECT_FALSE(reorder_undo_toast_container->is_toast_visible());

  {
    apps_page->UpdateForNewSortingOrder(
        AppListSortOrder::kNameAlphabetical,
        /*animate=*/true, /*update_position_closure=*/base::DoNothing());

    base::RunLoop run_loop;
    apps_page->scrollable_apps_grid_view()->AddReorderCallbackForTest(
        base::BindRepeating(&AppListBubbleAppsPageTest::OnReorderAnimationDone,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // After sorting, the undo toast should be visible.
  EXPECT_TRUE(reorder_undo_toast_container->is_toast_visible());

  // Scroll the apps page to the end.
  views::ScrollView* scroll_view = apps_page->scroll_view();
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), INT_MAX);

  // Verify that after scrolling the undo toast is not fully visible through
  // the view port.
  gfx::Point origin;
  views::View::ConvertPointToTarget(reorder_undo_toast_container, scroll_view,
                                    &origin);
  gfx::Rect toast_container_bounds_in_scroll_view(
      origin, reorder_undo_toast_container->size());
  EXPECT_FALSE(scroll_view->GetVisibleRect().Contains(
      toast_container_bounds_in_scroll_view));

  {
    apps_page->UpdateForNewSortingOrder(
        AppListSortOrder::kColor,
        /*animate=*/true, /*update_position_closure=*/base::DoNothing());

    base::RunLoop run_loop;
    apps_page->scrollable_apps_grid_view()->AddReorderCallbackForTest(
        base::BindRepeating(&AppListBubbleAppsPageTest::OnReorderAnimationDone,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Verify that after sorting again the undo toast is fully shown.
  origin = gfx::Point();
  views::View::ConvertPointToTarget(reorder_undo_toast_container,
                                    scroll_view->contents(), &origin);
  toast_container_bounds_in_scroll_view =
      gfx::Rect(origin, reorder_undo_toast_container->size());
  EXPECT_TRUE(reorder_undo_toast_container->is_toast_visible());
  EXPECT_TRUE(scroll_view->GetVisibleRect().Contains(
      toast_container_bounds_in_scroll_view));
}

}  // namespace
}  // namespace ash
