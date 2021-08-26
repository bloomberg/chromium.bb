// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

constexpr int kBrowserAppIndexOnShelf = 0;

// A test shelf item delegate that simulates an activated window when a shelf
// item is selected.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    std::move(callback).Run(SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

}  // namespace

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

// Used to test that app launched metrics are properly recorded.
class AppListAppLaunchedMetricTest : public AshTestBase {
 public:
  AppListAppLaunchedMetricTest() = default;
  ~AppListAppLaunchedMetricTest() override = default;

  void SetUp() override {
    AppListView::SetShortAnimationForTesting(true);
    AshTestBase::SetUp();

    search_model_ = Shell::Get()->app_list_controller()->GetSearchModel();

    shelf_test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    AppListView::SetShortAnimationForTesting(false);
  }

 protected:
  void CreateAndClickShelfItem() {
    // Add shelf item to be launched. Waits for the shelf view's bounds
    // animations to end.
    ShelfItem shelf_item;
    shelf_item.id = ShelfID("app_id");
    shelf_item.type = TYPE_BROWSER_SHORTCUT;
    ShelfModel::Get()->Add(
        shelf_item, std::make_unique<TestShelfItemDelegate>(shelf_item.id));
    shelf_test_api_->RunMessageLoopUntilAnimationsDone();

    ClickShelfItem();
  }

  void ClickShelfItem() {
    // Get location of the shelf item.
    const views::ViewModel* view_model =
        GetPrimaryShelf()->GetShelfViewForTesting()->view_model_for_test();
    gfx::Point center = view_model->view_at(kBrowserAppIndexOnShelf)
                            ->GetBoundsInScreen()
                            .CenterPoint();

    // Click on the shelf item.
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(center);
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  }

  void PopulateAndLaunchSearchBoxTileItem() {
    // Populate 4 tile items.
    for (size_t i = 0; i < 4; i++) {
      auto search_result = std::make_unique<SearchResult>();
      search_result->set_display_type(SearchResultDisplayType::kTile);
      search_model_->results()->Add(std::move(search_result));
    }
    GetAppListTestHelper()->WaitUntilIdle();

    // Mark the privacy notices as dismissed so that the tile items will be the
    // first search container.
    ContentsView* contents_view = Shell::Get()
                                      ->app_list_controller()
                                      ->presenter()
                                      ->GetView()
                                      ->app_list_main_view()
                                      ->contents_view();
    Shell::Get()->app_list_controller()->MarkSuggestedContentInfoDismissed();
    contents_view->search_result_page_view()
        ->GetPrivacyContainerViewForTest()
        ->Update();

    SearchResultContainerView* search_result_container_view =
        contents_view->search_result_page_view()
            ->GetSearchResultTileItemListViewForTest();

    // Request focus on the first tile item view.
    search_result_container_view->GetFirstResultView()->RequestFocus();

    // Press return to simulate an app launch from the tile item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  }

  void PopulateAndLaunchSuggestionChip() {
    // Populate 4 suggestion chips.
    for (size_t i = 0; i < 4; i++) {
      auto search_result_chip = std::make_unique<SearchResult>();
      search_result_chip->set_display_type(SearchResultDisplayType::kChip);
      search_result_chip->set_is_recommendation(true);
      search_model_->results()->Add(std::move(search_result_chip));
    }
    GetAppListTestHelper()->WaitUntilIdle();

    SearchResultContainerView* suggestions_container_ =
        Shell::Get()
            ->app_list_controller()
            ->presenter()
            ->GetView()
            ->app_list_main_view()
            ->contents_view()
            ->apps_container_view()
            ->suggestion_chip_container_view_for_test();

    // Get focus on the first chip.
    suggestions_container_->children().front()->RequestFocus();
    GetAppListTestHelper()->WaitUntilIdle();

    // Press return to simulate an app launch from the suggestion chip.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  }

  void PopulateAndLaunchAppInGrid() {
    // Populate apps in the root app grid.
    AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
    model->AddItem(std::make_unique<AppListItem>("item 0"));
    model->AddItem(std::make_unique<AppListItem>("item 1"));
    model->AddItem(std::make_unique<AppListItem>("item 2"));
    model->AddItem(std::make_unique<AppListItem>("item 3"));

    AppListView::TestApi test_api(
        Shell::Get()->app_list_controller()->presenter()->GetView());

    // Focus the first item in the root app grid.
    test_api.GetRootAppsGridView()->GetItemViewAt(0)->RequestFocus();

    // Press return to simulate an app launch from a grid item.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  }

 private:
  SearchModel* search_model_ = nullptr;
  std::unique_ptr<ShelfViewTestAPI> shelf_test_api_;

  DISALLOW_COPY_AND_ASSIGN(AppListAppLaunchedMetricTest);
};

// Test that the histogram records an app launch from the shelf while the half
// launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, HalfLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Press a letter key, the AppListView should transition to kHalf.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  CreateAndClickShelfItem();
  GetAppListTestHelper()->WaitUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Half", AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the search box while the
// half launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, HalfLaunchFromSearchBox) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Press a letter key, the AppListView should transition to kHalf.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  PopulateAndLaunchSearchBoxTileItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Half",
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      1 /* Number of times launched from search box */);
}

// Test that the histogram records an app launch from the search box while the
// fullscreen search launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenSearchLaunchFromSearchBox) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  PopulateAndLaunchSearchBoxTileItem();

  GetAppListTestHelper()->WaitUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenSearch",
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      1 /* Number of times launched from search box */);
}

// Test that the histogram records an app launch from the shelf while the
// fullscreen search launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenSearchLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenSearch",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from a suggestion chip while
// the fullscreen all apps launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenAllAppsLaunchFromChip) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchSuggestionChip();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps",
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      1 /* Number of times launched from chip */);
}

// Test that the histogram records an app launch from the app grid while the
// fullscreen all apps launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenAllAppsLaunchFromGrid) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchAppInGrid();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps",
      AppListLaunchedFrom::kLaunchedFromGrid,
      1 /* Number of times launched from grid */);
}

// Test that the histogram records an app launch from the shelf while the
// fullscreen all apps launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, FullscreenAllAppsLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  // Press search + shift to transition to kFullscreenAllApps.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.FullscreenAllApps",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the shelf while the
// peeking launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, PeekingLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Peeking",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from a suggestion chip while
// the peeking launcher is showing.
TEST_F(AppListAppLaunchedMetricTest, PeekingLaunchFromChip) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  PopulateAndLaunchSuggestionChip();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Peeking",
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      1 /* Number of times launched from chip */);
}

// Test that the histogram records an app launch from the shelf while the
// launcher is closed.
TEST_F(AppListAppLaunchedMetricTest, ClosedLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Closed",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);

  // Open the launcher to peeking.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Close launcher back to closed.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  ClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.Closed",
      AppListLaunchedFrom::kLaunchedFromShelf,
      2 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the shelf while the
// homecher all apps state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherAllAppsLaunchFromShelf) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the app grid while the
// homecher all apps state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherAllAppsLaunchFromGrid) {
  base::HistogramTester histogram_tester;

  // Enable tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchAppInGrid();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromGrid,
      1 /* Number of times launched from grid */);
}

// Test that the histogram records an app launch from a suggestion chip while
// the homecher all apps state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherAllAppsLaunchFromChip) {
  base::HistogramTester histogram_tester;

  GetAppListTestHelper()->WaitUntilIdle();
  // Enable tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  PopulateAndLaunchSuggestionChip();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherAllApps",
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      1 /* Number of times launched from chip */);
}

// Test that the histogram records an app launch from the shelf while the
// homecher search state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherSearchLaunchFromShelf) {
  base::HistogramTester histogram_tester;

  // Enable tablet mode.
  GetAppListTestHelper()->WaitUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  CreateAndClickShelfItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherSearch",
      AppListLaunchedFrom::kLaunchedFromShelf,
      1 /* Number of times launched from shelf */);
}

// Test that the histogram records an app launch from the search box while the
// homercher search state is showing.
TEST_F(AppListAppLaunchedMetricTest, HomecherSearchLaunchFromSearchBox) {
  base::HistogramTester histogram_tester;

  // Enable tablet mode.
  GetAppListTestHelper()->WaitUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Press a letter key, the AppListView should transition to kFullscreenSearch.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_H);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Populate search box with tile items and launch a tile item.
  PopulateAndLaunchSearchBoxTileItem();

  histogram_tester.ExpectBucketCount(
      "Apps.AppListAppLaunchedV2.HomecherSearch",
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      1 /* Number of times launched from search box */);
}

class AppListShowSourceMetricTest : public AshTestBase {
 public:
  AppListShowSourceMetricTest() = default;
  ~AppListShowSourceMetricTest() override = default;

 protected:
  void ClickHomeButton() {
    HomeButton* home_button =
        GetPrimaryShelf()->navigation_widget()->GetHomeButton();
    gfx::Point center = home_button->GetCenterPoint();
    views::View::ConvertPointToScreen(home_button, &center);
    GetEventGenerator()->MoveMouseTo(center);
    GetEventGenerator()->ClickLeftButton();
  }

  DISALLOW_COPY_AND_ASSIGN(AppListShowSourceMetricTest);
};

// In tablet mode, test that AppListShowSource metric is only recorded when
// pressing home button when not already home. Any presses on the home button
// when already home should do nothing.
TEST_F(AppListShowSourceMetricTest, TabletInAppToHome) {
  base::HistogramTester histogram_tester;

  // Enable accessibility feature that forces home button to be shown in tablet
  // mode.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  ClickHomeButton();
  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", kShelfButton,
      1 /* Number of times app list is shown with a shelf button */);
  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", kTabletMode,
      0 /* Number of times app list is shown by tablet mode transition */);

  GetAppListTestHelper()->CheckVisibility(true);

  // Ensure that any subsequent clicks while already at home do not count as
  // showing the app list.
  ClickHomeButton();
  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", kShelfButton,
      1 /* Number of times app list shown with a shelf button */);
  histogram_tester.ExpectTotalCount("Apps.AppListShowSource", 1);
}

// Ensure that app list is not recorded as shown when going to tablet mode with
// a window open.
TEST_F(AppListShowSourceMetricTest, TabletModeWithWindowOpen) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckVisibility(false);

  // Ensure that no AppListShowSource metric was recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListShowSource", 0);
}

// Ensure that app list is recorded as shown when going to tablet mode with no
// other windows open.
TEST_F(AppListShowSourceMetricTest, TabletModeWithNoWindowOpen) {
  base::HistogramTester histogram_tester;

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  GetAppListTestHelper()->CheckVisibility(true);

  histogram_tester.ExpectBucketCount(
      "Apps.AppListShowSource", kTabletMode,
      1 /* Number of times app list shown after entering tablet mode */);
}

class AppListBubbleShowSourceMetricTest : public AppListShowSourceMetricTest {
 public:
  AppListBubbleShowSourceMetricTest() {
    scoped_feature_list_.InitWithFeatures({features::kAppListBubble}, {});
  }
  ~AppListBubbleShowSourceMetricTest() override = default;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that showing the AppListBubble in clamshell mode records the proper
// metrics for Apps.AppListBubbleShowSource.
TEST_F(AppListBubbleShowSourceMetricTest, ClamshellModeHomeButton) {
  base::HistogramTester histogram_tester;
  auto* app_list_bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  // Show the Bubble AppList.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_bubble_presenter->IsShowing());

  // Test that the proper histogram is logged.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 1);

  // Hide the Bubble AppList.
  GetAppListTestHelper()->Dismiss();
  EXPECT_FALSE(app_list_bubble_presenter->IsShowing());

  // Test that no histograms were logged.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 1);

  // Show the Bubble AppList one more time.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_bubble_presenter->IsShowing());

  // Test that the histogram records 2 total shows.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 2);

  // Test that no fullscreen app list metrics were recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListShowSource", 0);
}

// Test that tablet mode launcher operations do not record AppListBubble
// metrics.
TEST_F(AppListBubbleShowSourceMetricTest,
       TabletModeDoesNotRecordAppListBubbleShow) {
  base::HistogramTester histogram_tester;
  // Enable accessibility feature that forces home button to be shown in tablet
  // mode.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);

  // Go to tablet mode, the tablet mode (non bubble) launcher will show. Create
  // a test widget so the launcher will show in the background.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto* app_list_bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  EXPECT_FALSE(app_list_bubble_presenter->IsShowing());

  // Ensure that no AppListBubbleShowSource metric was recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 0);

  // Press the Home Button, which hides `widget` and shows the tablet mode
  // launcher.
  ClickHomeButton();
  EXPECT_FALSE(app_list_bubble_presenter->IsShowing());

  // Test that no AppListBubble metrics were recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 0);
}

// Tests that Toggling the AppListBubble does not record metrics when the
// result of the toggle is that the AppListBubble is hidden.
TEST_F(AppListBubbleShowSourceMetricTest, ToggleDoesNotRecordOnHide) {
  base::HistogramTester histogram_tester;
  auto* app_list_controller = Shell::Get()->app_list_controller();

  // Toggle the AppListBubble to show it.
  app_list_controller->ToggleAppList(GetPrimaryDisplayId(),
                                     AppListShowSource::kSearchKey,
                                     base::TimeTicks::Now());
  auto* app_list_bubble_presenter =
      Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  ASSERT_TRUE(app_list_bubble_presenter->IsShowing());

  // Toggle the AppListBubble once more, to hide it.
  app_list_controller->ToggleAppList(GetPrimaryDisplayId(),
                                     AppListShowSource::kSearchKey,
                                     base::TimeTicks::Now());
  ASSERT_FALSE(app_list_bubble_presenter->IsShowing());
  // Test that only one show was recorded.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleShowSource", 1);
}

using AppListAppCountMetricTest = AshTestBase;

// Verify that the number of items in the app list are recorded correctly.
TEST_F(AppListAppCountMetricTest, RecordApplistItemCounts) {
  base::HistogramTester histogram;
  histogram.ExpectTotalCount("Apps.AppList.NumberOfApps", 0);
  histogram.ExpectTotalCount("Apps.AppList.NumberOfRootLevelItems", 0);

  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();

  // Add 5 items to the app list.
  for (int i = 0; i < 5; i++) {
    model->AddItem(
        std::make_unique<AppListItem>(base::StringPrintf("app_id_%d", i)));
  }

  // Check that 5 items are recorded as being in the app list.
  RecordPeriodicAppListMetrics();
  histogram.ExpectBucketCount("Apps.AppList.NumberOfApps", 5, 1);
  histogram.ExpectBucketCount("Apps.AppList.NumberOfRootLevelItems", 5, 1);
  histogram.ExpectTotalCount("Apps.AppList.NumberOfApps", 1);
  histogram.ExpectTotalCount("Apps.AppList.NumberOfRootLevelItems", 1);

  // Create a folder and add 3 items to it.
  const std::string folder_id = base::StringPrintf("folder_id");
  auto folder = std::make_unique<AppListFolderItem>(folder_id);
  model->AddItem(std::move(folder));
  for (int i = 0; i < 3; i++) {
    auto item =
        std::make_unique<AppListItem>(base::StringPrintf("id_in_folder_%d", i));
    model->AddItemToFolder(std::move(item), folder_id);
  }

  // Check that the folder and its items are recorded in the metrics.
  RecordPeriodicAppListMetrics();
  histogram.ExpectBucketCount("Apps.AppList.NumberOfApps", 8, 1);
  histogram.ExpectBucketCount("Apps.AppList.NumberOfRootLevelItems", 6, 1);
}

}  // namespace ash
