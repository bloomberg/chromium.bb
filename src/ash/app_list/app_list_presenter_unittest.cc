// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/app_list/views/productivity_launcher_search_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr int kAppListBezelMargin = 50;

AppListModel* GetAppListModel() {
  return AppListModelProvider::Get()->model();
}

SearchModel* GetSearchModel() {
  return AppListModelProvider::Get()->search_model();
}

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void SetShelfAlignment(ShelfAlignment alignment) {
  AshTestBase::GetPrimaryShelf()->SetAlignment(alignment);
}

void EnableTabletMode(bool enable) {
  // Avoid |TabletModeController::OnGetSwitchStates| from disabling tablet mode
  // again at the end of |TabletModeController::TabletModeController|.
  base::RunLoop().RunUntilIdle();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);

  // The app list will be shown automatically when tablet mode is enabled (Home
  // launcher flag is enabled). Wait here for the animation complete.
  base::RunLoop().RunUntilIdle();
}

// Generates a fling.
void FlingUpOrDown(ui::test::EventGenerator* generator,
                   AppListView* view,
                   bool up) {
  int offset = up ? -100 : 100;
  gfx::Point start_point = view->GetBoundsInScreen().origin();
  gfx::Point target_point = start_point;
  target_point.Offset(0, offset);

  generator->GestureScrollSequence(start_point, target_point,
                                   base::Milliseconds(10), 2);
}

std::unique_ptr<TestSearchResult> CreateOmniboxSuggestionResult(
    const std::string& result_id) {
  auto suggestion_result = std::make_unique<TestSearchResult>();
  suggestion_result->set_result_id(result_id);
  suggestion_result->set_is_omnibox_search(true);
  suggestion_result->set_display_type(SearchResultDisplayType::kList);
  SearchResultActions actions;
  actions.push_back(SearchResultAction(gfx::ImageSkia(), u"Remove",
                                       true /*visible_on_hover*/));
  suggestion_result->SetActions(actions);

  return suggestion_result;
}

}  // namespace

// This suite used to be called AppListPresenterDelegateTest. It's not called
// AppListPresenterImplTest because that name was already taken. The two test
// suite were not merged in order to maintain git blame history for this file.
class AppListPresenterTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppListPresenterTest() = default;
  AppListPresenterTest(const AppListPresenterTest&) = delete;
  AppListPresenterTest& operator=(const AppListPresenterTest&) = delete;
  ~AppListPresenterTest() override = default;

  // testing::Test:
  void SetUp() override {
    feature_list_.InitWithFeatureState(
        app_list_features::kNewDragSpecInLauncher, std::get<1>(GetParam()));
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  void SetAppListStateAndWait(AppListViewState new_state) {
    GetAppListView()->SetState(new_state);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(new_state);
  }

  // Whether to run the test with mouse or gesture events.
  bool TestMouseEventParam() const { return std::get<0>(GetParam()); }

  // Whether to run the test with mouse or gesture events.
  bool GetTestFullscreen() const { return std::get<0>(GetParam()); }

  gfx::Point GetPointOutsideSearchbox() {
    // Ensures that the point satisfies the following conditions:
    // (1) The point is within AppListView.
    // (2) The point is outside of the search box.
    // (3) The touch event on the point should not be consumed by the handler
    // for back gesture.
    return GetAppListView()
        ->search_box_view()
        ->GetBoundsInScreen()
        .bottom_right();
  }

  gfx::Point GetPointInsideSearchbox() {
    return GetAppListView()
        ->search_box_view()
        ->GetBoundsInScreen()
        .CenterPoint();
  }

  AppListView* GetAppListView() {
    return GetAppListTestHelper()->GetAppListView();
  }

  SearchResultPageView* search_result_page() {
    return GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->search_result_page_view();
  }

  AppsGridView* apps_grid_view() {
    return GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->apps_container_view()
        ->apps_grid_view();
  }

  int GetPeekingHeight() {
    return GetAppListView()->GetHeightForState(AppListViewState::kPeeking);
  }

  void ShowZeroStateSearchInHalfState() {
    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
    GetEventGenerator()->GestureTapAt(GetPointInsideSearchbox());
    GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  }

  SearchResultBaseView* GetSearchResultListViewItemAt(int index) {
    return GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->search_result_page_view()
        ->GetSearchResultListViewForTest()
        ->GetResultViewAt(index);
  }

  void ClickMouseAt(const gfx::Point& point) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(point);
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  }

  void LongPressAt(const gfx::Point& point) {
    ui::TouchEvent long_press(ui::ET_GESTURE_LONG_PRESS, point,
                              base::TimeTicks::Now(),
                              ui::PointerDetails(ui::EventPointerType::kTouch));
    GetEventGenerator()->Dispatch(&long_press);
  }

  views::DialogDelegate* GetSearchResultPageAnchoredDialog() {
    return search_result_page()
        ->anchored_dialog_for_test()
        ->widget()
        ->widget_delegate()
        ->AsDialogDelegate();
  }

  // Verifies the current search result page anchored dialog bounds.
  // The dialog is expected to be positioned horizontally centered within the
  // search box bounds.
  void SanityCheckSearchResultsAnchoredDialogBounds(
      const views::Widget* dialog) {
    auto horizontal_center_offset = [](const gfx::Rect& inner,
                                       const gfx::Rect& outer) -> int {
      return outer.CenterPoint().x() - inner.CenterPoint().x();
    };

    const gfx::Rect dialog_bounds = dialog->GetWindowBoundsInScreen();
    const gfx::Rect search_box_bounds = GetAppListView()
                                            ->search_box_view()
                                            ->GetWidget()
                                            ->GetWindowBoundsInScreen();
    // The dialog should be horizontally centered within the search box.
    EXPECT_EQ(0, horizontal_center_offset(dialog_bounds, search_box_bounds));
    // Verify the confirmation dialog is positioned with the top within search
    // box bounds.
    EXPECT_GT(dialog_bounds.y(), search_box_bounds.y());
    EXPECT_LT(dialog_bounds.y(), search_box_bounds.bottom());
  }

  // Returns the |dialog| vertical offset from the top of the search box bounds.
  int GetSearchResultsAnchoredDialogTopOffset(const views::Widget* dialog) {
    const gfx::Rect dialog_bounds = dialog->GetWindowBoundsInScreen();
    const gfx::Rect search_box_bounds = GetAppListView()
                                            ->search_box_view()
                                            ->GetWidget()
                                            ->GetWindowBoundsInScreen();
    return dialog_bounds.y() - search_box_bounds.y();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests all tablet/clamshell classic/bubble launcher combinations.
class AppListBubbleAndTabletTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AppListBubbleAndTabletTest() = default;
  AppListBubbleAndTabletTest(const AppListBubbleAndTabletTest&) = delete;
  AppListBubbleAndTabletTest& operator=(const AppListBubbleAndTabletTest&) =
      delete;
  ~AppListBubbleAndTabletTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kProductivityLauncher,
                                              productivity_launcher_param());
    AshTestBase::SetUp();

    app_list_test_model_ = std::make_unique<test::AppListTestModel>();
    search_model_ = std::make_unique<SearchModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, app_list_test_model_.get(), search_model_.get());

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  // Whether we should use the ProductivityLauncher flag.
  bool productivity_launcher_param() { return std::get<0>(GetParam()); }

  // Whether we should run the test in tablet mode.
  bool tablet_mode_param() { return std::get<1>(GetParam()); }

  // Bubble launcher is visible in clamshell mode with kProductivityLauncher
  // enabled.
  bool should_show_bubble_launcher() {
    return productivity_launcher_param() && !tablet_mode_param();
  }
  // Zero state be shown in clamshell mode and in tablet mode when bubble
  // launcher is not enabled.
  bool should_show_zero_state_search() {
    return !productivity_launcher_param();
  }

  bool AppListSearchResultPageVisible() {
    return should_show_bubble_launcher()
               ? GetAppListTestHelper()->GetBubbleSearchPage()->GetVisible()
               : GetAppListTestHelper()
                     ->GetAppListView()
                     ->app_list_main_view()
                     ->contents_view()
                     ->search_result_page_view()
                     ->GetVisible();
  }

  void EnsureLauncherShown() {
    auto* helper = GetAppListTestHelper();
    if (should_show_bubble_launcher()) {
      Shell::Get()->app_list_controller()->bubble_presenter_for_test()->Show(
          GetPrimaryDisplay().id());
    } else if (tablet_mode_param()) {
      // App list is always visible in tablet mode so we do not need to show it.
    } else {
      // Show fullscreen so folders are available.
      helper->ShowAndRunLoop(GetPrimaryDisplayId());
      helper->GetAppListView()->SetState(AppListViewState::kFullscreenAllApps);
    }
    if (should_show_bubble_launcher()) {
      apps_grid_view_ = helper->GetScrollableAppsGridView();
    } else {
      apps_grid_view_ = helper->GetRootPagedAppsGridView();
    }
    DCHECK(apps_grid_view_);
  }

  gfx::Point SearchBoxCenterPoint() {
    SearchBoxView* search_box_view =
        should_show_bubble_launcher()
            ? GetAppListTestHelper()->GetBubbleSearchBoxView()
            : GetAppListTestHelper()->GetAppListView()->search_box_view();
    return search_box_view->GetBoundsInScreen().CenterPoint();
  }

  AppListFolderView* folder_view() {
    auto* helper = GetAppListTestHelper();
    return should_show_bubble_launcher() ? helper->GetBubbleFolderView()
                                         : helper->GetFullscreenFolderView();
  }

  bool AppListIsInFolderView() {
    return GetAppListTestHelper()->IsInFolderView();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
  AppsGridView* apps_grid_view_ = nullptr;
};

// Used to test app_list behavior with a populated apps_grid
class PopulatedAppListTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  PopulatedAppListTest() = default;
  ~PopulatedAppListTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatureState(
        app_list_features::kNewDragSpecInLauncher, GetParam());
    AppListConfigProvider::Get().ResetForTesting();
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    app_list_test_model_ = std::make_unique<test::AppListTestModel>();
    search_model_ = std::make_unique<SearchModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, app_list_test_model_.get(), search_model_.get());
  }

 protected:
  void OpenAppListInFullscreen() {
    AppListPresenterImpl* presenter =
        Shell::Get()->app_list_controller()->presenter();
    presenter->Show(AppListViewState::kFullscreenAllApps,
                    GetPrimaryDisplay().id(), base::TimeTicks::Now(),
                    /*show_source=*/absl::nullopt);
    app_list_view_ = presenter->GetView();
  }

  void InitializeAppsGrid() {
    if (!app_list_view_)
      OpenAppListInFullscreen();
    apps_grid_view_ = app_list_view_->app_list_main_view()
                          ->contents_view()
                          ->apps_container_view()
                          ->apps_grid_view();
    apps_grid_test_api_ =
        std::make_unique<test::AppsGridViewTestApi>(apps_grid_view_);
  }

  void PopulateApps(int n) {
    app_list_test_model_->PopulateApps(n);
    app_list_view_->GetWidget()->LayoutRootViewIfNecessary();
  }

  AppListFolderItem* CreateAndPopulateFolderWithApps(int n) {
    auto* folder = app_list_test_model_->CreateAndPopulateFolderWithApps(n);
    app_list_view_->GetWidget()->LayoutRootViewIfNecessary();
    return folder;
  }

  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) const {
    DCHECK_GT(app_list_test_model_->top_level_item_list()->item_count(), 0u);
    return apps_grid_test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  bool AppListIsInFolderView() const {
    return app_list_view_->app_list_main_view()
        ->contents_view()
        ->apps_container_view()
        ->IsInFolderView();
  }

  AppListFolderView* folder_view() {
    return app_list_view_->app_list_main_view()
        ->contents_view()
        ->apps_container_view()
        ->app_list_folder_view();
  }

  void UpdateFolderName(const std::string& name) {
    std::u16string folder_name = base::UTF8ToUTF16(name);
    folder_view()->folder_header_view()->SetFolderNameForTest(folder_name);
    folder_view()->folder_header_view()->ContentsChanged(
        folder_view()->folder_header_view()->GetFolderNameViewForTest(),
        folder_name);
  }

  const std::string GetFolderName() {
    return base::UTF16ToUTF8(
        folder_view()->folder_header_view()->GetFolderNameForTest());
  }

  void RefreshFolderName() {
    folder_view()->folder_header_view()->ItemNameChanged();
  }

  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
  std::unique_ptr<test::AppsGridViewTestApi> apps_grid_test_api_;
  AppListView* app_list_view_ = nullptr;         // Owned by native widget.
  PagedAppsGridView* apps_grid_view_ = nullptr;  // Owned by |app_list_view_|.

  base::test::ScopedFeatureList feature_list_;
};

// Subclass of PopuplatedAppListTest which enables the virtual keyboard.
class PopulatedAppListWithVKEnabledTest : public PopulatedAppListTest {
 public:
  PopulatedAppListWithVKEnabledTest() = default;
  ~PopulatedAppListWithVKEnabledTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    PopulatedAppListTest::SetUp();
  }
};

// Instantiate the values in the parameterized tests. First boolean is used to
// toggle mouse and touch events and in some tests to toggle fullscreen mode
// tests. The second one toggles cardfied state feature enabled or disabled.
INSTANTIATE_TEST_SUITE_P(All,
                         AppListPresenterTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Instantiate the Boolean which is used to toggle cardified state in the
// parameterized tests.
INSTANTIATE_TEST_SUITE_P(All, PopulatedAppListTest, testing::Bool());

// Instantiate the Boolean which is used to toggle cardified state in the
// parameterized tests.
INSTANTIATE_TEST_SUITE_P(All,
                         PopulatedAppListWithVKEnabledTest,
                         testing::Bool());

// Instantiate the values in the parameterized tests. First boolean is used to
// determine whether to use the kProductivityLauncher feature flag. The second
// boolean is to determine whether to run the test in tablet mode.
INSTANTIATE_TEST_SUITE_P(All,
                         AppListBubbleAndTabletTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests that Zero State Search is only shown when needed.
TEST_P(AppListBubbleAndTabletTest, LauncherSearchZeroState) {
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Tap Search Box to activate it and check search result view visibility.
  generator->GestureTapAt(SearchBoxCenterPoint());
  EXPECT_EQ(should_show_zero_state_search(), AppListSearchResultPageVisible());

  // Type a character into the textfield and check visibility.
  generator->PressKey(ui::VKEY_A, 0);
  EXPECT_TRUE(AppListSearchResultPageVisible());

  // Delete the character in the textfield and check visibility.
  generator->PressKey(ui::VKEY_BACK, 0);
  EXPECT_EQ(should_show_zero_state_search(), AppListSearchResultPageVisible());
}

// Verifies that context menu click should not activate the search box
// (see https://crbug.com/941428).
TEST_P(AppListPresenterTest, RightClickSearchBoxInPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list_view = GetAppListView();
  gfx::Rect app_list_bounds = app_list_view->GetBoundsInScreen();
  ASSERT_EQ(AppListViewState::kPeeking, app_list_view->app_list_state());

  // Right click the search box and checks the following things:
  // (1) AppListView's bounds in screen does not change.
  // (2) AppListView is still in Peeking state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointInsideSearchbox());
  generator->PressRightButton();
  EXPECT_EQ(app_list_bounds, app_list_view->GetBoundsInScreen());
  EXPECT_EQ(AppListViewState::kPeeking, app_list_view->app_list_state());
}

TEST_P(AppListPresenterTest, ReshownAppListResetsSearchBoxActivation) {
  // Activate the search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetEventGenerator()->GestureTapAt(GetPointInsideSearchbox());

  // Dismiss and re-show the AppList.
  GetAppListTestHelper()->Dismiss();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Test that the search box is no longer active.
  EXPECT_FALSE(GetAppListTestHelper()
                   ->GetAppListView()
                   ->search_box_view()
                   ->is_search_box_active());
}

// Tests that the SearchBox activation is reset after the AppList is hidden with
// no animation from FULLSCREEN_SEARCH.
TEST_P(AppListPresenterTest, SideShelfAppListResetsSearchBoxActivationOnClose) {
  // Set the shelf to one side, then show the AppList and activate the
  // searchbox.
  SetShelfAlignment(ShelfAlignment::kRight);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetEventGenerator()->GestureTapAt(GetPointInsideSearchbox());
  ASSERT_TRUE(GetAppListTestHelper()
                  ->GetAppListView()
                  ->search_box_view()
                  ->is_search_box_active());

  // Dismiss the AppList using the controller, this is the same way we dismiss
  // the AppList when a SearchResult is launched, and skips the
  // FULLSCREEN_SEARCH -> FULLSCREEN_ALL_APPS transition.
  Shell::Get()->app_list_controller()->DismissAppList();

  // Test that the search box is not active.
  EXPECT_FALSE(GetAppListTestHelper()
                   ->GetAppListView()
                   ->search_box_view()
                   ->is_search_box_active());
}

// Verifies that tapping on the search box in tablet mode with animation and
// zero state enabled should not bring Chrome crash (https://crbug.com/958267).
TEST_P(AppListPresenterTest, ClickSearchBoxInTabletMode) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Necessary for AppListView::StateAnimationMetricsReporter::Report being
  // called when animation ends.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Gesture tap on the search box.
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Wait until animation finishes. Verifies AppListView's state.
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Gesture tap on the area out of search box.
  generator->GestureTapAt(GetPointOutsideSearchbox());

  // Wait until animation finishes. Verifies AppListView's state.
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_P(AppListPresenterTest, RemoveSuggestionShowsConfirmDialog) {
  ShowZeroStateSearchInHalfState();

  // Mark the suggested content info as dismissed so that it does not interfere
  // with the layout.
  Shell::Get()->app_list_controller()->MarkSuggestedContentInfoDismissed();
  GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->search_result_page_view()
      ->GetPrivacyContainerViewForTest()
      ->Update();

  // Add a zero state suggestion results - the result that will be tested is in
  // the second place.
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult("Another suggestion"));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId));
  // The result list is updated asynchronously.
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view = GetSearchResultListViewItemAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Make sure the search results page is laid out after adding result action
  // buttons.
  GetAppListView()->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_TRUE(result_view->actions_view());
  EXPECT_EQ(1u, result_view->actions_view()->children().size());
  views::View* const action_view = result_view->actions_view()->children()[0];

  // The remove action button is visible on hover only.
  EXPECT_FALSE(action_view->GetVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(result_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(action_view->GetVisible());

  // Record the current result selection before clicking the remove action
  // button.
  ResultSelectionController* result_selection_controller =
      search_result_page()->result_selection_controller();
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  ResultLocationDetails* result_location =
      result_selection_controller->selected_location_details();

  // Ensure layout after the action view visibility has been updated.
  GetAppListView()->GetWidget()->LayoutRootViewIfNecessary();

  // Click the remove action button, this should surface a confirmation dialog.
  ClickMouseAt(action_view->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndClearInvokedResultActions()
                  .empty());
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  // Cancel the dialog - the app list should remain in the search result page,
  // the suggestion removal dialog should be hidden, and no result action should
  // be invoked.
  GetSearchResultPageAnchoredDialog()->CancelDialog();

  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_FALSE(search_result_page()->anchored_dialog_for_test());
  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndClearInvokedResultActions()
                  .empty());

  // The result selection should be at the same position.
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_EQ(result_location,
            result_selection_controller->selected_location_details());

  // Click remove suggestion action button again.
  ClickMouseAt(action_view->GetBoundsInScreen().CenterPoint());

  // Expect the removal confirmation dialog - this time, accept it.
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());
  GetSearchResultPageAnchoredDialog()->AcceptDialog();

  // The app list should remain showing search results, the dialog should be
  // closed, and result removal action should be invoked.
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_FALSE(search_result_page()->anchored_dialog_for_test());

  // The result selection should be at the same position.
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_EQ(result_location,
            result_selection_controller->selected_location_details());

  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {kTestResultId, SearchResultActionType::kRemove}};
  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      GetAppListTestHelper()
          ->app_list_client()
          ->GetAndClearInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(AppListPresenterTest, RemoveSuggestionUsingLongTap) {
  ShowZeroStateSearchInHalfState();

  // Add a zero state suggestion results - the result that will be tested is in
  // the second place.
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult("Another suggestion"));
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId));
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view = GetSearchResultListViewItemAt(1);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Make sure the search results page is laid out after adding result action
  // buttons.
  GetAppListView()->GetWidget()->LayoutRootViewIfNecessary();

  // Long tap on the search result. This should show the removal confirmation
  // dialog.
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(result_view->selected());
  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndClearInvokedResultActions()
                  .empty());
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  // Cancel the dialog - the app list should remain in the search result page,
  // the suggestion removal dialog should be hidden, and no result action should
  // be invoked.
  GetSearchResultPageAnchoredDialog()->CancelDialog();

  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_FALSE(search_result_page()->anchored_dialog_for_test());
  EXPECT_TRUE(GetAppListTestHelper()
                  ->app_list_client()
                  ->GetAndClearInvokedResultActions()
                  .empty());
  EXPECT_FALSE(result_view->selected());

  // Long tap on the result again.
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());

  // Expect the removal confirmation dialog - this time, accept it.
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());
  GetSearchResultPageAnchoredDialog()->AcceptDialog();

  // The app list should remain showing search results, the dialog should be
  // closed, and result removal action should be invoked.
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_FALSE(search_result_page()->anchored_dialog_for_test());
  EXPECT_FALSE(result_view->selected());

  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {kTestResultId, SearchResultActionType::kRemove}};

  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      GetAppListTestHelper()
          ->app_list_client()
          ->GetAndClearInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(AppListPresenterTest, RemoveSuggestionDialogAnimatesWithAppListView) {
  ShowZeroStateSearchInHalfState();

  // Add a zero state suggestion result.
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId));
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view = GetSearchResultListViewItemAt(0);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Show remove suggestion dialog.
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  views::Widget* const confirmation_dialog =
      search_result_page()->anchored_dialog_for_test()->widget();
  ASSERT_TRUE(confirmation_dialog);

  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog);
  const gfx::Rect initial_dialog_bounds =
      confirmation_dialog->GetWindowBoundsInScreen();

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Transition to fullscreen search state.
  GetAppListView()->SetState(AppListViewState::kFullscreenSearch);
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  EXPECT_NE(confirmation_dialog->GetLayer()->transform(), gfx::Transform());
  EXPECT_EQ(confirmation_dialog->GetLayer()->GetTargetTransform(),
            gfx::Transform());

  // Verify that the dialog position in screen does not change when the
  // animation starts.
  gfx::RectF current_bounds(confirmation_dialog->GetWindowBoundsInScreen());
  confirmation_dialog->GetLayer()->transform().TransformRect(&current_bounds);
  EXPECT_EQ(gfx::RectF(initial_dialog_bounds), current_bounds);
}

TEST_P(AppListPresenterTest,
       RemoveSuggestionDialogBoundsUpdateWithAppListState) {
  ShowZeroStateSearchInHalfState();

  // Add a zero state suggestion result.
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId));
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListView()->GetWidget()->LayoutRootViewIfNecessary();

  SearchResultBaseView* result_view = GetSearchResultListViewItemAt(0);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Show the remove suggestion dialog.
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  views::Widget* const confirmation_dialog =
      search_result_page()->anchored_dialog_for_test()->widget();
  ASSERT_TRUE(confirmation_dialog);

  SCOPED_TRACE("Initial confirmation dialog bounds");
  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog);
  const int dialog_margin =
      GetSearchResultsAnchoredDialogTopOffset(confirmation_dialog);

  // Transition to fullscreen search state.
  GetAppListView()->SetState(AppListViewState::kFullscreenSearch);
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  // Verify that the confirmation dialog followed the search box widget.
  SCOPED_TRACE("Confirmation dialog bounds after transition");
  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog);
  EXPECT_EQ(dialog_margin,
            GetSearchResultsAnchoredDialogTopOffset(confirmation_dialog));
}

TEST_P(AppListPresenterTest,
       TransitionToAppsContainerClosesRemoveSuggestionDialog) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListView()->SetState(AppListViewState::kFullscreenAllApps);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(GetPointInsideSearchbox());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Add a zero state suggestion result.
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId));
  GetAppListTestHelper()->WaitUntilIdle();

  SearchResultBaseView* result_view = GetSearchResultListViewItemAt(0);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  // Show remove suggestion dialog.
  ui::TouchEvent long_press(
      ui::ET_GESTURE_LONG_PRESS, result_view->GetBoundsInScreen().CenterPoint(),
      base::TimeTicks::Now(), ui::PointerDetails(ui::EventPointerType::kTouch));
  GetEventGenerator()->Dispatch(&long_press);
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  views::Widget* const confirmation_dialog =
      search_result_page()->anchored_dialog_for_test()->widget();
  ASSERT_TRUE(confirmation_dialog);

  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog);

  // Verify that transition to apps page hides the removal confirmation dialog.
  views::test::WidgetDestroyedWaiter widget_close_waiter(confirmation_dialog);
  GetAppListView()->SetState(AppListViewState::kFullscreenAllApps);

  widget_close_waiter.Wait();
}

TEST_P(AppListPresenterTest, RemoveSuggestionDialogBoundsUpdateWhenVKHidden) {
  // Enable virtual keyboard for this test.
  KeyboardController* const keyboard_controller =
      Shell::Get()->keyboard_controller();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);

  ShowZeroStateSearchInHalfState();

  // Add a zero state suggestion result.
  const std::string kTestResultId = "Test suggestion";
  GetSearchModel()->results()->Add(
      CreateOmniboxSuggestionResult(kTestResultId));
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListView()->GetWidget()->LayoutRootViewIfNecessary();

  SearchResultBaseView* result_view = GetSearchResultListViewItemAt(0);
  ASSERT_TRUE(result_view);
  ASSERT_TRUE(result_view->result());
  ASSERT_EQ(kTestResultId, result_view->result()->id());

  auto* const keyboard_ui_controller = keyboard::KeyboardUIController::Get();
  keyboard_ui_controller->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Show remove suggestion dialog.
  LongPressAt(result_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(search_result_page()->anchored_dialog_for_test());

  // The search box should have lost the focus, which should have hidden the
  // keyboard.
  EXPECT_FALSE(keyboard_ui_controller->IsKeyboardVisible());

  // Sanity check the confirmation dialog bounds (hiding the keyboard might have
  // changed the position of the search box - the confirmation dialog should
  // have followed it).
  views::Widget* const confirmation_dialog =
      search_result_page()->anchored_dialog_for_test()->widget();
  SanityCheckSearchResultsAnchoredDialogBounds(confirmation_dialog);

  views::test::WidgetDestroyedWaiter widget_close_waiter(confirmation_dialog);

  // Go to peeking state, and verify the keyboard is not reshown.
  GetAppListView()->SetState(AppListViewState::kPeeking);
  GetAppListTestHelper()->WaitUntilIdle();
  // Exiting the search results page should close the dialog.
  widget_close_waiter.Wait();
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Verifies that the downward mouse drag on AppsGridView's first page should
// be handled by AppList.
TEST_P(PopulatedAppListTest, MouseDragAppsGridViewHandledByAppList) {
  InitializeAppsGrid();
  PopulateApps(2);

  // Calculate the drag start/end points.
  gfx::Point drag_start_point = apps_grid_view_->GetBoundsInScreen().origin();
  gfx::Point target_point = GetPrimaryDisplay().bounds().bottom_left();
  target_point.set_x(drag_start_point.x());

  // Drag AppsGridView downward by mouse. Check the following things:
  // (1) Mouse events are processed by AppsGridView, including mouse press,
  // mouse drag and mouse release.
  // (2) AppList is closed after mouse drag.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(drag_start_point);
  event_generator->DragMouseTo(target_point);
  event_generator->ReleaseLeftButton();

  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

// Verifies that the upward mouse drag on AppsGridView's first page should
// be handled by PaginationController.
TEST_P(PopulatedAppListTest,
       MouseDragAppsGridViewHandledByPaginationController) {
  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPage(0) + 1);
  EXPECT_EQ(2, apps_grid_view_->pagination_model()->total_pages());

  // Calculate the drag start/end points. |drag_start_point| is between the
  // first and the second AppListItem. Because in this test case, we want
  // AppsGridView to receive mouse events instead of AppListItemView.
  gfx::Point right_side =
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().right_center();
  gfx::Point left_side =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().left_center();
  ASSERT_EQ(left_side.y(), right_side.y());
  gfx::Point drag_start_point((right_side.x() + left_side.x()) / 2,
                              right_side.y());
  gfx::Point target_point = GetPrimaryDisplay().bounds().top_right();
  target_point.set_x(drag_start_point.x());

  // Drag AppsGridView downward by mouse. Checks that PaginationController
  // records the mouse drag.
  base::HistogramTester histogram_tester;
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(drag_start_point);
  event_generator->DragMouseTo(target_point);
  event_generator->ReleaseLeftButton();
  histogram_tester.ExpectUniqueSample(
      "Apps.AppListPageSwitcherSource.ClamshellMode",
      AppListPageSwitcherSource::kMouseDrag, 1);
}

// Tests that mouse app list item drag is cancelled when mouse capture is lost
// (e.g. on screen rotation).
TEST_P(PopulatedAppListTest, CancelItemDragOnMouseCaptureLoss) {
  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPage(0) + 1);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Start dragging the first item - move it in between items 1 and 2.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();
  event_generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center());
  EXPECT_TRUE(apps_grid_view_->IsDragging());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  // AppListView is usually notified of display bounds changes by
  // AppListPresenter, though the test delegate implementation does not
  // track display metrics changes, so OnParentWindowBoundsChanged() has to be
  // explicitly called here.
  app_list_view_->OnParentWindowBoundsChanged();

  // Verify that mouse drag has been canceled due to mouse capture loss.
  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(2)->item()->id());
}

// Tests that app list item drag gets canceled if the dragged app list item gets
// deleted.
TEST_P(PopulatedAppListTest, CancelItemDragOnDragItemDeletion) {
  InitializeAppsGrid();
  PopulateApps(4);

  // Start dragging a view.
  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();
  event_generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center());
  EXPECT_TRUE(apps_grid_view_->IsDragging());

  // Delete the dragged item.
  app_list_test_model_->DeleteUninstalledItem(dragged_view->item()->id());
  EXPECT_FALSE(apps_grid_view_->IsDragging());

  // Verify that mouse drag has been canceled.
  EXPECT_FALSE(apps_grid_view_->IsDragging());

  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 3", apps_grid_view_->GetItemViewAt(2)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

// Tests that app list item drag in folder gets canceled if the dragged app list
// item gets deleted.
TEST_P(PopulatedAppListTest, CancelFolderItemDragOnDragItemDeletion) {
  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(3);

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());
  event_generator->MoveTouchBy(10, 10);

  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());

  // Delete the dragged item.
  app_list_test_model_->DeleteUninstalledItem(dragged_view->item()->id());

  // Verify that drag has been canceled.
  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 3",
            folder_view()->items_grid_view()->GetItemViewAt(0)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

// Tests that app list item drag from folder to root apps grid gets canceled if
// the dragged app list item gets deleted.
TEST_P(PopulatedAppListTest, CancelFolderItemReparentDragOnDragItemDeletion) {
  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(3);

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());
  event_generator->MoveTouchBy(10, 10);

  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());

  // Drag the item outside the folder bounds.
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
  event_generator->MoveTouchBy(2, 2);

  // Fire reparenting timer.
  EXPECT_TRUE(
      folder_view()->items_grid_view()->FireFolderItemReparentTimerForTest());
  EXPECT_FALSE(AppListIsInFolderView());
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(apps_grid_view_->IsDragging());
  EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());

  // Delete the dragged item.
  app_list_test_model_->DeleteUninstalledItem(dragged_view->item()->id());

  // Verify that drag has been canceled.
  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 5", apps_grid_view_->GetItemViewAt(3)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

TEST_P(PopulatedAppListTest,
       CancelFolderItemReparentDragOnDragItemAndFolderDeletion) {
  InitializeAppsGrid();
  PopulateApps(2);
  CreateAndPopulateFolderWithApps(2);
  PopulateApps(3);

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());
  event_generator->MoveTouchBy(10, 10);

  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());

  // Drag the item outside the folder bounds.
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
  event_generator->MoveTouchBy(2, 2);

  // Fire reparenting timer.
  EXPECT_TRUE(
      folder_view()->items_grid_view()->FireFolderItemReparentTimerForTest());
  EXPECT_FALSE(AppListIsInFolderView());
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(apps_grid_view_->IsDragging());
  EXPECT_TRUE(folder_view()->items_grid_view()->IsDragging());

  // Delete the dragged item.
  app_list_test_model_->DeleteUninstalledItem(dragged_view->item()->id());

  // Verify that drag has been canceled.
  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 3", apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 4", apps_grid_view_->GetItemViewAt(3)->item()->id());

  // Hide and show the app list again to verify checks done when resetting the
  // apps grid for show pass (e.g. verification that size of the app list views
  // model matches the size of app list data model).
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetPrimaryDisplay().id());
  helper->DismissAndRunLoop();
}

// Tests that apps grid item layers are not destroyed immediately after item
// drag ends.
TEST_P(PopulatedAppListTest,
       ItemLayersNotDestroyedDuringBoundsAnimationAfterDrag) {
  InitializeAppsGrid();
  const int kItemCount = 5;
  PopulateApps(kItemCount);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Drag the first item between items 1 and 2.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();
  event_generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center());

  // Items should have layers during app list item drag.
  for (int i = 0; i < kItemCount; ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
  }

  EXPECT_TRUE(apps_grid_view_->IsDragging());
  event_generator->ReleaseLeftButton();

  // After the drag is released, the item bounds should animate to their final
  // bounds.
  EXPECT_TRUE(apps_grid_view_->IsAnimationRunningForTest());
  for (int i = 0; i < kItemCount; ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
  }

  // Layers should be destroyed once the bounds animation completes.
  apps_grid_view_->CancelAnimationsForTest();
  for (int i = 0; i < kItemCount; ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that apps grid item drag operation can continue normally after display
// rotation (and app list config change).
TEST_P(PopulatedAppListTest, ScreenRotationDuringAppsGridItemDrag) {
  // Set the display dimensions so rotation also changes the app list config.
  UpdateDisplay("1200x600");

  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPage(0) + 1);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Start dragging the first item.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  // AppListView is usually notified of display bounds changes by
  // AppListPresenter, though the test delegate implementation does not
  // track display metrics changes, so OnParentWindowBoundsChanged() has to be
  // explicitly called here.
  app_list_view_->OnParentWindowBoundsChanged();

  gfx::Point target =
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().left_center();
  Shell::GetPrimaryRootWindow()->GetHost()->ConvertPixelsToDIP(&target);

  // End drag at the in between items 1 and 2 - note that these have been
  // translated one slot left to fill in space left by the dragged view, so the
  // expected drop slot is actually slot 1.
  event_generator->MoveTouch(target);
  event_generator->ReleaseTouch();

  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(1)->item()->id());
}

// Tests screen rotation during apps grid item drag where the drag item ends up
// in page-scroll area. Tests that the apps grid page scrolls without a crash,
// and that releasing drag does not change the item position in the model.
TEST_P(PopulatedAppListTest,
       ScreenRotationDuringAppsGridItemDragWithPageScroll) {
  // Set the display dimensions so rotation also changes the app list config.
  UpdateDisplay("1200x600");

  InitializeAppsGrid();
  PopulateApps(apps_grid_test_api_->TilesPerPage(0) + 1);

  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(0);

  // Start dragging the first item.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  // Move the item close to screen edge, so it ends up in area that triggers
  // page scroll after rotation.
  event_generator->MoveTouch(app_list_view_->GetBoundsInScreen().left_center() +
                             gfx::Vector2d(100, 0));

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  // AppListView is usually notified of display bounds changes by
  // AppListPresenter, though the test delegate implementation does not
  // track display metrics changes, so OnParentWindowBoundsChanged() has to be
  // explicitly called here.
  app_list_view_->OnParentWindowBoundsChanged();

  event_generator->MoveTouchBy(0, 10);
  EXPECT_TRUE(apps_grid_view_->FirePageFlipTimerForTest());
  event_generator->ReleaseTouch();

  // The model state should not have been changed.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ("Item 2", apps_grid_view_->GetItemViewAt(2)->item()->id());
}

// Tests screen rotation while app list folder item is in progress, and the item
// remains in the folder bounds during the drag.
TEST_P(PopulatedAppListTest, ScreenRotationDuringFolderItemDrag) {
  // Set the display dimensions so rotation also changes the app list config.
  UpdateDisplay("1200x600");

  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(10);

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* const dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  // Drag the item within the folder bounds.
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  // AppListView is usually notified of display bounds changes by
  // AppListPresenter, though the test delegate implementation does not
  // track display metrics changes, so OnParentWindowBoundsChanged() has to be
  // explicitly called here.
  app_list_view_->OnParentWindowBoundsChanged();

  // The current behavior on app list bounds change is to close the active
  // folder, canceling the drag.
  EXPECT_FALSE(AppListIsInFolderView());
  EXPECT_FALSE(apps_grid_view_->IsDragging());
  EXPECT_FALSE(folder_view()->items_grid_view()->IsDragging());

  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ("Item 5", apps_grid_view_->GetItemViewAt(3)->item()->id());
}

// Tests that app list folder item reparenting drag (where a folder item is
// dragged outside the folder bounds, and dropped within the apps grid) can
// continue normally after screen rotation.
TEST_P(PopulatedAppListTest, ScreenRotationDuringAppsGridItemReparentDrag) {
  UpdateDisplay("1200x600");

  InitializeAppsGrid();
  PopulateApps(2);
  AppListFolderItem* folder = CreateAndPopulateFolderWithApps(3);
  PopulateApps(10);

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(2)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  AppListItem* dragged_item = dragged_view->item();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  // Drag the item outside the folder bounds.
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
  event_generator->MoveTouchBy(2, 2);

  // Fire reparenting timer.
  EXPECT_TRUE(
      folder_view()->items_grid_view()->FireFolderItemReparentTimerForTest());
  EXPECT_FALSE(AppListIsInFolderView());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  // AppListView is usually notified of display bounds changes by
  // AppListPresenter, though the test delegate implementation does not
  // track display metrics changes, so OnParentWindowBoundsChanged() has to be
  // explicitly called here.
  app_list_view_->OnParentWindowBoundsChanged();

  gfx::Point target =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().right_center();
  Shell::GetPrimaryRootWindow()->GetHost()->ConvertPixelsToDIP(&target);
  // End drag at the in between items 1 and 2.
  event_generator->MoveTouch(target);
  event_generator->ReleaseTouch();

  // Verify the new item location within the apps grid.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(dragged_item->id(),
            apps_grid_view_->GetItemViewAt(2)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(3)->item()->id());
}

// Tests that app list folder item reparenting drag to another folder.
TEST_P(AppListBubbleAndTabletTest, AppsGridItemReparentToFolderDrag) {
  UpdateDisplay("1200x600");

  app_list_test_model_->PopulateApps(2);
  AppListFolderItem* folder =
      app_list_test_model_->CreateAndPopulateFolderWithApps(3);
  app_list_test_model_->PopulateApps(10);
  EnableTabletMode(tablet_mode_param());
  EnsureLauncherShown();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Tap the folder item to show it.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  AppListItemView* folder_item = apps_grid_view_->GetItemViewAt(2);
  ASSERT_TRUE(folder_item);
  event_generator->GestureTapAt(folder_item->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  ASSERT_TRUE(dragged_view);
  AppListItem* dragged_item = dragged_view->item();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  // Drag the item outside the folder bounds.
  event_generator->MoveTouch(
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().CenterPoint());
  event_generator->MoveTouchBy(2, 2);

  EXPECT_TRUE(
      folder_view()->items_grid_view()->FireFolderItemReparentTimerForTest());
  EXPECT_FALSE(AppListIsInFolderView());

  // Move the pointer over the item 3, and drop the dragged item.
  gfx::Point target =
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(target);
  event_generator->ReleaseTouch();

  // Verify the new item location within the apps grid.
  EXPECT_EQ("Item 0", apps_grid_view_->GetItemViewAt(0)->item()->id());
  EXPECT_EQ("Item 1", apps_grid_view_->GetItemViewAt(1)->item()->id());
  EXPECT_EQ(folder->id(), apps_grid_view_->GetItemViewAt(2)->item()->id());

  EXPECT_TRUE(apps_grid_view_->GetItemViewAt(3)->item()->is_folder());
  EXPECT_EQ(dragged_item->folder_id(),
            apps_grid_view_->GetItemViewAt(3)->item()->id());
}

// Tests that an item can be removed just after creating a folder that contains
// that item. See https://crbug.com/1083942
TEST_P(PopulatedAppListTest, RemoveFolderItemAfterFolderCreation) {
  InitializeAppsGrid();
  const int kItemCount = 5;
  PopulateApps(kItemCount);

  // Dragging the item with index 4.
  AppListItemView* const dragged_view = apps_grid_view_->GetItemViewAt(4);
  AppListItem* const dragged_item = dragged_view->item();

  // Drag the item on top of the item with index 3.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  dragged_view->FireMouseDragTimerForTest();
  // Move mouse to switch to cardified state -the cardified state starts only
  // once the drag distance exceeds a drag threshold, so the pointer has to
  // sufficiently move from the original position.
  event_generator->MoveMouseBy(10, 10);
  event_generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(apps_grid_view_->IsDragging());

  EXPECT_TRUE(apps_grid_view_->GetItemViewAt(3)->item()->is_folder());
  EXPECT_EQ(dragged_item->folder_id(),
            apps_grid_view_->GetItemViewAt(3)->item()->id());

  // Verify that item layers have been destroyed after the drag operation ended.
  apps_grid_test_api_->WaitForItemMoveAnimationDone();

  for (int i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Open the newly created folder.
  event_generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(3)->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  event_generator->ReleaseLeftButton();

  // Verify that item views have no layers after the folder has been opened.
  apps_grid_test_api_->WaitForItemMoveAnimationDone();
  EXPECT_TRUE(AppListIsInFolderView());
  for (int i = 0; i < apps_grid_view_->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view_->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Verify that a pending layout, if any, does not cause a crash.
  apps_grid_view_->InvalidateLayout();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Remove the original drag view item.
  app_list_test_model_->DeleteUninstalledItem(dragged_item->id());
  apps_grid_test_api_->WaitForItemMoveAnimationDone();

  EXPECT_FALSE(AppListIsInFolderView());
  EXPECT_FALSE(apps_grid_view_->GetItemViewAt(3)->item()->is_folder());

  // Verify that a pending layout, if any, does not cause a crash.
  apps_grid_view_->InvalidateLayout();
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
}

TEST_P(PopulatedAppListWithVKEnabledTest,
       TappingAppsGridClosesVirtualKeyboard) {
  InitializeAppsGrid();
  PopulateApps(2);
  gfx::Point between_apps = GetItemRectOnCurrentPageAt(0, 0).right_center();
  gfx::Point empty_space = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();

  ui::GestureEvent tap_between(between_apps.x(), between_apps.y(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ui::GestureEvent tap_outside(empty_space.x(), empty_space.y(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Touch the apps_grid outside of any apps
  apps_grid_view_->OnGestureEvent(&tap_outside);
  // Expect that the event is ignored here and allowed to propogate to app_list
  EXPECT_FALSE(tap_outside.handled());
  // Hit the app_list with the same event
  app_list_view_->OnGestureEvent(&tap_outside);
  // Expect that the event is handled and the keyboard is closed.
  EXPECT_TRUE(tap_outside.handled());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // Reshow the VKeyboard
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Touch the apps_grid between two apps
  apps_grid_view_->OnGestureEvent(&tap_between);
  // Expect the event to be handled in the grid, and the keyboard to be closed.
  EXPECT_TRUE(tap_between.handled());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Tests that a folder item that is dragged to the page flip area and released
// will discard empty pages in the apps grid. If an empty page is not discarded,
// the apps grid crashes (See http://crbug.com/1100011).
TEST_P(PopulatedAppListTest, FolderItemDroppedRemovesBlankPage) {
  InitializeAppsGrid();
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  PopulateApps(2);
  ASSERT_EQ(1, apps_grid_view_->pagination_model()->total_pages());

  // Tap the folder item to show its contents.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Start dragging the first item in the active folder.
  AppListItemView* dragged_view =
      folder_view()->items_grid_view()->GetItemViewAt(0);
  AppListItem* dragged_item = dragged_view->item();
  event_generator->MoveTouch(dragged_view->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  ASSERT_TRUE(dragged_view->FireTouchDragTimerForTest());

  // Move the pointer over the page flip area in the apps grid. We first fire
  // the folder item reparent timer. The folder view should be hidden.
  const gfx::Rect apps_grid_bounds = apps_grid_view_->GetBoundsInScreen();
  const gfx::Point page_flip_bottom_center =
      gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() + 1);
  event_generator->MoveTouch(page_flip_bottom_center);
  event_generator->MoveTouchBy(0, 5);
  EXPECT_TRUE(
      folder_view()->items_grid_view()->FireFolderItemReparentTimerForTest());
  EXPECT_FALSE(AppListIsInFolderView());

  // Move again to trigger the page flip timer, fire it and finish the page flip
  // animation. There should be 2 pages.
  event_generator->MoveTouchBy(0, -10);
  EXPECT_TRUE(apps_grid_view_->FirePageFlipTimerForTest());
  apps_grid_view_->pagination_model()->FinishAnimation();
  EXPECT_EQ(2, apps_grid_view_->pagination_model()->total_pages());

  // Drop the item outside of the drag buffer, which should cancel the drag. The
  // dragged app should be still in the folder, and the  newly blank page should
  // be discarded without crashing.
  event_generator->MoveTouch(apps_grid_bounds.bottom_left() +
                             gfx::Vector2d(-100, 0));
  event_generator->ReleaseTouch();
  EXPECT_EQ(1, apps_grid_view_->pagination_model()->total_pages());
  EXPECT_EQ(folder_item->id(), dragged_item->folder_id());
}

// Tests that app list hides when focus moves to a normal window.
TEST_P(AppListPresenterTest, HideOnFocusOut) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that app list remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_P(AppListPresenterTest, RemainVisibleWhenFocusingToApplistContainer) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests opening the app list on a secondary display, then deleting the display.
TEST_P(AppListPresenterTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  ASSERT_EQ("1024,0 1024x768", root_windows[1]->GetBoundsInScreen().ToString());

  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests updating display should not close the app list.
TEST_P(AppListPresenterTest, UpdateDisplayNotCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Change display bounds.
  UpdateDisplay("1024x768");

  // Updating the display should not close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests the app list window's bounds under multi-displays environment.
TEST_P(AppListPresenterTest, AppListWindowBounds) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");
  const gfx::Size display_size(1024, 768);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Test the app list window's bounds on primary display.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect primary_display_rect(
      gfx::Point(0, display_size.height() - GetPeekingHeight()), display_size);
  EXPECT_EQ(
      primary_display_rect,
      GetAppListView()->GetWidget()->GetNativeView()->GetBoundsInScreen());

  // Close the app list on primary display.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);

  // Test the app list window's bounds on secondary display.
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect secondary_display_rect(
      gfx::Point(display_size.width(),
                 display_size.height() - GetPeekingHeight()),
      display_size);
  EXPECT_EQ(
      secondary_display_rect,
      GetAppListView()->GetWidget()->GetNativeView()->GetBoundsInScreen());
}

// Tests that the app list window's bounds and the search box bounds are updated
// when the display bounds change.
TEST_P(AppListPresenterTest, AppListBoundsChangeForDisplayChange) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the half
// state are updated when the display bounds change.
TEST_P(AppListPresenterTest, AppListBoundsChangeForDisplayChangeSearch) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kHalf);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the
// fullscreen state are updated when the display bounds change.
TEST_P(AppListPresenterTest, AppListBoundsChangeForDisplayChangeFullscreen) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kFullscreenAllApps);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the
// fullscreen search state are updated when the display bounds change.
TEST_P(AppListPresenterTest,
       AppListBoundsChangeForDisplayChangeFullscreenSearch) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kFullscreenAllApps);
  SetAppListStateAndWait(AppListViewState::kFullscreenSearch);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list is not draggable in side shelf alignment.
TEST_P(AppListPresenterTest, SideShelfAlignmentDragDisabled) {
  SetShelfAlignment(ShelfAlignment::kRight);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  const AppListView* app_list = GetAppListView();
  EXPECT_TRUE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Drag the widget across the screen over an arbitrary 100Ms, this would
  // normally result in the app list transitioning to PEEKING but will now
  // result in no state change.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(GetPointOutsideSearchbox(),
                                   gfx::Point(10, 900), base::Milliseconds(100),
                                   10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Tap the app list body. This should still close the app list.
  generator->GestureTapAt(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the app list initializes in fullscreen with side shelf alignment
// and that the state transitions via text input act properly.
TEST_P(AppListPresenterTest, SideShelfAlignmentTextStateTransitions) {
  SetShelfAlignment(ShelfAlignment::kLeft);

  // Open the app list with side shelf alignment, then check that it is in
  // fullscreen mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list = GetAppListView();
  EXPECT_TRUE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Pressing escape should transition the app list should to fullscreen all
  // apps state.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list initializes in peeking with bottom shelf alignment
// and that the state transitions via text input act properly.
TEST_P(AppListPresenterTest, BottomShelfAlignmentTextStateTransitions) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list = GetAppListView();
  EXPECT_FALSE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter text in the searchbox, this should transition the app list to half
  // state.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Empty the searchbox - app list should remain in half state (and show zero
  // state results).
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BACK);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // ESC should transition app list to the peeking state.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
}

// Tests that the app list initializes in fullscreen with tablet mode active
// and that the state transitions via text input act properly.
TEST_P(AppListPresenterTest, TabletModeTextStateTransitions) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Pressing the escape key should transition the app list to the fullscreen
  // all apps state.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list closes when tablet mode deactivates.
TEST_P(AppListPresenterTest, AppListClosesWhenLeavingTabletMode) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

// Tests that the app list state responds correctly to tablet mode being
// enabled while the app list is being shown with half launcher.
TEST_P(AppListPresenterTest, HalfToFullscreenWhenTabletModeIsActive) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter text in the search box to transition to half app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Enable tablet mode and force the app list to transition to the fullscreen
  // equivalent of the current state.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list view handles drag properly in laptop mode.
TEST_P(AppListPresenterTest, AppListViewDragHandler) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  ui::test::EventGenerator* generator = GetEventGenerator();
  // Execute a slow short upwards drag this should fail to transition the app
  // list.
  int top_of_app_list =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().y();
  generator->GestureScrollSequence(gfx::Point(0, top_of_app_list + 20),
                                   gfx::Point(0, top_of_app_list - 20),
                                   base::Milliseconds(500), 50);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Execute a long upwards drag, this should transition the app list.
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10), base::Milliseconds(100),
                                   10);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Execute a short downward drag, this should fail to transition the app list.
  gfx::Point start(10, 10);
  gfx::Point end(10, 100);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Execute a long and slow downward drag to switch to peeking.
  start = gfx::Point(10, 200);
  end = gfx::Point(10, 650);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Transition to fullscreen.
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10), base::Milliseconds(100),
                                   10);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text to transition to |FULLSCREEN_SEARCH|.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Execute a short downward drag, this should fail to close the app list.
  start = gfx::Point(10, 10);
  end = gfx::Point(10, 100);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Execute a long downward drag, this should close the app list.
  generator->GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 900),
                                   base::Milliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the bottom shelf background is hidden when the app list is shown
// in laptop mode.
TEST_P(AppListPresenterTest, ShelfBackgroundIsHiddenWhenAppListIsShown) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::kAppList,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests the shelf background type is as expected when a window is created after
// going to tablet mode.
TEST_P(AppListPresenterTest, ShelfBackgroundWithHomeLauncher) {
  // Enter tablet mode to display the home launcher.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
            shelf_layout_manager->GetShelfBackgroundType());

  // Add a window. It should be in-app because it is in tablet mode.
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests that app list understands shelf rounded corners state while animating
// out and in, and that it keeps getting notified of shelf state changes if
// close animation is interrupted by another show request.
TEST_P(AppListPresenterTest, AppListShownWhileClosing) {
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kMaximizedWithAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  // Enable animation to account for delay between app list starting to close
  // and reporting visibility change (which happens when close animation
  // finishes).
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Dismiss and immediately show the app list (before close animation is done).
  GetAppListTestHelper()->Dismiss();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kMaximizedWithAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Finish app list animations.
  if (GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->is_animating())
    GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->StopAnimating();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kMaximizedWithAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  // Verify that the app list still picks up shelf changes.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_TRUE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kAppList,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests how shelf state is updated as app list state changes with a maximized
// window open. It verifies that the app list knows that the maximized shelf had
// no rounded corners.
TEST_P(AppListPresenterTest, AppListWithMaximizedShelf) {
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kMaximizedWithAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  // Enable animation to account for delay between app list starting to close
  // and reporting visibility change (which happens when close animation
  // finishes).
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start closing the app list view.
  GetAppListTestHelper()->Dismiss();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kMaximizedWithAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  // Minimize the window, and verify that the shelf state changed from a
  // maximized state, and that |shelf_has_rounded_corners()| value was updated.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);

  EXPECT_TRUE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  // Stop app list hide animation.
  ASSERT_TRUE(
      GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->is_animating());
  GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->StopAnimating();

  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Verifies the shelf background state changes when a window is maximized while
// app list is shown. Verifies that AppList::shelf_has_rounded_corners() is
// updated.
TEST_P(AppListPresenterTest, WindowMaximizedWithAppListShown) {
  auto window = CreateTestWindow();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();

  EXPECT_TRUE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::kMaximizedWithAppList,
            shelf_layout_manager->GetShelfBackgroundType());

  GetAppListTestHelper()->Dismiss();

  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests that the bottom shelf is auto hidden when a window is fullscreened in
// tablet mode (home launcher is shown behind).
TEST_P(AppListPresenterTest, ShelfAutoHiddenWhenFullscreen) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);
  Shelf* shelf =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()));
  EXPECT_EQ(ShelfVisibilityState::SHELF_VISIBLE, shelf->GetVisibilityState());

  // Create and fullscreen a window. The shelf should be auto hidden.
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ShelfVisibilityState::SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN,
            shelf->GetAutoHideState());
}

// Tests that the peeking app list closes if the user taps or clicks outside
// its bounds.
TEST_P(AppListPresenterTest, TapAndClickOutsideClosesPeekingAppList) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Tapping outside the bounds closes the app list.
  const gfx::Rect peeking_bounds = GetAppListView()->GetBoundsInScreen();
  gfx::Point tap_point = peeking_bounds.origin();
  tap_point.Offset(10, -10);
  ASSERT_FALSE(peeking_bounds.Contains(tap_point));

  if (test_mouse_event) {
    generator->MoveMouseTo(tap_point);
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(tap_point);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterTest, LongPressOutsideCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point = GetAppListView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch LONG_PRESS to AppListPresenter.
  ui::TouchEvent long_press(ui::ET_GESTURE_LONG_PRESS, outside_point,
                            base::TimeTicks::Now(),
                            ui::PointerDetails(ui::EventPointerType::kTouch));
  GetEventGenerator()->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterTest, TwoFingerTapOutsideCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point = GetAppListView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch TWO_FINGER_TAP to AppListPresenter.
  ui::TouchEvent two_finger_tap(
      ui::ET_GESTURE_TWO_FINGER_TAP, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch));
  GetEventGenerator()->Dispatch(&two_finger_tap);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a keypress activates the searchbox and that clearing the
// searchbox, the searchbox remains active.
TEST_P(AppListPresenterTest, KeyPressEnablesSearchBox) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Press any key, the search box should be active.
  PressAndReleaseKey(ui::VKEY_0);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Delete the text, the search box should be inactive.
  search_box_view->ClearSearch();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

// Tests that a tap/click on the AppListView from half launcher returns the
// AppListView to Peeking, and that a tap/click on the AppListView from
// Peeking closes the app list.
TEST_P(AppListPresenterTest,
       StateTransitionsByTapAndClickingAppListBodyFromHalf) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list_view = GetAppListView();
  SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Press a key, the AppListView should transition to half.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the search box, the AppListView should transition to Peeking
  // and the search box should be inactive.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the search box again, the AppListView should hide.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a tap/click on the AppListView from Fullscreen search returns
// the AppListView to fullscreen all apps, and that a tap/click on the
// AppListView from fullscreen all apps closes the app list.
TEST_P(AppListPresenterTest,
       StateTransitionsByTappingAppListBodyFromFullscreen) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list_view = GetAppListView();
  SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Execute a long upwards drag, this should transition the app list to
  // fullscreen.
  const int top_of_app_list =
      app_list_view->GetWidget()->GetWindowBoundsInScreen().y();
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10), base::Milliseconds(100),
                                   10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Press a key, this should activate the searchbox and transition to
  // fullscreen search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the searchbox, this should deactivate the searchbox and the
  // applistview should return to fullscreen all apps.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the searchbox again, this should close the applistview.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the searchbox activates when it is tapped and that the widget is
// closed after tapping outside the searchbox.
TEST_P(AppListPresenterTest, TapAndClickEnablesSearchBox) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();

  // Tap/Click the search box, it should activate.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointInsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap on the body of the app list, the search box should deactivate.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap on the body of the app list again, the app list should hide.
  if (test_mouse_event) {
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that search box gets deactivated if the active search model gets
// switched.
TEST_P(AppListPresenterTest, SearchBoxDeactivatedOnModelChange) {
  EnableTabletMode(true);

  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();

  // Tap/Click the search box, it should activate.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointInsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Switch the active app list and search model, and verify the search box is
  // deactivated.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get());

  EXPECT_FALSE(search_box_view->is_search_box_active());

  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  Shell::Get()->app_list_controller()->ClearActiveModel();
}

// Tests that search UI gets closed if search model gets changed.
TEST_P(AppListPresenterTest, SearchClearedOnModelChange) {
  EnableTabletMode(true);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();

  // Press a key to start search, and activate the search box.
  PressAndReleaseKey(ui::VKEY_A);

  SearchModel* search_model = GetSearchModel();
  auto test_result = std::make_unique<TestSearchResult>();
  test_result->set_result_id("test");
  test_result->set_display_type(SearchResultDisplayType::kList);
  search_model->results()->Add(std::move(test_result));

  auto test_tile_result = std::make_unique<TestSearchResult>();
  test_tile_result->set_result_id("test_tile");
  test_tile_result->set_display_type(SearchResultDisplayType::kTile);
  search_model->results()->Add(std::move(test_tile_result));

  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  SearchResultContainerView* const tile_item_container =
      search_result_page()->GetSearchResultTileItemListViewForTest();
  ASSERT_EQ(1, tile_item_container->num_results());
  EXPECT_EQ("test_tile",
            tile_item_container->GetResultViewAt(0)->result()->id());

  SearchResultContainerView* item_list_container =
      search_result_page()->GetSearchResultListViewForTest();
  ASSERT_EQ(1, item_list_container->num_results());
  EXPECT_EQ("test", item_list_container->GetResultViewAt(0)->result()->id());

  // Switch the active app list and search model, and verify the search UI gets
  // cleared.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get());

  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Verify that the search UI shows results from the current active search
  // model.
  PressAndReleaseKey(ui::VKEY_A);

  auto test_result_override = std::make_unique<TestSearchResult>();
  test_result_override->set_result_id("test_override");
  test_result_override->set_display_type(SearchResultDisplayType::kList);
  search_model_override->results()->Add(std::move(test_result_override));

  auto test_tile_result_override = std::make_unique<TestSearchResult>();
  test_tile_result_override->set_result_id("test_tile_override");
  test_tile_result_override->set_display_type(SearchResultDisplayType::kTile);
  search_model_override->results()->Add(std::move(test_tile_result_override));

  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  ASSERT_EQ(1, tile_item_container->num_results());
  EXPECT_EQ("test_tile_override",
            tile_item_container->GetResultViewAt(0)->result()->id());

  ASSERT_EQ(1, item_list_container->num_results());
  EXPECT_EQ("test_override",
            item_list_container->GetResultViewAt(0)->result()->id());

  Shell::Get()->app_list_controller()->ClearActiveModel();

  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the result selection will reset after closing the search box by
// clicking somewhere outside the search box.
TEST_P(AppListPresenterTest,
       ClosingSearchBoxByClickingOutsideResetsResultSelection) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  ResultSelectionController* result_selection_controller =
      search_result_page()->result_selection_controller();

  // Mark the suggested content info as dismissed so that it does not interfere
  // with the layout for the selection traversal.
  Shell::Get()->app_list_controller()->MarkSuggestedContentInfoDismissed();

  // Add search results to the search model.
  SearchModel* search_model = GetSearchModel();
  search_model->results()->Add(CreateOmniboxSuggestionResult("Suggestion1"));
  search_model->results()->Add(CreateOmniboxSuggestionResult("Suggestion2"));
  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  // Click the search box, the result selection should be the first one in
  // default.
  ShowZeroStateSearchInHalfState();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_TRUE(result_selection_controller->selected_location_details()
                  ->is_first_result());

  // Move the selection to the second result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_FALSE(result_selection_controller->selected_location_details()
                   ->is_first_result());

  // Tap on the body of the app list, the search box should deactivate.
  if (test_mouse_event) {
    ClickMouseAt(GetPointOutsideSearchbox());
  } else {
    GetEventGenerator()->GestureTapAt(GetPointOutsideSearchbox());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap/Click the search box again, the result selection should be reset to the
  // first one.
  ShowZeroStateSearchInHalfState();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_TRUE(result_selection_controller->selected_location_details()
                  ->is_first_result());
}

// Tests that the result selection will reset after closing the search box by
// clicking the close button.
TEST_P(AppListPresenterTest,
       ClosingSearchBoxByClickingCloseButtonResetsResultSelection) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  ResultSelectionController* result_selection_controller =
      search_result_page()->result_selection_controller();

  // Mark the suggested content info as dismissed so that it does not interfere
  // with the layout for the selection traversal.
  Shell::Get()->app_list_controller()->MarkSuggestedContentInfoDismissed();

  // Add search results to the search model.
  SearchModel* search_model = GetSearchModel();
  search_model->results()->Add(CreateOmniboxSuggestionResult("Suggestion1"));
  search_model->results()->Add(CreateOmniboxSuggestionResult("Suggestion2"));
  // The results are updated asynchronously. Wait until the update is finished.
  base::RunLoop().RunUntilIdle();

  // Click the search box, the result selection should be the first one in
  // default.
  ShowZeroStateSearchInHalfState();

  EXPECT_TRUE(search_box_view->is_search_box_active());
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_TRUE(result_selection_controller->selected_location_details()
                  ->is_first_result());

  // Move the selection to the second result.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_FALSE(result_selection_controller->selected_location_details()
                   ->is_first_result());

  // Use the close button in search_box_view to close the search box.
  const views::View* close_button =
      GetAppListView()->search_box_view()->close_button();
  if (test_mouse_event) {
    ClickMouseAt(close_button->GetBoundsInScreen().CenterPoint());
  } else {
    GetEventGenerator()->GestureTapAt(
        close_button->GetBoundsInScreen().CenterPoint());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap/Click the search box again, the result selection should be reset to the
  // first one.
  if (test_mouse_event) {
    ClickMouseAt(GetPointInsideSearchbox());
  } else {
    GetEventGenerator()->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());
  ASSERT_TRUE(result_selection_controller->selected_result());
  EXPECT_TRUE(result_selection_controller->selected_result()->selected());
  EXPECT_TRUE(result_selection_controller->selected_location_details()
                  ->is_first_result());
}

// Tests that the shelf background displays/hides with bottom shelf
// alignment.
TEST_P(AppListPresenterTest, ShelfBackgroundRespondsToAppListBeingShown) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  // Show the app list, the shelf background should be transparent.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::kAppList,
            shelf_layout_manager->GetShelfBackgroundType());
  GetAppListTestHelper()->DismissAndRunLoop();

  // Set the alignment to the side and show the app list. The background
  // should show.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_FALSE(GetPrimaryShelf()->IsHorizontalAlignment());
  EXPECT_EQ(
      ShelfBackgroundType::kAppList,
      GetPrimaryShelf()->shelf_layout_manager()->GetShelfBackgroundType());
}

// Tests that the half app list closes itself if the user taps outside its
// bounds.
TEST_P(AppListPresenterTest, TapAndClickOutsideClosesHalfAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Transition to half app list by entering text.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // A point outside the bounds of launcher.
  gfx::Point to_point(
      0, GetAppListView()->GetWidget()->GetWindowBoundsInScreen().y() - 1);

  // Clicking/tapping outside the bounds closes the app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(to_point);
    generator->ClickLeftButton();
  } else {
    generator->GestureTapAt(to_point);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the search box is set active with a whitespace query.
TEST_P(AppListPresenterTest, WhitespaceQuery) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  EXPECT_FALSE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter a whitespace query, the searchbox should activate (in zero state).
  PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_TRUE(view->search_box_view()->IsSearchBoxTrimmedQueryEmpty());
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_FALSE(view->search_box_view()->IsSearchBoxTrimmedQueryEmpty());
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Delete the non-whitespace character, the Searchbox should not deactivate.
  PressAndReleaseKey(ui::VKEY_BACK);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_TRUE(view->search_box_view()->IsSearchBoxTrimmedQueryEmpty());
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Delete the whitespace, the search box remains active, in zero state.
  PressAndReleaseKey(ui::VKEY_BACK);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_TRUE(view->search_box_view()->IsSearchBoxTrimmedQueryEmpty());
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
}

// Tests that an unhandled two finger tap/right click does not close the app
// list, and an unhandled one finger tap/left click closes the app list in
// Peeking mode.
TEST_P(AppListPresenterTest, UnhandledEventOnPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Two finger tap or right click in the empty space below the searchbox. The
  // app list should not close.
  gfx::Point empty_space =
      GetAppListView()->search_box_view()->GetBoundsInScreen().bottom_left();
  empty_space.Offset(0, 10);
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(empty_space);
    generator->PressRightButton();
    generator->ReleaseRightButton();
  } else {
    ui::GestureEvent two_finger_tap(
        empty_space.x(), empty_space.y(), 0, base::TimeTicks(),
        ui::GestureEventDetails(ui::ET_GESTURE_TWO_FINGER_TAP));
    generator->Dispatch(&two_finger_tap);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);

  // One finger tap or left click in the empty space below the searchbox. The
  // app list should close.
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(empty_space);
    generator->ClickLeftButton();
  } else {
    generator->GestureTapAt(empty_space);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a drag to the bezel from Fullscreen/Peeking will close the app
// list.
TEST_P(AppListPresenterTest, DragToBezelClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetTestFullscreen();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  }

  // Drag the app list to 50 DIPs from the bottom bezel.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int bezel_y = display::Screen::GetScreen()
                          ->GetDisplayNearestView(view->parent_window())
                          .bounds()
                          .bottom();
  generator->GestureScrollSequence(
      gfx::Point(0, bezel_y - (kAppListBezelMargin + 100)),
      gfx::Point(0, bezel_y - (kAppListBezelMargin)), base::Milliseconds(1500),
      100);

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a drag to the bezel from Fullscreen/Peeking will close the app
// list even on external display with non zero y origin.
TEST_P(AppListPresenterTest,
       DragToBezelClosesAppListFromFullscreenAndPeekingOnExternal) {
  UpdateDisplay("800x600,1000x768");

  const bool test_fullscreen = GetTestFullscreen();
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  AppListView* view = GetAppListView();
  {
    SCOPED_TRACE("Peeking");
    GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  }
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            view->GetWidget()->GetNativeWindow()->GetRootWindow());

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    SCOPED_TRACE("FullscreenAllApps");
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  }

  // Drag the app list to 50 DIPs from the bottom bezel.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(
          view->GetWidget()->GetNativeWindow());
  const int bezel_y = display.bounds().bottom();
  const int drag_x = display.bounds().x() + 10;
  GetEventGenerator()->GestureScrollSequence(
      gfx::Point(drag_x, bezel_y - (kAppListBezelMargin + 100)),
      gfx::Point(drag_x, bezel_y - (kAppListBezelMargin)),
      base::Milliseconds(1500), 100);

  GetAppListTestHelper()->WaitUntilIdle();
  SCOPED_TRACE("Closed");
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Regression test for crash due to use-after-free. https://crbug.com/1163332
TEST_P(AppListPresenterTest, ShouldNotCrashOnItemClickAfterMonitorDisconnect) {
  // Set up two displays.
  UpdateDisplay("1024x768,1200x900");
  AppListModel* model = GetAppListModel();
  model->AddItem(std::make_unique<AppListItem>("item 0"));
  model->AddItem(std::make_unique<AppListItem>("item 1"));

  // Open and close app list on secondary display.
  AppListTestHelper* helper = GetAppListTestHelper();
  helper->ShowAndRunLoop(GetSecondaryDisplay().id());
  helper->DismissAndRunLoop();

  // Open and close app list on primary display.
  helper->ShowAndRunLoop(GetPrimaryDisplayId());
  helper->DismissAndRunLoop();

  // Disconnect secondary display.
  UpdateDisplay("1024x768");

  // Open app list to fullscreen.
  helper->ShowAndRunLoop(GetPrimaryDisplayId());
  helper->GetAppListView()->SetState(AppListViewState::kFullscreenAllApps);
  helper->WaitUntilIdle();

  // Click on an item.
  AppListItemView* item_view = apps_grid_view()->GetItemViewAt(0);
  gfx::Point item_center = item_view->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(item_center);
  generator->ClickLeftButton();

  // No crash. No use-after-free detected by ASAN.
}

// Tests that the app list window's bounds height (from the shelf) in kPeeking
// state is the same whether the app list is shown on the primary display
// or the secondary display fir different display placements.
TEST_P(AppListPresenterTest, AppListPeekingStateHeightOnMultiDisplay) {
  UpdateDisplay("800x1000, 800x600");

  const std::vector<display::DisplayPlacement::Position> placements = {
      display::DisplayPlacement::LEFT, display::DisplayPlacement::RIGHT,
      display::DisplayPlacement::BOTTOM, display::DisplayPlacement::TOP};
  for (const display::DisplayPlacement::Position placement : placements) {
    SCOPED_TRACE(testing::Message() << "Testing placement " << placement);

    GetAppListTestHelper()->CheckVisibility(false);
    Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
        display::test::CreateDisplayLayout(display_manager(), placement, 0));

    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
    GetAppListTestHelper()->CheckVisibility(true);
    SetAppListStateAndWait(AppListViewState::kPeeking);

    views::Widget* app_list_widget = GetAppListView()->GetWidget();
    EXPECT_EQ(Shell::GetAllRootWindows()[0],
              app_list_widget->GetNativeWindow()->GetRootWindow());
    const display::Display primary_display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            app_list_widget->GetNativeWindow());
    const int primary_display_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        primary_display.bounds().bottom();

    GetAppListTestHelper()->Dismiss();
    GetAppListTestHelper()->CheckVisibility(false);
    const int primary_display_closed_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        primary_display.bounds().bottom();

    GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
    GetAppListTestHelper()->CheckVisibility(true);
    SetAppListStateAndWait(AppListViewState::kPeeking);

    app_list_widget = GetAppListView()->GetWidget();
    EXPECT_EQ(Shell::GetAllRootWindows()[1],
              app_list_widget->GetNativeWindow()->GetRootWindow());
    const display::Display secondary_display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            app_list_widget->GetNativeWindow());
    const int secondary_display_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        secondary_display.bounds().bottom();

    EXPECT_EQ(secondary_display_height, primary_display_height);

    GetAppListTestHelper()->Dismiss();
    GetAppListTestHelper()->CheckVisibility(false);

    const int secondary_display_closed_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        secondary_display.bounds().bottom();
    EXPECT_EQ(secondary_display_closed_height, primary_display_closed_height);
  }
}

// Tests that the app list window's bounds height (from the shelf) in kHalf
// state is the same whether the app list is shown on the primary display
// or the secondary display fir different display placements.
TEST_P(AppListPresenterTest, AppListHalfStateHeightOnMultiDisplay) {
  UpdateDisplay("800x1000, 800x600");

  const std::vector<display::DisplayPlacement::Position> placements = {
      display::DisplayPlacement::LEFT, display::DisplayPlacement::RIGHT,
      display::DisplayPlacement::BOTTOM, display::DisplayPlacement::TOP};
  for (const display::DisplayPlacement::Position placement : placements) {
    SCOPED_TRACE(testing::Message() << "Testing placement " << placement);

    GetAppListTestHelper()->CheckVisibility(false);
    Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
        display::test::CreateDisplayLayout(display_manager(), placement, 0));

    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
    GetAppListTestHelper()->CheckVisibility(true);
    SetAppListStateAndWait(AppListViewState::kHalf);

    views::Widget* app_list_widget = GetAppListView()->GetWidget();
    EXPECT_EQ(Shell::GetAllRootWindows()[0],
              app_list_widget->GetNativeWindow()->GetRootWindow());
    const display::Display primary_display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            app_list_widget->GetNativeWindow());
    const int primary_display_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        primary_display.bounds().bottom();

    GetAppListTestHelper()->Dismiss();
    GetAppListTestHelper()->CheckVisibility(false);
    const int primary_display_closed_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        primary_display.bounds().bottom();

    GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
    GetAppListTestHelper()->CheckVisibility(true);
    SetAppListStateAndWait(AppListViewState::kHalf);

    app_list_widget = GetAppListView()->GetWidget();
    EXPECT_EQ(Shell::GetAllRootWindows()[1],
              app_list_widget->GetNativeWindow()->GetRootWindow());
    const display::Display secondary_display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            app_list_widget->GetNativeWindow());
    const int secondary_display_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        secondary_display.bounds().bottom();

    EXPECT_EQ(secondary_display_height, primary_display_height);

    GetAppListTestHelper()->Dismiss();
    GetAppListTestHelper()->CheckVisibility(false);

    const int secondary_display_closed_height =
        app_list_widget->GetWindowBoundsInScreen().y() -
        secondary_display.bounds().bottom();
    EXPECT_EQ(secondary_display_closed_height, primary_display_closed_height);
  }
}

// Tests that a fling from Fullscreen/Peeking closes the app list.
TEST_P(AppListPresenterTest, FlingDownClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetTestFullscreen();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  }

  // Fling down, the app list should close.
  FlingUpOrDown(GetEventGenerator(), view, false /* down */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that drag using a mouse does not always close the app list if the app
// list was previously closed using a fling gesture.
TEST_P(AppListPresenterTest, MouseDragAfterDownwardFliing) {
  const bool test_fullscreen = GetTestFullscreen();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  AppListView* view = GetAppListView();
  const gfx::Point expand_arrow_point = view->app_list_main_view()
                                            ->contents_view()
                                            ->expand_arrow_view()
                                            ->GetBoundsInScreen()
                                            .CenterPoint();

  if (test_fullscreen)
    GetEventGenerator()->GestureTapAt(expand_arrow_point);
  GetAppListTestHelper()->CheckState(test_fullscreen
                                         ? AppListViewState::kFullscreenAllApps
                                         : AppListViewState::kPeeking);

  // Fling down, the app list should close.
  FlingUpOrDown(GetEventGenerator(), view, false /* down */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  // Show the app list again, and perform mouse drag that ends up at the same
  // position.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  if (test_fullscreen)
    GetEventGenerator()->GestureTapAt(expand_arrow_point);
  GetAppListTestHelper()->CheckState(test_fullscreen
                                         ? AppListViewState::kFullscreenAllApps
                                         : AppListViewState::kPeeking);

  GetEventGenerator()->MoveMouseTo(GetPointOutsideSearchbox());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseBy(0, -10);
  GetEventGenerator()->MoveMouseBy(0, 10);
  GetEventGenerator()->ReleaseLeftButton();

  // Verify the app list state has not changed.
  GetAppListTestHelper()->CheckState(test_fullscreen
                                         ? AppListViewState::kFullscreenAllApps
                                         : AppListViewState::kPeeking);
}

TEST_P(AppListPresenterTest,
       MouseWheelFromAppListPresenterImplTransitionsAppListState) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  GetAppListView()->HandleScroll(gfx::Point(0, 0), gfx::Vector2d(0, 30),
                                 ui::ET_MOUSEWHEEL);

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_P(AppListPresenterTest, LongUpwardDragInFullscreenShouldNotClose) {
  const bool test_fullscreen_search = GetTestFullscreen();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  FlingUpOrDown(GetEventGenerator(), view, true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  if (test_fullscreen_search) {
    // Enter a character into the searchbox to transition to FULLSCREEN_SEARCH.
    PressAndReleaseKey(ui::VKEY_0);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  }

  // Drag from the center of the applist to the top of the screen very slowly.
  // This should not trigger a state transition.
  gfx::Point drag_start = view->GetBoundsInScreen().CenterPoint();
  drag_start.set_x(15);
  gfx::Point drag_end = view->GetBoundsInScreen().top_right();
  drag_end.set_x(15);
  GetEventGenerator()->GestureScrollSequence(
      drag_start, drag_end,
      GetEventGenerator()->CalculateScrollDurationForFlingVelocity(
          drag_start, drag_end, 1, 1000),
      1000);
  GetAppListTestHelper()->WaitUntilIdle();
  if (test_fullscreen_search)
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  else
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests closing the app list during drag, and verifies the bounds get properly
// updated when the app list is shown again..
TEST_P(AppListPresenterTest, CloseAppListDuringDrag) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point drag_start = GetAppListView()->GetBoundsInScreen().origin();

  // Start drag and press escape to close the app list view.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(drag_start);
    generator->PressLeftButton();
    generator->MoveMouseBy(0, -10);
  } else {
    generator->MoveTouch(drag_start);
    generator->PressTouch();
    generator->MoveTouch(drag_start + gfx::Vector2d(0, -10));
  }

  EXPECT_TRUE(GetAppListView()->is_in_drag());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  EXPECT_FALSE(GetAppListView()->is_in_drag());

  // Show the app list and verify the app list returns to peeking position.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_EQ(drag_start, GetAppListView()->GetBoundsInScreen().origin());
}

// Tests closing the app list during drag, and verifies that drag updates are
// ignored while the app list is closing.
TEST_P(AppListPresenterTest, DragUpdateWhileAppListClosing) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point drag_start = GetAppListView()->GetBoundsInScreen().origin();

  // Set up non zero animation duration to ensure app list is not closed
  // immediately.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start drag and press escape to close the app list view.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(drag_start);
    generator->PressLeftButton();
    generator->MoveMouseBy(0, -10);
  } else {
    generator->MoveTouch(drag_start);
    generator->PressTouch();
    generator->MoveTouch(drag_start + gfx::Vector2d(0, -10));
  }
  EXPECT_TRUE(GetAppListView()->is_in_drag());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);

  // Update the drag before running the loop that waits for the close animation
  // to finish,
  if (test_mouse_event) {
    generator->MoveMouseBy(0, -10);
  } else {
    generator->MoveTouch(drag_start + gfx::Vector2d(0, -20));
  }

  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  EXPECT_FALSE(GetAppListView()->is_in_drag());

  // Show the app list and verify the app list returns to peeking position.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_EQ(drag_start, GetAppListView()->GetBoundsInScreen().origin());
}

// Tests that a drag can not make the app list smaller than the shelf height.
TEST_P(AppListPresenterTest, LauncherCannotGetSmallerThanShelf) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();

  // Try to place the app list 1 px below the shelf, it should stay at shelf
  // height.
  int target_y = GetPrimaryShelf()
                     ->GetShelfViewForTesting()
                     ->GetBoundsInScreen()
                     .top_right()
                     .y();
  const int expected_app_list_y = target_y;
  target_y += 1;
  view->UpdateYPositionAndOpacity(target_y, 1);

  EXPECT_EQ(expected_app_list_y, view->GetBoundsInScreen().top_right().y());
}

// Tests that the AppListView is on screen on a small display.
TEST_P(AppListPresenterTest, SearchBoxShownOnSmallDisplay) {
  // Update the display to a small scale factor.
  UpdateDisplay("600x400");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Animate to Half.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());

  // Animate to peeking.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());

  // Animate back to Half.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());
}

// Tests that the AppListView is on screen on a small work area.
TEST_P(AppListPresenterTest, SearchBoxShownOnSmallWorkArea) {
  // Update the work area to a small size.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ASSERT_TRUE(display_manager()->UpdateWorkAreaOfDisplay(
      GetPrimaryDisplayId(), gfx::Insets(400, 0, 0, 0)));

  // Animate to Half.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());

  // Animate to peeking.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());

  // Animate back to Half.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());
}

// Tests that no crash occurs after an attempt to show app list in an invalid
// display.
TEST_P(AppListPresenterTest, ShowInInvalidDisplay) {
  GetAppListTestHelper()->ShowAndRunLoop(display::kInvalidDisplayId);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

// Tests that tap the auto-hide shelf with app list opened should dismiss the
// app list but keep shelf visible.
TEST_P(AppListPresenterTest, TapAutoHideShelfWithAppListOpened) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a normal unmaximized window; the shelf should be hidden.
  std::unique_ptr<views::Widget> window = CreateTestWidget();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap the system tray should open system tray bubble and keep shelf visible.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap to dismiss the app list and the auto-hide shelf.
  generator->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  // Show the AppList again.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Make sure the shelf has at least one item.
  ShelfItem item =
      ShelfTestUtil::AddAppShortcut(base::NumberToString(1), TYPE_PINNED_APP);

  // Wait for shelf view's bounds animation to end. Otherwise the scrollable
  // shelf's bounds are not updated yet.
  ShelfView* const shelf_view = shelf->GetShelfViewForTesting();
  ShelfViewTestAPI shelf_view_test_api(shelf_view);
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();

  // Test that tapping the auto-hidden shelf dismisses the app list when tapping
  // part of the shelf that does not contain the apps.
  generator->GestureTapAt(shelf_view->GetBoundsInScreen().left_center() +
                          gfx::Vector2d(10, 0));
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the AppList again.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // App list should remain visible when tapping on a shelf app button.
  ASSERT_TRUE(shelf_view_test_api.GetButton(0));
  generator->GestureTapAt(
      shelf_view_test_api.GetButton(0)->GetBoundsInScreen().CenterPoint());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

TEST_P(AppListPresenterTest, ClickingShelfArrowDoesNotHideAppList) {
  // Add enough shelf items for the shelf to enter overflow.
  Shelf* const shelf = GetPrimaryShelf();
  ScrollableShelfView* const scrollable_shelf_view =
      shelf->hotseat_widget()->scrollable_shelf_view();
  ShelfView* const shelf_view = shelf->GetShelfViewForTesting();
  int index = 0;
  while (scrollable_shelf_view->layout_strategy_for_test() ==
         ScrollableShelfView::kNotShowArrowButtons) {
    ShelfItem item = ShelfTestUtil::AddAppShortcut(
        base::NumberToString(index++), TYPE_PINNED_APP);
  }

  ShelfViewTestAPI shelf_view_test_api(shelf_view);
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click right scrollable shelf arrow - verify the the app list remains
  // visible.
  const views::View* right_arrow = scrollable_shelf_view->right_arrow();
  ASSERT_TRUE(right_arrow->GetVisible());
  GetEventGenerator()->MoveMouseTo(
      right_arrow->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click left button - verify the app list stays visible.
  const views::View* left_arrow = scrollable_shelf_view->left_arrow();
  ASSERT_TRUE(left_arrow->GetVisible());
  GetEventGenerator()->MoveMouseTo(
      left_arrow->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click right of the right arrow - verify the app list gets dismissed.
  ASSERT_TRUE(right_arrow->GetVisible());
  GetEventGenerator()->MoveMouseTo(
      right_arrow->GetBoundsInScreen().right_center() + gfx::Vector2d(10, 0));
  GetEventGenerator()->ClickLeftButton();

  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Verifies that in clamshell mode, AppList has the expected state based on the
// drag distance after dragging from Peeking state.
TEST_P(AppListPresenterTest, DragAppListViewFromPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Calculate |threshold| in the same way with AppListView::EndDrag.
  const int threshold = GetPeekingHeight() / kAppListThresholdDenominator;

  // Drag AppListView downward by |threshold| then release the gesture.
  // Check the final state should be Peeking.
  ui::test::EventGenerator* generator = GetEventGenerator();
  AppListView* view = GetAppListView();
  const int drag_to_peeking_distance = threshold;
  gfx::Point drag_start = view->GetBoundsInScreen().top_center();
  gfx::Point drag_end(drag_start.x(),
                      drag_start.y() + drag_to_peeking_distance);
  generator->GestureScrollSequence(
      drag_start, drag_end,
      generator->CalculateScrollDurationForFlingVelocity(drag_start, drag_end,
                                                         2, 1000),
      1000);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Drag AppListView upward by bigger distance then release the gesture.
  // Check the final state should be kFullscreenAllApps.
  const int drag_to_fullscreen_distance = threshold + 1;
  drag_start = view->GetBoundsInScreen().top_center();
  drag_end =
      gfx::Point(drag_start.x(), drag_start.y() - drag_to_fullscreen_distance);

  generator->GestureScrollSequence(
      drag_start, drag_end,
      generator->CalculateScrollDurationForFlingVelocity(drag_start, drag_end,
                                                         2, 1000),
      1000);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list background corner radius remains constant during app
// list drag if the shelf is not in maximized state.
TEST_P(AppListPresenterTest, BackgroundCornerRadiusDuringDrag) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  const gfx::Point shelf_top = GetPrimaryShelf()
                                   ->GetShelfViewForTesting()
                                   ->GetBoundsInScreen()
                                   .top_center();
  const int background_radius = ShelfConfig::Get()->shelf_size() / 2;

  AppListView* view = GetAppListView();
  const views::View* const background_shield =
      view->GetAppListBackgroundShieldForTest();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Start drag at the peeking height, and move to different
  // positions relative to the shelf top.
  // Verify that the app list background shield never changes.
  const gfx::Point peeking_top = view->GetBoundsInScreen().top_center();
  generator->MoveTouch(peeking_top);
  generator->PressTouch();
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the shelf, with an offset less than the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the top of the shelf.
  generator->MoveTouch(shelf_top);
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to half rounded background radius height.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 2));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height just under the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius - 1));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height that equals the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height just over the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 1));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the peeking height.
  generator->MoveTouch(gfx::Point(peeking_top.x(), peeking_top.y() + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move back to peeking height, and end drag.
  generator->MoveTouch(peeking_top);
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
}

// Tests how app list background rounded corners are changed during a drag while
// the shelf is in a maximized state (i.e. while a maximized window is shown).
TEST_P(AppListPresenterTest,
       BackgroundCornerRadiusDuringDragWithMaximizedShelf) {
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  const gfx::Point shelf_top = GetPrimaryShelf()
                                   ->GetShelfViewForTesting()
                                   ->GetBoundsInScreen()
                                   .top_center();
  const int background_radius = ShelfConfig::Get()->shelf_size() / 2;

  AppListView* view = GetAppListView();
  const views::View* const background_shield =
      view->GetAppListBackgroundShieldForTest();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Start drag at the peeking app list top.
  const gfx::Point peeking_top = view->GetBoundsInScreen().top_center();
  generator->MoveTouch(peeking_top);
  generator->PressTouch();

  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the shelf, with an offset less than the background radius.
  // Verify that current background corner radius matches the offset from the
  // shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 5));
  EXPECT_EQ(
      gfx::RoundedCornersF(background_radius / 5, background_radius / 5, 0, 0),
      background_shield->layer()->rounded_corner_radii());

  // Move to the shelf top - background should have no rounded corners.
  generator->MoveTouch(shelf_top);
  EXPECT_EQ(gfx::RoundedCornersF(),
            background_shield->layer()->rounded_corner_radii());

  // Move to half background radius height - the background corner radius should
  // match the offset from the shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 2));
  EXPECT_EQ(
      gfx::RoundedCornersF(background_radius / 2, background_radius / 2, 0, 0),
      background_shield->layer()->rounded_corner_radii());

  // Move to the height just under the background radius - the current
  // background corners should be equal to the offset from the shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius - 1));
  EXPECT_EQ(
      gfx::RoundedCornersF(background_radius - 1, background_radius - 1, 0, 0),
      background_shield->layer()->rounded_corner_radii());

  // Move to the height that equals the background radius - the current
  // background corners should be equal to the offset from the shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height just over the background radius - the background corner
  // radius value should stay at the |background_radius| value.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 1));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the peeking height - the background radius should remain the
  // same.
  generator->MoveTouch(gfx::Point(peeking_top.x(), peeking_top.y() + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move back to peeking height, and end drag.
  generator->MoveTouch(peeking_top);
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
}

// Tests that the touch selection menu created when tapping an open folder's
// folder name view be interacted with.
TEST_P(PopulatedAppListTest, TouchSelectionMenu) {
  InitializeAppsGrid();

  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(4);
  EXPECT_TRUE(folder_item->is_folder());
  EXPECT_EQ(1u, app_list_test_model_->top_level_item_list()->item_count());
  EXPECT_EQ(
      AppListFolderItem::kItemType,
      app_list_test_model_->top_level_item_list()->item_at(0)->GetItemType());

  // Open the folder.
  ASSERT_FALSE(AppListIsInFolderView());
  GetEventGenerator()->GestureTapAt(
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(AppListIsInFolderView());

  // Check that the touch selection menu runner is not running.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Set the folder name and simulate tap on the folder name view.
  views::View* folder_name_view =
      folder_view()->folder_header_view()->GetFolderNameViewForTest();
  UpdateFolderName("folder_name");
  GetEventGenerator()->GestureTapAt(
      folder_name_view->GetBoundsInScreen().CenterPoint());

  // Fire the timer to show the textfield quick menu.
  views::TextfieldTestApi textfield_test_api(
      folder_view()->folder_header_view()->GetFolderNameViewForTest());
  static_cast<views::TouchSelectionControllerImpl*>(
      textfield_test_api.touch_selection_controller())
      ->ShowQuickMenuImmediatelyForTesting();
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the leftmost button in the touch_menu_container and check that the
  // folder name has been cut.
  views::TouchSelectionMenuRunnerViews::TestApi test_api(
      static_cast<views::TouchSelectionMenuRunnerViews*>(
          ui::TouchSelectionMenuRunner::GetInstance()));
  views::LabelButton* button = test_api.GetFirstButton();
  ASSERT_TRUE(button);
  gfx::Point point = button->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->GestureTapAt(point);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Refresh the folder item name.
  RefreshFolderName();
  // Check folder_name_view's name.
  ASSERT_EQ("", GetFolderName());
}

// Tests how app list is laid out during different state transitions and app
// list drag.
class AppListPresenterLayoutTest : public AppListPresenterTest {
 public:
  AppListPresenterLayoutTest() = default;
  ~AppListPresenterLayoutTest() override = default;

  void SetUp() override {
    AppListPresenterTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kEnableBackgroundBlur);

    UpdateDisplay("1080x900");
    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  }

  int ExpectedSuggestionChipContainerTop(const gfx::Rect& search_box_bounds) {
    return search_box_bounds.bottom() + 16 /*suggesion chip top margin*/;
  }

  // Calculates expected apps grid position based on display height and the
  // search box in-screen bounds.
  // NOTE: This assumes that the display size is such that the preferred apps
  // grid size is within min and max apps grid height (in which case the margin
  // when scalable app list is not enabled is 1 / 16 of the available height).
  int ExpectedAppsGridTop(int display_height,
                          const gfx::Rect& search_box_bounds) {
    return ExpectedSuggestionChipContainerTop(search_box_bounds) +
           32 /*suggestion chip container height*/ +
           24 /*grid fadeout zone height*/ - 16 /*grid fadeout mask height*/;
  }

  // Calculates expected apps grid position on the search results page based on
  // the display height and the search box in-screen bounds.
  int ExpectedAppsGridTopForSearchResults(int display_height,
                                          const gfx::Rect& search_box_bounds) {
    const int top = ExpectedAppsGridTop(display_height, search_box_bounds);
    // In the search results page, the apps grid is shown 24 dip below where
    // they'd be shown in the apps page. The |top| was calculated relative to
    // search box bounds in the search results page, so it has to be further
    // offset by the difference between search box bottom bounds in the apps and
    // search results page.
    const int search_box_diff =
        contents_view()->GetSearchBoxBounds(AppListState::kStateApps).bottom() -
        contents_view()
            ->GetSearchBoxBounds(AppListState::kStateSearchResults)
            .bottom();
    return top + search_box_diff +
           24 /*apps grid offset in fullscreen search state*/;
  }

  ContentsView* contents_view() {
    return GetAppListView()->app_list_main_view()->contents_view();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Instantiate the values in the parameterized tests. First boolean is used to
// toggle mouse and touch events and in some tests to toggle fullscreen mode
// tests. The second one toggles cardfied state feature enabled or disabled.
INSTANTIATE_TEST_SUITE_P(All,
                         AppListPresenterLayoutTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests that the app list contents top margin is gradually updated during drag
// between peeking and fullscreen view state while showing apps page.
TEST_P(AppListPresenterLayoutTest, AppsPagePositionDuringDrag) {
  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int fullscreen_y = 0;
  const int closed_y = 900 - shelf_height;
  const int fullscreen_search_box_padding = (900 - shelf_height) / 16;

  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point peeking_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  // Drag AppListView upwards half way to the top of the screen, and check the
  // search box padding has been updated to a value half-way between peeking and
  // fullscreen values.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(peeking_top);
  generator->PressTouch();
  generator->MoveTouch(
      gfx::Point(peeking_top.x(), (peeking_top.y() + fullscreen_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  float progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagNone);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);

  EXPECT_EQ((peeking_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1,
                    ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                        AppListState::kStateApps),
                    fullscreen_search_box_padding),
            search_box_bounds.y());

  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  // In apps state, search results page should be hidden behind the search
  // box.
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Move to the fullscreen position, and verify the search box padding is
  // equal to the expected fullscreen value.
  generator->MoveTouch(gfx::Point(peeking_top.x(), fullscreen_y));
  GetAppListTestHelper()->WaitUntilIdle();

  EXPECT_EQ(2.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagNone));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(fullscreen_search_box_padding, search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Move half way between peeking and closed state - the search box padding
  // should be half distance between closed and peeking padding.
  generator->MoveTouch(
      gfx::Point(peeking_top.x(), (peeking_top.y() + closed_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagNone);
  EXPECT_LE(std::abs(progress - 0.5f), 0.01f);

  EXPECT_EQ((peeking_top.y() + closed_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress,
                    ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                        AppListState::kStateApps),
                    0),
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Move to the closed state height, and verify the search box padding matches
  // the state.
  generator->MoveTouch(gfx::Point(peeking_top.x(), closed_y));
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(0.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagNone));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(closed_y, search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());
}

// Tests that the app list contents top margin is gradually updated during drag
// between half and fullscreen state while showing search results.
TEST_P(AppListPresenterLayoutTest, SearchResultsPagePositionDuringDrag) {
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter text in the search box to transition to half app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int search_results_height = 440;
  const int fullscreen_y = 0;
  const int closed_y = 900 - shelf_height;
  const int fullscreen_search_box_padding = (900 - shelf_height) / 16;

  const gfx::Point half_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  // Drag AppListView upwards half way to the top of the screen, and check the
  // search box padding has been updated to a value half-way between peeking and
  // fullscreen values.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(half_top);
  generator->PressTouch();
  generator->MoveTouch(
      gfx::Point(half_top.x(), (half_top.y() + fullscreen_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  float progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);

  EXPECT_EQ((half_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1,
                    ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                        AppListState::kStateSearchResults),
                    fullscreen_search_box_padding),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  // Move to the fullscreen position, and verify the search box padding is
  // equal to the expected fullscreen value.
  generator->MoveTouch(gfx::Point(half_top.x(), fullscreen_y));
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(2.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagSearchResults));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(fullscreen_search_box_padding, search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  // Move half way between peeking and closed state - the search box padding
  // should be half distance between closed and peeking padding.
  generator->MoveTouch(gfx::Point(half_top.x(), (half_top.y() + closed_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 0.5f), 0.01f);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ((half_top.y() + closed_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress,
                    ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                        AppListState::kStateSearchResults),
                    0),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  // Move to the closed state height, and verify the search box padding matches
  // the state.
  generator->MoveTouch(gfx::Point(half_top.x(), closed_y));
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(0.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagSearchResults));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(closed_y, search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
}

// Tests changing the active app list page while drag is in progress.
TEST_P(AppListPresenterLayoutTest, SwitchPageDuringDrag) {
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point peeking_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  // Enter text in the search box to transition to half app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  const gfx::Point half_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int search_results_height = 440;
  const int fullscreen_y = 0;
  const int fullscreen_search_box_padding = (900 - shelf_height) / 16;

  // Drag AppListView upwards half way to the top of the screen, and check the
  // search box padding has been updated to a value half-way between peeking and
  // fullscreen values.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(half_top);
  generator->PressTouch();
  generator->MoveTouch(
      gfx::Point(half_top.x(), (half_top.y() + fullscreen_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  float progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);
  EXPECT_EQ((half_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1,
                    ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                        AppListState::kStateSearchResults),
                    fullscreen_search_box_padding),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  const gfx::Rect apps_grid_bounds_in_results_page =
      apps_grid_view()->GetBoundsInScreen();
  const gfx::Rect app_list_bounds = GetAppListView()->GetBoundsInScreen();

  // Press ESC key - this should move the UI back to the app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // The app list position should remain the same.
  EXPECT_EQ(app_list_bounds, GetAppListView()->GetBoundsInScreen());

  // The search box should be moved so drag progress for peeking state matches
  // the current height.
  float new_progress = (0.5 * half_top.y()) / peeking_top.y();
  int expected_search_box_top =
      new_progress * peeking_top.y() +
      (1 - new_progress) * fullscreen_search_box_padding +
      new_progress * ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                         AppListState::kStateApps);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(expected_search_box_top, search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(apps_grid_bounds_in_results_page.y() - 24,
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_EQ(apps_grid_bounds_in_results_page.size(),
            apps_grid_view()->GetBoundsInScreen().size());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Enter text in the search box to transition back to search results page.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);
  EXPECT_EQ((half_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1,
                    ContentsView::GetPeekingSearchBoxTopMarginOnPage(
                        AppListState::kStateSearchResults),
                    fullscreen_search_box_padding),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
}

// Tests changing the active app list page in fullscreen state.
TEST_P(AppListPresenterLayoutTest, SwitchPageInFullscreen) {
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int search_results_height = 440;
  const int fullscreen_y = 0;
  const int fullscreen_search_box_padding = (900 - shelf_height) / 16;

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(fullscreen_y + fullscreen_search_box_padding,
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  const gfx::Rect app_list_bounds = GetAppListView()->GetBoundsInScreen();

  // Enter text in the search box to transition to half app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(app_list_bounds, GetAppListView()->GetBoundsInScreen());
  EXPECT_EQ(fullscreen_y + fullscreen_search_box_padding,
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  EXPECT_EQ(ExpectedAppsGridTopForSearchResults(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  const gfx::Rect apps_grid_bounds_in_results_page =
      apps_grid_view()->GetBoundsInScreen();

  // Press ESC key - this should move the UI back to the app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(app_list_bounds, GetAppListView()->GetBoundsInScreen());
  EXPECT_EQ(fullscreen_y + fullscreen_search_box_padding,
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(apps_grid_bounds_in_results_page.y() - 24,
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_EQ(apps_grid_bounds_in_results_page.size(),
            apps_grid_view()->GetBoundsInScreen().size());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());
}

// Test a variety of behaviors for home launcher (app list in tablet mode).
class AppListPresenterHomeLauncherTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AppListPresenterHomeLauncherTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kEnableBackgroundBlur,
           app_list_features::kNewDragSpecInLauncher},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kEnableBackgroundBlur},
          {app_list_features::kNewDragSpecInLauncher});
    }
  }
  AppListPresenterHomeLauncherTest(const AppListPresenterHomeLauncherTest&) =
      delete;
  AppListPresenterHomeLauncherTest& operator=(
      const AppListPresenterHomeLauncherTest&) = delete;
  ~AppListPresenterHomeLauncherTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    GetAppListTestHelper()->WaitUntilIdle();
    wallpaper_test_api_ = std::make_unique<WallpaperControllerTestApi>(
        Shell::Get()->wallpaper_controller());
  }

  void TearDown() override {
    wallpaper_test_api_.reset();
    AshTestBase::TearDown();
  }

  void TapHomeButton(int64_t display_id) {
    HomeButton* const home_button =
        Shell::GetRootWindowControllerWithDisplayId(display_id)
            ->shelf()
            ->navigation_widget()
            ->GetHomeButton();
    gfx::Point tap_point = home_button->GetBoundsInScreen().CenterPoint();
    GetEventGenerator()->GestureTapDownAndUp(tap_point);
    GetAppListTestHelper()->WaitUntilIdle();
  }

  // Ensures transition to home screen in tablet mode (where home button is not
  // always shown).
  void GoHome() {
    const int64_t primary_display_id = GetPrimaryDisplay().id();
    // If home button is not expected to be shown, use
    // AppListControllerImpl::GoHome() directly, otherwise tap on the primary
    // screen home button.
    if (!Shell::Get()->shelf_config()->shelf_controls_shown()) {
      Shell::Get()->app_list_controller()->GoHome(primary_display_id);
      return;
    }
    TapHomeButton(primary_display_id);
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  AppListView* GetAppListView() {
    return GetAppListTestHelper()->GetAppListView();
  }

  gfx::Point GetPointOutsideSearchbox() {
    // Ensures that the point satisfies the following conditions:
    // (1) The point is within AppListView.
    // (2) The point is outside of the search box.
    // (3) The touch event on the point should not be consumed by the handler
    // for back gesture.
    return GetAppListView()
        ->search_box_view()
        ->GetBoundsInScreen()
        .bottom_right();
  }

  void ShowAppList() {
    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  }

  bool IsAppListVisible() {
    auto* app_list_controller = Shell::Get()->app_list_controller();
    return app_list_controller->IsVisible() &&
           app_list_controller->GetTargetVisibility(absl::nullopt);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<WallpaperControllerTestApi> wallpaper_test_api_;
};

// Instantiate the Boolean which is used to toggle cardified state in the
// parameterized tests.
INSTANTIATE_TEST_SUITE_P(All,
                         AppListPresenterHomeLauncherTest,
                         testing::Bool());

// Verifies that mouse dragging AppListView is enabled.
TEST_P(AppListPresenterHomeLauncherTest, MouseDragAppList) {
  std::unique_ptr<AppListItem> item(new AppListItem("fake id"));
  GetAppListModel()->AddItem(std::move(item));

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Drag AppListView upward by mouse. Before moving the mouse, AppsGridView
  // should be invisible.
  const gfx::Point start_point = GetAppListView()->GetBoundsInScreen().origin();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_point);
  generator->PressLeftButton();
  AppsGridView* apps_grid_view = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->apps_container_view()
                                     ->apps_grid_view();
  EXPECT_FALSE(apps_grid_view->GetVisible());

  // Verifies that the AppListView state after mouse drag should be
  // FullscreenAllApps.
  generator->MoveMouseBy(0, -start_point.y());
  generator->ReleaseLeftButton();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_TRUE(apps_grid_view->GetVisible());
}

// Verifies that mouse dragging AppListView creates layers, causes to change the
// opacity, and destroys the layers when done.
TEST_P(AppListPresenterHomeLauncherTest, MouseDragAppListItemOpacity) {
  const int items_in_page =
      SharedAppListConfig::instance().GetMaxNumOfItemsPerPage();
  for (int i = 0; i < items_in_page; ++i) {
    std::unique_ptr<AppListItem> item(
        new AppListItem(base::StringPrintf("fake id %d", i)));
    GetAppListModel()->AddItem(std::move(item));
  }

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Drag AppListView by mouse. Before moving the mouse, each AppListItem
  // doesn't have its own layer.
  const gfx::Point start_point = GetAppListView()->GetBoundsInScreen().origin();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_point);
  generator->PressLeftButton();
  AppsGridView* apps_grid_view = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->apps_container_view()
                                     ->apps_grid_view();
  // No items have layer.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Drags the mouse a bit above (twice as shelf's height). This should show the
  // item vaguely.
  const int shelf_height =
      GetPrimaryShelf()->GetShelfViewForTesting()->height();
  generator->MoveMouseBy(0, -shelf_height * 2);
  // All of the item should have the layer at this point.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Moves the mouse to the top edge of the screen; now all app-list items are
  // fully visible, but stays to keep layer. The opacity should be almost 1.0.
  generator->MoveMouseTo(start_point.x(), 0);
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Finishes the drag. It should destruct the layer.
  generator->ReleaseLeftButton();
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that ending of the mouse dragging of app-list destroys the layers for
// the items which are in the second page. See https://crbug.com/990529.
TEST_P(AppListPresenterHomeLauncherTest, LayerOnSecondPage) {
  const int items_in_page =
      SharedAppListConfig::instance().GetMaxNumOfItemsPerPage();
  AppListModel* model = GetAppListModel();
  for (int i = 0; i < items_in_page; ++i) {
    std::unique_ptr<AppListItem> item(
        new AppListItem(base::StringPrintf("fake id %02d", i)));
    model->AddItem(std::move(item));
  }

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  const gfx::Point start_point = GetAppListView()->GetBoundsInScreen().origin();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_point);
  generator->PressLeftButton();
  PagedAppsGridView* apps_grid_view = GetAppListView()
                                          ->app_list_main_view()
                                          ->contents_view()
                                          ->apps_container_view()
                                          ->apps_grid_view();

  // Drags the mouse a bit above (twice as shelf's height). This should show the
  // item vaguely.
  const int shelf_height =
      GetPrimaryShelf()->GetShelfViewForTesting()->height();
  generator->MoveMouseBy(0, -shelf_height * 2);
  // All of the item should have the layer at this point.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Add items at the front of the items.
  const int additional_items = 10;
  syncer::StringOrdinal prev_position =
      model->top_level_item_list()->item_at(0)->position();
  for (int i = 0; i < additional_items; ++i) {
    std::unique_ptr<AppListItem> item(
        new AppListItem(base::StringPrintf("fake id %02d", i + items_in_page)));
    // Update the position so that the item is added at the front of the list.
    auto metadata = item->CloneMetadata();
    metadata->position = prev_position.CreateBefore();
    prev_position = metadata->position;
    item->SetMetadata(std::move(metadata));
    model->AddItem(std::move(item));
  }

  generator->MoveMouseBy(0, -1);

  // At this point, some items move out from the first page.
  EXPECT_LT(1, apps_grid_view->pagination_model()->total_pages());

  // The items on the first page should have layers.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Drag to the top of the screen and finish the drag. It should destroy all
  // of the layers, including items on the second page.
  generator->MoveMouseTo(start_point.x(), 0);
  generator->ReleaseLeftButton();
  for (int i = 0; i < apps_grid_view->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that the app list is shown automatically when the tablet mode is on.
// The app list is dismissed when the tablet mode is off.
TEST_P(AppListPresenterHomeLauncherTest, ShowAppListForTabletMode) {
  GetAppListTestHelper()->CheckVisibility(false);

  // Turns on tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Turns off tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the app list window's parent is changed after entering tablet
// mode.
TEST_P(AppListPresenterHomeLauncherTest, ParentWindowContainer) {
  // Show app list in non-tablet mode. The window container should be
  // kShellWindowId_AppListContainer.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  aura::Window* window = GetAppListView()->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_AppListContainer)
                  ->Contains(window));

  // Turn on tablet mode. The window container should be
  // kShellWindowId_HomeScreenContainer.
  EnableTabletMode(true);
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_HomeScreenContainer)
                  ->Contains(window));
}

// Tests that the background opacity change for app list.
TEST_P(AppListPresenterHomeLauncherTest, BackgroundOpacity) {
  // Show app list in non-tablet mode. The background shield opacity should be
  // 70%.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // The opacity should be set on the color, not the layer. Setting opacity on
  // the layer will change the opacity of the blur effect, which is not desired.
  const U8CPU clamshell_background_opacity = static_cast<U8CPU>(255 * 0.8);
  EXPECT_EQ(SkColorSetA(AppListColorProvider::Get()->GetAppListBackgroundColor(
                            /*is_tablet_mode*/
                            false, /*default_color*/ gfx::kGoogleGrey900),
                        clamshell_background_opacity),
            GetAppListView()->GetAppListBackgroundShieldColorForTest());
  EXPECT_EQ(1, GetAppListView()
                   ->GetAppListBackgroundShieldForTest()
                   ->layer()
                   ->opacity());

  // Turn on tablet mode. The background shield should be transparent.
  EnableTabletMode(true);

  const U8CPU tablet_background_opacity = static_cast<U8CPU>(0);
  EXPECT_EQ(SkColorSetA(AppListColorProvider::Get()->GetAppListBackgroundColor(
                            /*is_tablet_mode*/
                            true, /*default_color*/ gfx::kGoogleGrey900),
                        tablet_background_opacity),
            GetAppListView()->GetAppListBackgroundShieldColorForTest());
  EXPECT_EQ(1, GetAppListView()
                   ->GetAppListBackgroundShieldForTest()
                   ->layer()
                   ->opacity());
}

// Tests that the background blur which is present in clamshell mode does not
// show in tablet mode.
TEST_P(AppListPresenterHomeLauncherTest, BackgroundBlur) {
  // Show app list in non-tablet mode. The background blur should be enabled.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_GT(GetAppListView()
                ->GetAppListBackgroundShieldForTest()
                ->layer()
                ->background_blur(),
            0.0f);

  // Turn on tablet mode. The background blur should be disabled.
  EnableTabletMode(true);
  EXPECT_EQ(0.0f, GetAppListView()
                      ->GetAppListBackgroundShieldForTest()
                      ->layer()
                      ->background_blur());
}

// Tests that tapping or clicking on background cannot dismiss the app list.
TEST_P(AppListPresenterHomeLauncherTest, TapOrClickToDismiss) {
  // Show app list in non-tablet mode. Click outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Tap outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Click outside search box.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->PressLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap outside search box.
  generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

TEST_P(AppListPresenterHomeLauncherTest,
       EscapeKeyInNonTabletModeClosesLauncher) {
  ShowAppList();
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsAppListVisible());
}

TEST_P(AppListPresenterHomeLauncherTest, BackKeyInNonTabletModeClosesLauncher) {
  ShowAppList();
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_BACK);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsAppListVisible());
}

TEST_P(AppListPresenterHomeLauncherTest,
       SearchKeyInNonTabletModeClosesLauncher) {
  ShowAppList();
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsAppListVisible());
}

TEST_P(AppListPresenterHomeLauncherTest,
       EscapeKeyInTabletModeDoesNotCloseLauncher) {
  EnableTabletMode(true);
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

TEST_P(AppListPresenterHomeLauncherTest,
       BackKeyInTabletModeDoesNotCloseLauncher) {
  EnableTabletMode(true);
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_BACK);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

TEST_P(AppListPresenterHomeLauncherTest,
       SearchKeyInTabletModeDoesNotCloseLauncher) {
  EnableTabletMode(true);
  EXPECT_TRUE(IsAppListVisible());

  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsAppListVisible());
}

// Tests that moving focus outside app list window can dismiss it.
TEST_P(AppListPresenterHomeLauncherTest, FocusOutToDismiss) {
  // Show app list in non-tablet mode. Move focus to another window.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Go to tablet mode with a focused window, the AppList should not be visible.
  EnableTabletMode(true);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Refocusing the already focused window should change nothing.
  wm::ActivateWindow(window.get());

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Minimizing the focused window with no remaining windows should result in a
  // shown applist.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the gesture-scroll cannot dismiss the app list.
TEST_P(AppListPresenterHomeLauncherTest, GestureScrollToDismiss) {
  // Show app list in non-tablet mode. Fling down.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), false /* up */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Fling down.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), false /* up */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

TEST_P(AppListPresenterHomeLauncherTest,
       MouseScrollUpFromPeekingShowsFullscreenLauncher) {
  // Show app list in non-tablet mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListControllerImpl* app_list = Shell::Get()->app_list_controller();
  EXPECT_EQ(app_list->GetAppListViewState(), AppListViewState::kPeeking);

  // Mouse-scroll up.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();

  // Launcher is fullscreen.
  EXPECT_EQ(app_list->GetAppListViewState(),
            AppListViewState::kFullscreenAllApps);
  EXPECT_TRUE(app_list->IsVisible());
}

TEST_P(AppListPresenterHomeLauncherTest,
       MouseScrollDownFromPeekingClosesLauncher) {
  // Show app list in non-tablet mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListControllerImpl* app_list = Shell::Get()->app_list_controller();
  EXPECT_EQ(app_list->GetAppListViewState(), AppListViewState::kPeeking);

  // Mouse-scroll down.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->MoveMouseWheel(0, -1);
  GetAppListTestHelper()->WaitUntilIdle();

  // Launcher is closed.
  EXPECT_EQ(app_list->GetAppListViewState(), AppListViewState::kClosed);
  EXPECT_FALSE(app_list->IsVisible());
}

// Tests that mouse-scroll up at fullscreen will dismiss app list.
TEST_P(AppListPresenterHomeLauncherTest, MouseScrollToDismissFromFullscreen) {
  // Show app list in non-tablet mode. Mouse-scroll down.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up with mouse wheel to fullscreen.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll down with mouse wheel to close app list.
  generator->MoveMouseWheel(0, -1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Test that the AppListView opacity is reset after it is hidden during the
// overview mode animation.
TEST_P(AppListPresenterHomeLauncherTest, LauncherShowsAfterOverviewMode) {
  // Show the AppList in clamshell mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Enable overview mode.
  EnterOverview();

  // Test that the AppListView is transparent.
  EXPECT_EQ(0.0f, GetAppListView()->GetWidget()->GetLayer()->opacity());

  // Disable overview mode.
  ExitOverview();

  // Show the launcher, test that the opacity is restored.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  EXPECT_EQ(1.0f, GetAppListView()->GetWidget()->GetLayer()->opacity());
  EXPECT_TRUE(GetAppListView()->GetWidget()->IsVisible());
}

// Tests that tapping home button while home screen is visible and showing
// search results moves the home screen to apps container page.
TEST_P(AppListPresenterHomeLauncherTest, HomeButtonDismissesSearchResults) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enable accessibility feature that forces home button to be shown even with
  // kHideShelfControlsInTabletMode enabled.
  // TODO(https://crbug.com/1050544) Use the a11y feature specific to showing
  // navigation buttons in tablet mode once it lands.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Tap home button - verify that home goes back to showing the apps page.
  TapHomeButton(GetPrimaryDisplay().id());

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests the app list opacity in overview mode.
TEST_P(AppListPresenterHomeLauncherTest, OpacityInOverviewMode) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Enable overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ui::Layer* layer = GetAppListView()->GetWidget()->GetNativeWindow()->layer();
  EXPECT_EQ(0.0f, layer->opacity());

  // Disable overview mode.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1.0f, layer->opacity());
}

TEST_P(AppListPresenterHomeLauncherTest, AppListHiddenDuringWallpaperPreview) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterHomeLauncherTest,
       AppListShownAfterWallpaperPreviewConfirmed) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  wallpaper_test_api_->EndWallpaperPreview(/*confirm_preview_wallpaper=*/true);
  GetAppListTestHelper()->CheckVisibility(true);
}

TEST_P(AppListPresenterHomeLauncherTest,
       AppListShownAfterWallpaperPreviewCanceled) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  wallpaper_test_api_->EndWallpaperPreview(/*confirm_preview_wallpaper=*/false);
  GetAppListTestHelper()->CheckVisibility(true);
}

TEST_P(AppListPresenterHomeLauncherTest,
       AppListShownAfterWallpaperPreviewAndExitOverviewMode) {
  EnableTabletMode(true);
  wallpaper_test_api_->StartWallpaperPreview();
  EnterOverview();
  EXPECT_FALSE(IsAppListVisible());

  // Disable overview mode.
  ExitOverview();
  EXPECT_TRUE(IsAppListVisible());
}

// Tests that going home will minimize all windows.
TEST_P(AppListPresenterHomeLauncherTest, GoingHomeMinimizesAllWindows) {
  // Show app list in tablet mode. Maximize all windows.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0)),
      window2(CreateTestWindowInShellWithId(1)),
      window3(CreateTestWindowInShellWithId(2));
  WindowState *state1 = WindowState::Get(window1.get()),
              *state2 = WindowState::Get(window2.get()),
              *state3 = WindowState::Get(window3.get());
  state1->Maximize();
  state2->Maximize();
  state3->Maximize();
  EXPECT_TRUE(state1->IsMaximized());
  EXPECT_TRUE(state2->IsMaximized());
  EXPECT_TRUE(state3->IsMaximized());

  // The windows need to be activated for the mru window tracker.
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  auto ordering =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  GoHome();
  EXPECT_TRUE(state1->IsMinimized());
  EXPECT_TRUE(state2->IsMinimized());
  EXPECT_TRUE(state3->IsMinimized());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tests that the window ordering remains the same as before we minimize.
  auto new_order =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  EXPECT_TRUE(std::equal(ordering.begin(), ordering.end(), new_order.begin()));
}

// Tests that going home will end split view mode.
TEST_P(AppListPresenterHomeLauncherTest, GoingHomeEndsSplitViewMode) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  GoHome();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that going home will end overview mode.
TEST_P(AppListPresenterHomeLauncherTest, GoingHomeEndOverviewMode) {
  // Show app list in tablet mode. Enter overview mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  GoHome();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that going home will end overview and split view mode if both are
// active (e.g. one side of the split view contains overview).
TEST_P(AppListPresenterHomeLauncherTest,
       GoingHomeEndsSplitViewModeWithOverview) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> dummy_window(CreateTestWindowInShellWithId(1));

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  GoHome();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the context menu is triggered in the same way as if we are on
// the wallpaper.
TEST_P(AppListPresenterHomeLauncherTest, WallpaperContextMenu) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Long press on the app list to open the context menu.
  // TODO(ginko) look into a way to populate an apps grid, then get a point
  // between these apps so that clicks/taps between apps can be tested
  const gfx::Point onscreen_point(GetPointOutsideSearchbox());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ui::GestureEvent long_press(
      onscreen_point.x(), onscreen_point.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  generator->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  const aura::Window* root = window_util::GetRootWindowAt(onscreen_point);
  const RootWindowController* root_window_controller =
      RootWindowController::ForWindow(root);
  EXPECT_TRUE(root_window_controller->IsContextMenuShown());

  // Tap down to close the context menu.
  ui::GestureEvent tap_down(onscreen_point.x(), onscreen_point.y(), 0,
                            base::TimeTicks(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  generator->Dispatch(&tap_down);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShown());

  // Right click to open the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->ClickRightButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_TRUE(root_window_controller->IsContextMenuShown());

  // Left click to close the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShown());
}

// Tests app list visibility when switching to tablet mode during dragging from
// shelf.
TEST_P(AppListPresenterHomeLauncherTest,
       SwitchToTabletModeDuringDraggingFromShelf) {
  UpdateDisplay("1080x900");
  GetAppListTestHelper()->CheckVisibility(false);

  // Drag from the shelf to show the app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int x = 540;
  const int closed_y = 890;
  const int fullscreen_y = 0;
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list.
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Drag from the shelf to show the app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  GetAppListTestHelper()->CheckVisibility(true);

  // Switch to tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to try to close app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests app list visibility when switching to tablet mode during dragging to
// close app list.
TEST_P(AppListPresenterHomeLauncherTest,
       SwitchToTabletModeDuringDraggingToClose) {
  UpdateDisplay("1080x900");

  // Open app list.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int x = 540;
  const int peeking_height =
      900 - GetAppListView()->GetHeightForState(AppListViewState::kPeeking);
  const int closed_y = 890;
  generator->MoveTouch(gfx::Point(x, peeking_height));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Open app list.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list, meanwhile switch to tablet mode.
  generator->MoveTouch(gfx::Point(x, peeking_height));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, peeking_height + 10));
  EnableTabletMode(true);
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Test backdrop exists for active non-fullscreen window in tablet mode.
TEST_P(AppListPresenterHomeLauncherTest, BackdropTest) {
  WorkspaceControllerTestApi test_helper(ShellTestApi().workspace_controller());
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(test_helper.GetBackdropWindow());

  std::unique_ptr<aura::Window> non_fullscreen_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  non_fullscreen_window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_TRUE(test_helper.GetBackdropWindow());
}

// Tests that app list is not active when switching to tablet mode if an active
// window exists.
TEST_P(AppListPresenterHomeLauncherTest,
       NotActivateAppListWindowWhenActiveWindowExists) {
  // No window is active.
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Show app list in tablet mode. It should become active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(GetAppListView()->GetWidget()->GetNativeWindow(),
            window_util::GetActiveWindow());

  // End tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Activate a window.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState::Get(window.get())->Activate();
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Show app list in tablet mode. It should not be active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Tests that involve the virtual keyboard.
class AppListPresenterVirtualKeyboardTest : public AppListPresenterTest {
 public:
  AppListPresenterVirtualKeyboardTest() = default;
  ~AppListPresenterVirtualKeyboardTest() override = default;

  // AppListPresenterTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AppListPresenterTest::SetUp();
  }

  // Performs mouse click or tap gesture on the provided point, depending on
  // whether the test is parameterized to use mouse clicks or tap gestures.
  void ClickOrTap(const gfx::Point& point) {
    if (TestMouseEventParam())
      ClickMouseAt(point);
    else
      GetEventGenerator()->GestureTapAt(point);
  }
};

// Instantiate the values in the parameterized tests. First boolean is used to
// toggle mouse and touch events and in some tests to toggle fullscreen mode
// tests. The second one toggles cardfied state feature enabled or disabled.
INSTANTIATE_TEST_SUITE_P(All,
                         AppListPresenterVirtualKeyboardTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests that tapping or clicking the body of the applist with an active virtual
// keyboard when there exists text in the searchbox results in the virtual
// keyboard closing with no side effects.
TEST_P(AppListPresenterVirtualKeyboardTest,
       TapAppListWithVirtualKeyboardDismissesVirtualKeyboardWithSearchText) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);

  // Tap to activate the searchbox.
  ClickOrTap(GetPointInsideSearchbox());

  // Enter some text in the searchbox, the applist should transition to
  // fullscreen search.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Tap or click outside the searchbox, the virtual keyboard should hide.
  ClickOrTap(GetPointOutsideSearchbox());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // The searchbox should still be active and the AppListView should still be in
  // FULLSCREEN_SEARCH.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(GetAppListView()->search_box_view()->is_search_box_active());

  // Tap or click the body of the AppList again, the searchbox should deactivate
  // and the applist should be in FULLSCREEN_ALL_APPS.
  ClickOrTap(GetPointOutsideSearchbox());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(GetAppListView()->search_box_view()->is_search_box_active());
}

// Tests that tapping or clicking the body of the applist with an active virtual
// keyboard when there is no text in the searchbox results in both the virtual
// keyboard and searchbox closing with no side effects.
TEST_P(AppListPresenterVirtualKeyboardTest,
       TapAppListWithVirtualKeyboardDismissesVirtualKeyboardWithoutSearchText) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);

  // Tap to activate the searchbox.
  ClickOrTap(GetPointInsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Tap or click outside the searchbox, the virtual keyboard should hide and
  // the searchbox should be inactive when there is no text in the searchbox.
  ClickOrTap(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
  EXPECT_FALSE(GetAppListView()->search_box_view()->is_search_box_active());
}

TEST_P(AppListPresenterHomeLauncherTest, TapHomeButtonOnExternalDisplay) {
  UpdateDisplay("800x600,1000x768");

  TapHomeButton(GetSecondaryDisplay().id());
  {
    SCOPED_TRACE("1st tap");
    GetAppListTestHelper()->CheckVisibility(true);
    GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  }

  TapHomeButton(GetSecondaryDisplay().id());
  {
    SCOPED_TRACE("2nd tap");
    GetAppListTestHelper()->CheckVisibility(false);
    GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  }
}

// Test that gesture tapping the app list search box correctly handles the event
// by moving the textfield's cursor to the tapped position within the text.
TEST_P(AppListPresenterTest, SearchBoxTextfieldGestureTap) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListView()->SetState(AppListViewState::kFullscreenAllApps);

  // Tap on the search box to focus and open it.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Set the text of the search box textfield.
  views::Textfield* textfield =
      GetAppListView()->search_box_view()->search_box();
  textfield->SetText(u"Test search box string");

  // The textfield's cursor position should start out at the end of the string.
  size_t initial_cursor_position = textfield->GetCursorPosition();

  // Gesture tap to move the cursor to the middle of the string.
  gfx::Rect textfield_bounds = textfield->GetBoundsInScreen();
  gfx::Rect cursor_bounds =
      views::TextfieldTestApi(textfield).GetCursorViewRect();
  EXPECT_GT(cursor_bounds.x(), 0);
  gfx::Point touch_location(textfield_bounds.x() + cursor_bounds.x() / 2,
                            textfield_bounds.y());
  generator->GestureTapAt(touch_location);

  // Cursor position should have changed after the gesture tap.
  EXPECT_LT(textfield->GetCursorPosition(), initial_cursor_position);
}

}  // namespace ash
