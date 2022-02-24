// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <set>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/layer_animation_stopped_waiter.h"
#include "ash/test/test_widget_builder.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

using views::Widget;

namespace ash {
namespace {

// Distance under which two points are considered "near" each other.
constexpr int kNearDistanceDips = 20;

// The exact position of a bubble relative to its anchor is an implementation
// detail, so tests assert that points are "near" each other. This also makes
// the tests less fragile if padding changes.
testing::AssertionResult IsNear(const gfx::Point& a, const gfx::Point& b) {
  gfx::Vector2d delta = a - b;
  float distance = delta.Length();
  if (distance < float{kNearDistanceDips})
    return testing::AssertionSuccess();

  return testing::AssertionFailure()
         << a.ToString() << " is more than " << kNearDistanceDips
         << " dips away from " << b.ToString();
}

gfx::Rect GetShelfBounds() {
  return AshTestBase::GetPrimaryShelf()
      ->shelf_widget()
      ->GetWindowBoundsInScreen();
}

// Returns the number of widgets in the app list container for `display_id`.
size_t NumberOfWidgetsInAppListContainer(int64_t display_id) {
  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  aura::Window* container =
      Shell::GetContainer(root, kShellWindowId_AppListContainer);
  std::set<views::Widget*> widgets;
  views::Widget::GetAllChildWidgets(container, &widgets);
  return widgets.size();
}

class AppListBubblePresenterTest : public AshTestBase {
 public:
  AppListBubblePresenterTest()
      : assistant_test_api_(AssistantTestApi::Create()) {
    scoped_features_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~AppListBubblePresenterTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    // Use a realistic screen size so the default size bubble will fit.
    UpdateDisplay("1366x768");
  }

  // Returns the presenter instance. Use this instead of creating a new
  // presenter instance in each test to avoid situations where two bubbles
  // exist at the same time (the per-test one and the "production" one).
  AppListBubblePresenter* GetBubblePresenter() {
    return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  }

  void AddAppItems(int num_items) {
    GetAppListTestHelper()->AddAppItems(num_items);
    auto* presenter = GetBubblePresenter();
    if (presenter->bubble_widget_for_test()) {
      // Widget is cached between shows, so adding apps may require layout.
      presenter->bubble_widget_for_test()->LayoutRootViewIfNecessary();
    }
  }

  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<AssistantTestApi> assistant_test_api_;
};

TEST_F(AppListBubblePresenterTest, ShowOpensOneWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ShowStartsZeroStateSearch) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_EQ(1, GetTestAppListClient()->start_zero_state_search_count());

  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_EQ(2, GetTestAppListClient()->start_zero_state_search_count());
}

TEST_F(AppListBubblePresenterTest, ShowRecordsCreationTimeHistogram) {
  base::HistogramTester histogram_tester;
  AppListBubblePresenter* presenter = GetBubblePresenter();

  presenter->Show(GetPrimaryDisplay().id());
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleCreationTime", 1);

  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  // The widget is cached, so the metric was not recorded again.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleCreationTime", 1);
}

TEST_F(AppListBubblePresenterTest, ShowOnSecondaryDisplay) {
  UpdateDisplay("1600x1200,1366x768");

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  presenter->Show(GetSecondaryDisplay().id());
  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ToggleWithHomeButtonOnSecondaryDisplay) {
  // Set up 2 displays.
  UpdateDisplay("1366x768,1920x1080");

  // Show and hide the widget on the primary display. This forces it to be
  // cached.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();
  ASSERT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));

  // Click the home button on the secondary display.
  aura::Window* root =
      Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id());
  HomeButton* home_button =
      Shelf::ForWindow(root)->navigation_widget()->GetHomeButton();
  LeftClickOn(home_button);

  // Widget is shown.
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));

  // Click the home button again.
  LeftClickOn(home_button);

  // Widget is hidden.
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ShowAfterDisconnectingDisplay) {
  // Set up 2 displays.
  UpdateDisplay("1366x768,1920x1080");

  // Show and hide the widget on the secondary display. This forces it to be
  // cached.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetSecondaryDisplay().id());
  presenter->Dismiss();
  ASSERT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));

  // Disconnect the secondary monitor.
  UpdateDisplay("1366x768");

  // Show the widget on the primary display.
  presenter->Show(GetPrimaryDisplay().id());

  // Widget is shown (and no crash).
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, DismissHidesWidget) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  presenter->Dismiss();

  // The widget still exists. It was cached for performance.
  views::Widget* widget = presenter->bubble_widget_for_test();
  ASSERT_TRUE(widget);
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(AppListBubblePresenterTest, DismissWhenNotShowingDoesNotCrash) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_FALSE(presenter->IsShowing());

  presenter->Dismiss();
  // No crash.
}

TEST_F(AppListBubblePresenterTest, ToggleOpensOneWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Toggle(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ToggleHidesWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Toggle(GetPrimaryDisplay().id());
  ASSERT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));

  presenter->Toggle(GetPrimaryDisplay().id());

  views::Widget* widget = presenter->bubble_widget_for_test();
  ASSERT_TRUE(widget);
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(AppListBubblePresenterTest, BubbleIsNotShowingByDefault) {
  AppListBubblePresenter* presenter = GetBubblePresenter();

  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, BubbleIsShowingAfterShow) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, BubbleIsNotShowingAfterDismiss) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, CanShowWhileAnimatingClosed) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  presenter->Dismiss();
  // Widget is not considered showing because it is animating closed.
  EXPECT_FALSE(presenter->IsShowing());
  // Widget is still visible because the animation is still playing.
  EXPECT_TRUE(presenter->bubble_widget_for_test()->IsVisible());

  // Attempt to abort the dismiss by showing again.
  presenter->Show(GetPrimaryDisplay().id());

  // Widget shows.
  EXPECT_TRUE(presenter->IsShowing());
}

TEST_F(AppListBubblePresenterTest, DismissWhileWaitingForZeroStateSearch) {
  // Simulate production behavior for animations and zero-state search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_EQ(1, GetTestAppListClient()->start_zero_state_search_count());

  // Dismiss while the code is waiting for the zero-state results. The widget
  // is not created.
  presenter->Dismiss();
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->bubble_widget_for_test());

  // Show and wait for the show to finish.
  presenter->Show(GetPrimaryDisplay().id());
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/true);
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(presenter->bubble_widget_for_test());
}

TEST_F(AppListBubblePresenterTest, DismissOnFocusLoss) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Creating a window in these containers should not dismiss the launcher.
  for (int id : kContainersThatWontHideAppListOnFocus) {
    std::unique_ptr<views::Widget> widget = CreateTestWidget(nullptr, id);
    EXPECT_TRUE(presenter->IsShowing());
  }

  // Creating a window in the default window container dismisses the launcher.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  EXPECT_FALSE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1275755
TEST_F(AppListBubblePresenterTest, AssistantKeyOpensToAssistantPage) {
  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_TRUE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, AssistantKeyOpensAssistantPageWhenCached) {
  // Show and hide the widget to force it to be cached.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_TRUE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, AppsPageVisibleAfterShowingAssistant) {
  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  // Show the assistant.
  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Hide the assistant.
  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  base::RunLoop().RunUntilIdle();

  AppListBubblePresenter* presenter = GetBubblePresenter();
  ASSERT_FALSE(presenter->IsShowing());

  // Show the launcher.
  PressAndReleaseKey(ui::VKEY_BROWSER_SEARCH);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Apps page is visible, even though it was hidden when showing assistant.
  EXPECT_TRUE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_FALSE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, SearchKeyOpensToAppsPage) {
  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  PressAndReleaseKey(ui::VKEY_LWIN);  // Search key.
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_FALSE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, SearchFieldHasFocusAfterAssistantPageShown) {
  // Search box takes focus on show.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  auto* search_box_view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Switch to assistant page. Search box loses focus.
  presenter->ShowEmbeddedAssistantUI();
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());

  // The widget is still open, but hidden.
  presenter->Dismiss();
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());

  // Focus returns to the main search box on show.
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
}

TEST_F(AppListBubblePresenterTest, DoesNotCrashWhenNativeWidgetDestroyed) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  ASSERT_EQ(1u, container->children().size());
  aura::Window* native_window = container->children()[0];
  delete native_window;
  // No crash.

  // Trigger an event that would normally be handled by the event filter.
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();
  // No crash.
}

TEST_F(AppListBubblePresenterTest, ClickInTopLeftOfScreenClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  ASSERT_FALSE(widget->GetWindowBoundsInScreen().Contains(0, 0));
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(presenter->IsShowing());
}

// Verifies that the launcher does not reopen when it's closed by a click on the
// home button.
TEST_F(AppListBubblePresenterTest, ClickOnHomeButtonClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click the home button.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());

  EXPECT_FALSE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1237264.
TEST_F(AppListBubblePresenterTest, ClickInCornerOfScreenClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click the bottom left corner of the screen.
  GetEventGenerator()->MoveMouseTo(GetPrimaryDisplay().bounds().bottom_left());
  GetEventGenerator()->ClickLeftButton();

  // Bubble is closed (and did not reopen).
  EXPECT_FALSE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1268220.
TEST_F(AppListBubblePresenterTest, CreatingActiveWidgetClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Create a new widget, which will activate itself and deactivate the bubble.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();
  EXPECT_TRUE(widget->IsActive());

  // Bubble is closed.
  EXPECT_FALSE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1268220.
TEST_F(AppListBubblePresenterTest, CreatingChildWidgetDoesNotCloseBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Create a new widget parented to the bubble, similar to an app uninstall
  // confirmation dialog.
  aura::Window* bubble_window =
      presenter->bubble_widget_for_test()->GetNativeWindow();
  std::unique_ptr<views::Widget> widget = TestWidgetBuilder()
                                              .SetShow(true)
                                              .SetParent(bubble_window)
                                              .BuildOwnsNativeWidget();

  // Bubble stays open.
  EXPECT_TRUE(presenter->IsShowing());

  // Close the widget.
  widget.reset();

  // Bubble stays open.
  EXPECT_TRUE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1285443.
TEST_F(AppListBubblePresenterTest, CanOpenBubbleThenOpenSystemTray) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a widget, which will activate itself when the launcher closes.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();

  // Show the launcher.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click on the system tray.
  LeftClickOn(GetPrimaryUnifiedSystemTray());

  // Wait for launcher animations to end.
  LayerAnimationStoppedWaiter().Wait(
      presenter->bubble_view_for_test()->layer());

  // Launcher is closed and system tray is open.
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

TEST_F(AppListBubblePresenterTest, BubbleOpensInBottomLeftForBottomShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetPrimaryDisplay().work_area().bottom_left()));
}

TEST_F(AppListBubblePresenterTest, BubbleOpensInTopLeftForLeftShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().origin(),
                     GetPrimaryDisplay().work_area().origin()));
}

TEST_F(AppListBubblePresenterTest, BubbleOpensInTopRightForRightShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().top_right(),
                     GetPrimaryDisplay().work_area().top_right()));
}

TEST_F(AppListBubblePresenterTest, BubbleOpensInBottomRightForBottomShelfRTL) {
  base::test::ScopedRestoreICUDefaultLocale locale("he");
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_right(),
                     GetPrimaryDisplay().work_area().bottom_right()));
}

// Regression test for https://crbug.com/1263697
TEST_F(AppListBubblePresenterTest,
       BubbleStaysInBottomLeftAfterScreenResolutionChange) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Changing to a large display keeps the bubble in the corner.
  UpdateDisplay("2100x2000");
  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetPrimaryDisplay().work_area().bottom_left()));

  // Changing to a small display keeps the bubble in the corner.
  UpdateDisplay("800x600");
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetPrimaryDisplay().work_area().bottom_left()));
}

TEST_F(AppListBubblePresenterTest, BubbleSizedForDisplay) {
  const int default_bubble_height = 688;
  UpdateDisplay("800x900");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  views::View* client_view = presenter->bubble_view_for_test()->parent();

  // Check that the bubble launcher has the initial default bounds.
  EXPECT_EQ(640, client_view->bounds().width());
  EXPECT_EQ(default_bubble_height, client_view->bounds().height());

  // Check that the space between the top of the bubble launcher and the top of
  // the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());

  // Change the display height to be smaller than 800.
  UpdateDisplay("800x600");
  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  client_view = presenter->bubble_view_for_test()->parent();

  // With a smaller display, check that the space between the top of the
  // bubble launcher and the top of the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());
  // The bubble height should be smaller than the default bubble height.
  EXPECT_LT(client_view->bounds().height(), default_bubble_height);

  // Change the display height so that the work area is slightly smaller than
  // twice the default bubble height.
  UpdateDisplay("800x1470");
  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  client_view = presenter->bubble_view_for_test()->parent();

  // The bubble height should still be the default.
  EXPECT_EQ(client_view->bounds().height(), default_bubble_height);

  // Change the display height so that the work area is slightly bigger than
  // twice the default bubble height. Add apps so the bubble height grows to its
  // maximum possible height.
  UpdateDisplay("800x1490");
  presenter->Dismiss();
  AddAppItems(50);
  presenter->Show(GetPrimaryDisplay().id());
  client_view = presenter->bubble_view_for_test()->parent();

  // The bubble height should be slightly larger than the default bubble height,
  // but less than half the display height.
  EXPECT_GT(client_view->bounds().height(), default_bubble_height);
  EXPECT_LT(client_view->bounds().height(), 1490 / 2);
}

// Test that the AppListBubbleView scales up with more apps on a larger display.
TEST_F(AppListBubblePresenterTest, BubbleSizedForLargeDisplay) {
  UpdateDisplay("2100x2000");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  int no_apps_bubble_view_height = presenter->bubble_view_for_test()->height();

  // Add enough apps to enlarge the bubble view size from its default height.
  presenter->Dismiss();
  AddAppItems(35);
  presenter->Show(GetPrimaryDisplay().id());

  int thirty_five_apps_bubble_view_height =
      presenter->bubble_view_for_test()->height();

  // The AppListBubbleView should be larger after apps have been added to it.
  EXPECT_GT(thirty_five_apps_bubble_view_height, no_apps_bubble_view_height);

  // Add 50 more apps to the app list and reopen.
  presenter->Dismiss();
  AddAppItems(50);
  presenter->Show(GetPrimaryDisplay().id());

  int eighty_apps_bubble_view_height =
      presenter->bubble_view_for_test()->height();

  // With more apps added, the height of the bubble should increase.
  EXPECT_GT(eighty_apps_bubble_view_height,
            thirty_five_apps_bubble_view_height);

  // The bubble height should not be larger than half the display height.
  EXPECT_LE(eighty_apps_bubble_view_height, 1000);

  // The bubble should be contained within the display bounds.
  EXPECT_TRUE(GetPrimaryDisplay().work_area().Contains(
      presenter->bubble_view_for_test()->bounds()));
}

// Tests that the AppListBubbleView is positioned correctly when
// shown with bottom auto-hidden shelf.
TEST_F(AppListBubblePresenterTest, BubblePositionWithBottomAutoHideShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  gfx::Point bubble_view_bottom_left = presenter->bubble_widget_for_test()
                                           ->GetWindowBoundsInScreen()
                                           .bottom_left();

  // The bottom of the AppListBubbleView should be near the top of the shelf and
  // not near the bottom side of the display.
  EXPECT_FALSE(IsNear(bubble_view_bottom_left,
                      GetPrimaryDisplay().bounds().bottom_left()));
  EXPECT_TRUE(IsNear(bubble_view_bottom_left, GetShelfBounds().origin()));
}

// Tests that the AppListBubbleView is positioned correctly when shown with left
// auto-hidden shelf.
TEST_F(AppListBubblePresenterTest, BubblePositionWithLeftAutoHideShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  gfx::Point bubble_view_origin =
      presenter->bubble_widget_for_test()->GetWindowBoundsInScreen().origin();

  // The left of the AppListBubbleView should be near the right of the shelf and
  // not near the left side of the display.
  EXPECT_FALSE(
      IsNear(bubble_view_origin, GetPrimaryDisplay().bounds().origin()));
  EXPECT_TRUE(IsNear(bubble_view_origin, GetShelfBounds().top_right()));
}

// Tests that the AppListBubbleView is positioned correctly when shown with
// right auto-hidden shelf.
TEST_F(AppListBubblePresenterTest, BubblePositionWithRightAutoHideShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  gfx::Point bubble_view_top_right = presenter->bubble_widget_for_test()
                                         ->GetWindowBoundsInScreen()
                                         .top_right();

  // The right of the AppListBubbleView should be near the left of the shelf and
  // not near the right side of the display.
  EXPECT_FALSE(
      IsNear(bubble_view_top_right, GetPrimaryDisplay().bounds().top_right()));
  EXPECT_TRUE(IsNear(bubble_view_top_right, GetShelfBounds().origin()));
}

}  // namespace
}  // namespace ash
