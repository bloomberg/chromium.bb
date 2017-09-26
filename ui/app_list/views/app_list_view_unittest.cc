// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/expand_arrow_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_page_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/app_list/views/suggestions_container_view.h"
#include "ui/app_list/views/test/app_list_view_test_api.h"
#include "ui/app_list/views/test/apps_grid_view_test_api.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_model.h"

namespace app_list {
namespace test {

namespace {

// Choose a set that is 3 regular app list pages and 2 landscape app list pages.
constexpr int kInitialItems = 34;
// Default peeking y value for the app list.
constexpr int kPeekingYValue = 280;

template <class T>
size_t GetVisibleViews(const std::vector<T*>& tiles) {
  size_t count = 0;
  for (const auto& tile : tiles) {
    if (tile->visible())
      count++;
  }
  return count;
}

// A standard set of checks on a view, e.g., ensuring it is drawn and visible.
void CheckView(views::View* subview) {
  ASSERT_TRUE(subview);
  EXPECT_TRUE(subview->parent());
  EXPECT_TRUE(subview->visible());
  EXPECT_TRUE(subview->IsDrawn());
  EXPECT_FALSE(subview->bounds().IsEmpty());
}

void SimulateClick(views::View* view) {
  gfx::Point center = view->GetLocalBounds().CenterPoint();
  view->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  view->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
}

class TestStartPageSearchResult : public TestSearchResult {
 public:
  TestStartPageSearchResult() { set_display_type(DISPLAY_RECOMMENDATION); }
  ~TestStartPageSearchResult() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStartPageSearchResult);
};

class AppListViewTest : public views::ViewsTestBase {
 public:
  AppListViewTest() = default;
  ~AppListViewTest() override = default;

  // testing::Test
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    gfx::NativeView parent = GetContext();
    delegate_.reset(new AppListTestViewDelegate);
    view_ = new AppListView(delegate_.get());
    AppListView::InitParams params;
    params.parent = parent;
    view_->Initialize(params);
    // Initialize around a point that ensures the window is wholly shown.
    const gfx::Size size = view_->bounds().size();
    view_->MaybeSetAnchorPoint(gfx::Point(size.width() / 2, size.height() / 2));
  }

  void TearDown() override {
    view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
  }

 protected:
  // Switches the launcher to |state| and lays out to ensure all launcher pages
  // are in the correct position. Checks that the state is where it should be
  // and returns false on failure.
  bool SetAppListState(AppListModel::State state) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();
    contents_view->SetActiveState(state);
    contents_view->Layout();
    return IsStateShown(state);
  }

  // Returns true if all of the pages are in their correct position for |state|.
  bool IsStateShown(AppListModel::State state) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();
    bool success = true;
    for (int i = 0; i < contents_view->NumLauncherPages(); ++i) {
      success = success &&
                (contents_view->GetPageView(i)->GetPageBoundsForState(state) ==
                 contents_view->GetPageView(i)->bounds());
    }
    return success && state == delegate_->GetTestModel()->state();
  }

  // Shows the app list and waits until a paint occurs.
  void Show() {
    view_->GetWidget()->Show();
    base::RunLoop run_loop;
    AppListViewTestApi test_api(view_);
    test_api.SetNextPaintCallback(run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_TRUE(view_->GetWidget()->IsVisible());
  }

  // Checks the search box widget is at |expected| in the contents view's
  // coordinate space.
  bool CheckSearchBoxWidget(const gfx::Rect& expected) {
    ContentsView* contents_view = view_->app_list_main_view()->contents_view();
    // Adjust for the search box view's shadow.
    gfx::Rect expected_with_shadow =
        view_->app_list_main_view()
            ->search_box_view()
            ->GetViewBoundsForSearchBoxContentsBounds(expected);
    gfx::Point point = expected_with_shadow.origin();
    views::View::ConvertPointToScreen(contents_view, &point);

    return gfx::Rect(point, expected_with_shadow.size()) ==
           view_->search_box_widget()->GetWindowBoundsInScreen();
  }

  // Gets the PaginationModel owned by |view_|.
  PaginationModel* GetPaginationModel() const {
    return view_->GetAppsPaginationModel();
  }

  AppListView* view_ = nullptr;  // Owned by native widget.
  std::unique_ptr<AppListTestViewDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewTest);
};

// TODO(crbug.com/748260): Stop inheriting from AppListViewTest here.
class AppListViewFullscreenTest : public AppListViewTest {
 public:
  AppListViewFullscreenTest() = default;
  ~AppListViewFullscreenTest() override = default;

  // testing::Test
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableFullscreenAppList);
  }

 protected:
  void Show() { view_->ShowWhenReady(); }

  void Initialize(int initial_apps_page,
                  bool is_tablet_mode,
                  bool is_side_shelf) {
    delegate_.reset(new AppListTestViewDelegate);
    view_ = new AppListView(delegate_.get());
    AppListView::InitParams params;
    params.parent = GetContext();
    params.initial_apps_page = initial_apps_page;
    params.is_tablet_mode = is_tablet_mode;
    params.is_side_shelf = is_side_shelf;
    view_->Initialize(params);
    // Initialize around a point that ensures the window is wholly shown.
    EXPECT_FALSE(view_->GetWidget()->IsVisible());
  }

  void InitializeStartPageView(size_t apps_num) {
    Initialize(0, false, false);
    AppListTestModel* model = delegate_->GetTestModel();

    // Adds suggestion apps to the start page view and show start page view.
    for (size_t i = 0; i < apps_num; i++)
      model->results()->Add(base::MakeUnique<TestStartPageSearchResult>());
    EXPECT_TRUE(SetAppListState(AppListModel::STATE_START));
    start_page_view()->UpdateForTesting();
    EXPECT_EQ(apps_num, GetVisibleViews(start_page_view()->tile_views()));

    // Initially, no view gets focus.
    EXPECT_EQ(FOCUS_NONE, search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kNoSelection,
              start_page_view()->GetSelectedIndexForTest());
  }

  // Test focus movement within search box view and start page view on tab key
  // or right arrow key.
  void TestStartPageViewForwardFocusOnKey(ui::KeyEvent* key, size_t apps_num) {
    // Sets focus on search box.
    search_box_view()->search_box()->OnKeyEvent(key);
    EXPECT_EQ(FOCUS_SEARCH_BOX, search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kNoSelection,
              start_page_view()->GetSelectedIndexForTest());

    // When focus is on search box, moves focus through suggestion apps.
    for (size_t i = 0; i < apps_num; i++) {
      search_box_view()->search_box()->OnKeyEvent(key);
      EXPECT_EQ(FOCUS_CONTENTS_VIEW,
                search_box_view()->get_focused_view_for_test());
      EXPECT_EQ(static_cast<int>(i),
                start_page_view()->GetSelectedIndexForTest());
    }

    // When focus is on the last suggestion app, moves focus to expand arrow.
    search_box_view()->search_box()->OnKeyEvent(key);
    EXPECT_EQ(FOCUS_CONTENTS_VIEW,
              search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kExpandArrowSelection,
              start_page_view()->GetSelectedIndexForTest());

    // When focus is on expand arrow, clears focus.
    search_box_view()->search_box()->OnKeyEvent(key);
    EXPECT_EQ(FOCUS_NONE, search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kNoSelection,
              start_page_view()->GetSelectedIndexForTest());
  }

  // Test focus movement within search box view and start page view on shift+tab
  // key or left arrow key.
  void TestStartPageViewBackwardFocusOnKey(ui::KeyEvent* key, size_t apps_num) {
    // Sets focus on expand arrow.
    search_box_view()->search_box()->OnKeyEvent(key);
    EXPECT_EQ(FOCUS_CONTENTS_VIEW,
              search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kExpandArrowSelection,
              start_page_view()->GetSelectedIndexForTest());

    // When focus is on search box, moves focus through suggestion apps
    // reversely.
    for (size_t i = 0; i < apps_num; i++) {
      search_box_view()->search_box()->OnKeyEvent(key);
      EXPECT_EQ(FOCUS_CONTENTS_VIEW,
                search_box_view()->get_focused_view_for_test());
      EXPECT_EQ(static_cast<int>(apps_num - 1 - i),
                start_page_view()->GetSelectedIndexForTest());
    }

    // When focus is on the first suggestion app, moves focus to search box.
    search_box_view()->search_box()->OnKeyEvent(key);
    EXPECT_EQ(FOCUS_SEARCH_BOX, search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kNoSelection,
              start_page_view()->GetSelectedIndexForTest());

    // When focus is on search box, clears focus.
    search_box_view()->search_box()->OnKeyEvent(key);
    EXPECT_EQ(FOCUS_NONE, search_box_view()->get_focused_view_for_test());
    EXPECT_EQ(StartPageView::kNoSelection,
              start_page_view()->GetSelectedIndexForTest());
  }

  AppListMainView* main_view() { return view_->app_list_main_view(); }

  StartPageView* start_page_view() {
    return main_view()->contents_view()->start_page_view();
  }

  SearchBoxView* search_box_view() { return main_view()->search_box_view(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(AppListViewFullscreenTest);
};

// TODO(weidongg/766807) Remove all old focus tests after the flag is enabled
// by default.
class AppListViewFocusTest : public views::ViewsTestBase {
 public:
  AppListViewFocusTest() = default;
  ~AppListViewFocusTest() override = default;

  // testing::Test
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Enable app list focus.
    scoped_feature_list_.InitAndEnableFeature(features::kEnableAppListFocus);

    // Initialize app list view.
    delegate_.reset(new AppListTestViewDelegate);
    view_ = new AppListView(delegate_.get());
    AppListView::InitParams params;
    params.parent = GetContext();
    view_->Initialize(params);
    test_api_.reset(new AppsGridViewTestApi(apps_grid_view()));

    // Add suggestion apps and app list items.
    const int kSuggestionAppNum = 5;
    const int kAppListItemNum = test_api_->TilesPerPage(0) + 1;
    AppListTestModel* model = delegate_->GetTestModel();
    for (size_t i = 0; i < kSuggestionAppNum; i++)
      model->results()->Add(base::MakeUnique<TestStartPageSearchResult>());
    model->PopulateApps(kAppListItemNum);
    apps_grid_view()->ResetForShowApps();

    // Disable animation timer.
    view_->GetWidget()->GetLayer()->GetAnimator()->set_disable_timer_for_test(
        true);
  }

  void TearDown() override {
    view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
  }

  void SetAppListState(AppListView::AppListState state) {
    if (state == AppListView::CLOSED) {
      view_->Dismiss();
      return;
    }
    view_->SetState(state);
  }

  void Show() { view_->ShowWhenReady(); }

  void SimulateKeyPress(ui::KeyboardCode key_code, bool shift_down) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, key_code,
                           shift_down ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    view_->GetWidget()->OnKeyEvent(&key_event);
  }

  // Add search results for test on focus movement.
  void SetUpSearchResults(int tile_results_num, int list_results_num) {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_TILE, tile_results_num));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_LIST, list_results_num));

    AppListModel::SearchResults* results = delegate_->GetTestModel()->results();
    results->DeleteAll();
    double relevance = result_types.size();
    for (const auto& data : result_types) {
      // Set the relevance of the results in each group in decreasing order (so
      // the earlier groups have higher relevance, and therefore appear first).
      relevance -= 0.5;
      for (int i = 0; i < data.second; ++i) {
        std::unique_ptr<TestSearchResult> result =
            base::MakeUnique<TestSearchResult>();
        result->set_display_type(data.first);
        result->set_relevance(relevance);
        results->Add(std::move(result));
      }
    }

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  int GetOpenFirstSearchResultCount() {
    std::map<size_t, int>& counts = delegate_->open_search_result_counts();
    if (counts.size() == 0)
      return 0;
    return counts[0];
  }

  int GetTotalOpenSearchResultCount() {
    return delegate_->open_search_result_count();
  }

  AppListView* app_list_view() { return view_; }

  AppListMainView* main_view() { return view_->app_list_main_view(); }

  ContentsView* contents_view() {
    return view_->app_list_main_view()->contents_view();
  }

  AppsGridView* apps_grid_view() {
    return main_view()
        ->contents_view()
        ->apps_container_view()
        ->apps_grid_view();
  }

  SuggestionsContainerView* suggestions_container_view() {
    return apps_grid_view()->suggestions_container_for_test();
  }

  SearchBoxView* search_box_view() { return main_view()->search_box_view(); }

  views::View* focused_view() {
    return view_->GetWidget()->GetFocusManager()->GetFocusedView();
  }

 private:
  AppListView* view_ = nullptr;  // Owned by native widget.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AppsGridViewTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewFocusTest);
};

}  // namespace

// Tests that the initial focus is on search box.
TEST_F(AppListViewFocusTest, InitialFocus) {
  Show();
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests the focus traversal triggered by tab in peeking state.
TEST_F(AppListViewFocusTest, FocusTraversalInPeekingState) {
  Show();
  SetAppListState(AppListView::PEEKING);

  // Move focus along suggestion apps.
  for (views::View* v : suggestions_container_view()->tile_views()) {
    SimulateKeyPress(ui::VKEY_TAB, false);
    EXPECT_EQ(v, focused_view());
  }

  // Move focus on expand arrow.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_EQ(apps_grid_view()->expand_arrow_view_for_test(), focused_view());

  // Move focus back to search box.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests the focus traversal triggered by tab in fullscreen all apps state.
TEST_F(AppListViewFocusTest, FocusTraversalInFullscreenAllAppsState) {
  Show();
  SetAppListState(AppListView::FULLSCREEN_ALL_APPS);

  // Move focus along suggestion apps.
  for (views::View* v : suggestions_container_view()->tile_views()) {
    SimulateKeyPress(ui::VKEY_TAB, false);
    EXPECT_EQ(v, focused_view());
  }

  // Move focus along app list items.
  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view()->view_model_for_test();
  for (int i = 0; i < view_model->view_size(); ++i) {
    SimulateKeyPress(ui::VKEY_TAB, false);
    EXPECT_EQ(view_model->view_at(i), focused_view());
  }

  // Move focus back to search box.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests the focus traversal triggered by tab in half state with opened search
// box.
TEST_F(AppListViewFocusTest, FocusTraversalInHalfState) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  search_box_view()->search_box()->InsertText(base::UTF8ToUTF16("test"));
  EXPECT_EQ(app_list_view()->app_list_state(), AppListView::HALF);
  const int kTileResults = 3;
  const int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);

  // Move focus to close button.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_EQ(search_box_view()->close_button(), focused_view());

  // Move focus along the search results.
  const std::vector<SearchResultTileItemView*>& tile_views =
      contents_view()
          ->search_result_tile_item_list_view_for_test()
          ->tile_views_for_test();
  for (int i = 0; i < kTileResults; ++i) {
    SimulateKeyPress(ui::VKEY_TAB, false);
    EXPECT_EQ(tile_views[i], focused_view());
  }
  const views::View* results_container =
      contents_view()
          ->search_result_list_view_for_test()
          ->results_container_for_test();
  for (int i = 0; i < kListResults; ++i) {
    SimulateKeyPress(ui::VKEY_TAB, false);
    EXPECT_EQ(results_container->child_at(i), focused_view());
  }

  // Move focus back to search box.
  SimulateKeyPress(ui::VKEY_TAB, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests that the focus is set back onto search box after state transition.
TEST_F(AppListViewFocusTest, FocusResetAfterStateTransition) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake search results.
  search_box_view()->search_box()->InsertText(base::UTF8ToUTF16("test"));
  const int kTileResults = 3;
  const int kListResults = 2;
  SetUpSearchResults(kTileResults, kListResults);
  EXPECT_EQ(app_list_view()->app_list_state(), AppListView::HALF);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());

  // Move focus to the first search result, then transition to PEEKING state.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SimulateKeyPress(ui::VKEY_TAB, false);
  SetAppListState(AppListView::PEEKING);
  EXPECT_EQ(app_list_view()->app_list_state(), AppListView::PEEKING);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());

  // Move focus to the first suggestion app, then transition to
  // FULLSCREEN_ALL_APPS state.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SetAppListState(AppListView::FULLSCREEN_ALL_APPS);
  EXPECT_EQ(app_list_view()->app_list_state(),
            AppListView::FULLSCREEN_ALL_APPS);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());

  // Move focus to the first suggestion app, then transition to PEEKING state.
  SimulateKeyPress(ui::VKEY_TAB, false);
  SetAppListState(AppListView::PEEKING);
  EXPECT_EQ(app_list_view()->app_list_state(), AppListView::PEEKING);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
}

// Tests that key event which is not handled by focused view will be redirected
// to search box.
TEST_F(AppListViewFocusTest, RedirectFocusToSearchBox) {
  Show();

  // Set focus to first suggestion app and type a character.
  suggestions_container_view()->tile_views()[0]->RequestFocus();
  SimulateKeyPress(ui::VKEY_SPACE, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->text(), base::UTF8ToUTF16(" "));

  // Set focus to expand arrow and type a character.
  apps_grid_view()->expand_arrow_view_for_test()->RequestFocus();
  SimulateKeyPress(ui::VKEY_A, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->text(), base::UTF8ToUTF16(" a"));

  // Set focus to close button and type a character.
  search_box_view()->close_button()->RequestFocus();
  SimulateKeyPress(ui::VKEY_B, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->text(), base::UTF8ToUTF16(" ab"));

  // Set focus to close button and hitting backspace.
  search_box_view()->close_button()->RequestFocus();
  SimulateKeyPress(ui::VKEY_BACK, false);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(search_box_view()->search_box()->text(), base::UTF8ToUTF16(" a"));
}

// Tests that the first search result's view is always selected after search
// results are updated, but the focus is always on search box.
TEST_F(AppListViewFocusTest, FirstResultSelectedAfterSearchResultsUpdated) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake list results.
  search_box_view()->search_box()->InsertText(base::UTF8ToUTF16("test"));
  const int kListResults = 2;
  SetUpSearchResults(0, kListResults);
  const views::View* results_container =
      contents_view()
          ->search_result_list_view_for_test()
          ->results_container_for_test();
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(results_container->child_at(0),
            contents_view()->search_results_page_view()->first_result_view());

  // Populate both fake list results and tile results.
  const int kTileResults = 3;
  SetUpSearchResults(kTileResults, kListResults);
  const std::vector<SearchResultTileItemView*>& tile_views =
      contents_view()
          ->search_result_tile_item_list_view_for_test()
          ->tile_views_for_test();
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(tile_views[0],
            contents_view()->search_results_page_view()->first_result_view());

  // Clear up all search results.
  SetUpSearchResults(0, 0);
  EXPECT_EQ(search_box_view()->search_box(), focused_view());
  EXPECT_EQ(nullptr,
            contents_view()->search_results_page_view()->first_result_view());
}

// Tests that hitting Enter key when focus is on search box opens the first
// result when it exists.
TEST_F(AppListViewFocusTest, HittingEnterWhenFocusOnSearchBox) {
  Show();

  // Type something in search box to transition to HALF state and populate
  // fake list results. Then hit Enter key.
  search_box_view()->search_box()->InsertText(base::UTF8ToUTF16("test"));
  const int kListResults = 2;
  SetUpSearchResults(0, kListResults);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(1, GetOpenFirstSearchResultCount());
  EXPECT_EQ(1, GetTotalOpenSearchResultCount());

  // Populate both fake list results and tile results. Then hit Enter key.
  const int kTileResults = 3;
  SetUpSearchResults(kTileResults, kListResults);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(2, GetOpenFirstSearchResultCount());
  EXPECT_EQ(2, GetTotalOpenSearchResultCount());

  // Clear up all search results. Then hit Enter key.
  SetUpSearchResults(0, 0);
  SimulateKeyPress(ui::VKEY_RETURN, false);
  EXPECT_EQ(2, GetOpenFirstSearchResultCount());
  EXPECT_EQ(2, GetTotalOpenSearchResultCount());
}

// Tests that opening the app list opens in peeking mode by default.
TEST_F(AppListViewFullscreenTest, ShowPeekingByDefault) {
  Initialize(0, false, false);

  Show();

  ASSERT_EQ(AppListView::PEEKING, view_->app_list_state());
}

// Tests that in side shelf mode, the app list opens in fullscreen by default.
TEST_F(AppListViewFullscreenTest, ShowFullscreenWhenInSideShelfMode) {
  Initialize(0, false, true);

  Show();

  ASSERT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that in tablet mode, the app list opens in fullscreen by default.
TEST_F(AppListViewFullscreenTest, ShowFullscreenWhenInTabletMode) {
  Initialize(0, true, false);

  Show();

  ASSERT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that setting empty text in the search box does not change the state.
TEST_F(AppListViewFullscreenTest, EmptySearchTextStillPeeking) {
  Initialize(0, false, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());

  ASSERT_EQ(AppListView::PEEKING, view_->app_list_state());
}

// Tests that typing text after opening transitions from peeking to half.
TEST_F(AppListViewFullscreenTest, TypingPeekingToHalf) {
  Initialize(0, false, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("nice"));

  ASSERT_EQ(AppListView::HALF, view_->app_list_state());
}

// Tests that typing when in fullscreen changes the state to fullscreen search.
TEST_F(AppListViewFullscreenTest, TypingFullscreenToFullscreenSearch) {
  Initialize(0, false, false);
  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("https://youtu.be/dQw4w9WgXcQ"));

  ASSERT_EQ(AppListView::FULLSCREEN_SEARCH, view_->app_list_state());
}

// Tests that in tablet mode, typing changes the state to fullscreen search.
TEST_F(AppListViewFullscreenTest, TypingTabletModeFullscreenSearch) {
  Initialize(0, true, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("cool!"));

  ASSERT_EQ(AppListView::FULLSCREEN_SEARCH, view_->app_list_state());
}

// Tests that pressing escape when in peeking closes the app list.
TEST_F(AppListViewFullscreenTest, EscapeKeyPeekingToClosed) {
  Initialize(0, false, false);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::CLOSED, view_->app_list_state());
}

// Tests that pressing escape when in half screen changes the state to peeking.
TEST_F(AppListViewFullscreenTest, EscapeKeyHalfToPeeking) {
  Initialize(0, false, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("doggie"));
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::PEEKING, view_->app_list_state());
}

// Tests that pressing escape when in fullscreen changes the state to closed.
TEST_F(AppListViewFullscreenTest, EscapeKeyFullscreenToClosed) {
  Initialize(0, false, false);
  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::CLOSED, view_->app_list_state());
}

// Tests that pressing escape when in fullscreen side-shelf closes the app list.
TEST_F(AppListViewFullscreenTest, EscapeKeySideShelfFullscreenToClosed) {
  // Put into fullscreen by using side-shelf.
  Initialize(0, false, true);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::CLOSED, view_->app_list_state());
}

// Tests that pressing escape when in tablet mode closes the app list.
TEST_F(AppListViewFullscreenTest, EscapeKeyTabletModeFullscreenToClosed) {
  // Put into fullscreen by using tablet mode.
  Initialize(0, true, false);

  Show();
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::CLOSED, view_->app_list_state());
}

// Tests that pressing escape when in fullscreen search changes to fullscreen.
TEST_F(AppListViewFullscreenTest, EscapeKeyFullscreenSearchToFullscreen) {
  Initialize(0, false, false);
  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("https://youtu.be/dQw4w9WgXcQ"));
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that pressing escape when in sideshelf search changes to fullscreen.
TEST_F(AppListViewFullscreenTest, EscapeKeySideShelfSearchToFullscreen) {
  // Put into fullscreen using side-shelf.
  Initialize(0, false, true);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("kitty"));
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that in fullscreen, the app list has multiple pages with enough apps.
TEST_F(AppListViewFullscreenTest, PopulateAppsCreatesAnotherPage) {
  Initialize(0, false, false);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  Show();

  ASSERT_EQ(2, GetPaginationModel()->total_pages());
}

// Tests that even if initialize is called again with a different initial page,
// that for fullscreen we always select the first page.
TEST_F(AppListViewFullscreenTest, MultiplePagesAlwaysReinitializeOnFirstPage) {
  Initialize(0, false, false);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  // Show and close the widget once.
  Show();
  view_->GetWidget()->Close();
  // Set it up again with a nonzero initial page.
  view_ = new AppListView(delegate_.get());
  AppListView::InitParams params;
  params.parent = GetContext();
  params.initial_apps_page = 1;
  view_->Initialize(params);
  const gfx::Size size = view_->bounds().size();
  view_->MaybeSetAnchorPoint(gfx::Point(size.width() / 2, size.height() / 2));
  Show();

  ASSERT_EQ(0, view_->GetAppsPaginationModel()->selected_page());
}

// Tests that pressing escape when in tablet search changes to fullscreen.
TEST_F(AppListViewFullscreenTest, EscapeKeyTabletModeSearchToFullscreen) {
  // Put into fullscreen using tablet mode.
  Initialize(0, true, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("yay"));
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that leaving tablet mode when in tablet search causes no change.
TEST_F(AppListViewFullscreenTest, LeaveTabletModeNoChange) {
  // Put into fullscreen using tablet mode.
  Initialize(0, true, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("something"));
  view_->OnTabletModeChanged(false);

  ASSERT_EQ(AppListView::FULLSCREEN_SEARCH, view_->app_list_state());
}

// Tests that escape works after leaving tablet mode from search.
TEST_F(AppListViewFullscreenTest, LeaveTabletModeEscapeKeyToFullscreen) {
  // Put into fullscreen using tablet mode.
  Initialize(0, true, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("nothing"));
  view_->OnTabletModeChanged(false);
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that escape twice closes after leaving tablet mode from search.
TEST_F(AppListViewFullscreenTest, LeaveTabletModeEscapeKeyTwiceToClosed) {
  // Put into fullscreen using tablet mode.
  Initialize(0, true, false);
  views::Textfield* search_box =
      view_->app_list_main_view()->search_box_view()->search_box();

  Show();
  search_box->SetText(base::string16());
  search_box->InsertText(base::UTF8ToUTF16("nothing"));
  view_->OnTabletModeChanged(false);
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  ASSERT_EQ(AppListView::CLOSED, view_->app_list_state());
}

// Tests that opening in peeking mode sets the correct height.
TEST_F(AppListViewFullscreenTest, OpenInPeekingCorrectHeight) {
  Initialize(0, false, false);

  Show();
  view_->SetState(AppListView::PEEKING);
  const views::Widget* widget = view_->get_fullscreen_widget_for_test();
  const int y = widget->GetWindowBoundsInScreen().y();

  ASSERT_EQ(kPeekingYValue, y);
}

// Tests that opening in peeking mode sets the correct height.
TEST_F(AppListViewFullscreenTest, OpenInFullscreenCorrectHeight) {
  Initialize(0, false, false);

  Show();
  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
  const views::Widget* widget = view_->get_fullscreen_widget_for_test();
  const int y = widget->GetWindowBoundsInScreen().y();

  ASSERT_EQ(0, y);
}

// Tests that AppListView::SetState fails when the state has been set to CLOSED.
TEST_F(AppListViewFullscreenTest, SetStateFailsWhenClosing) {
  Initialize(0, false, false);
  Show();
  view_->SetState(AppListView::CLOSED);

  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);

  ASSERT_EQ(AppListView::CLOSED, view_->app_list_state());
}

// Tests that when a click or tap event propagates to the AppListView, if the
// event location is within the bounds of AppsGridView, do not close the
// AppListView.
TEST_F(AppListViewFullscreenTest, TapAndClickWithinAppsGridView) {
  Initialize(0, false, false);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
  EXPECT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
  ContentsView* contents_view = view_->app_list_main_view()->contents_view();
  AppsContainerView* container_view = contents_view->apps_container_view();
  const gfx::Rect grid_view_bounds =
      container_view->apps_grid_view()->GetBoundsInScreen();
  gfx::Point target_point = grid_view_bounds.origin();
  target_point.Offset(100, 100);
  ASSERT_TRUE(grid_view_bounds.Contains(target_point));

  // Tests gesture tap within apps grid view doesn't close app list view.
  ui::GestureEvent tap(target_point.x(), target_point.y(), 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  view_->OnGestureEvent(&tap);
  EXPECT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());

  // Tests mouse click within apps grid view doesn't close app list view.
  ui::MouseEvent mouse_click(ui::ET_MOUSE_PRESSED, target_point, target_point,
                             base::TimeTicks(), 0, 0);
  view_->OnMouseEvent(&mouse_click);
  EXPECT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());
}

// Tests that search box should not become a rectangle during drag.
TEST_F(AppListViewFullscreenTest, SearchBoxCornerRadiusDuringDragging) {
  Initialize(0, false, false);
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  Show();
  view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
  EXPECT_EQ(AppListView::FULLSCREEN_ALL_APPS, view_->app_list_state());

  // Send SCROLL_START and SCROLL_UPDATE events, simulating dragging the
  // launcher.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Point start = view_->get_fullscreen_widget_for_test()
                               ->GetWindowBoundsInScreen()
                               .top_right();
  int delta_y = 0;
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  view_->OnGestureEvent(&start_event);

  // Drag down the launcher.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  delta_y += 10;
  ui::GestureEvent update_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  view_->OnGestureEvent(&update_event);

  EXPECT_TRUE(IsStateShown(AppListModel::STATE_APPS));
  EXPECT_EQ(kSearchBoxBorderCornerRadiusFullscreen,
            search_box_view()->GetSearchBoxBorderCornerRadiusForState(
                AppListModel::STATE_APPS));

  // Search box should keep |kSearchBoxCornerRadiusFullscreen| corner radius
  // during drag.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_SEARCH_RESULTS));
  EXPECT_TRUE(view_->is_in_drag());
  EXPECT_EQ(kSearchBoxBorderCornerRadiusFullscreen,
            search_box_view()->GetSearchBoxBorderCornerRadiusForState(
                AppListModel::STATE_SEARCH_RESULTS));

  // Ends to drag the launcher.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_APPS));
  timestamp += base::TimeDelta::FromMilliseconds(25);
  ui::GestureEvent end_event =
      ui::GestureEvent(start.x(), start.y() + delta_y, ui::EF_NONE, timestamp,
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  view_->OnGestureEvent(&end_event);

  // Search box should keep |kSearchBoxCornerRadiusFullscreen| corner radius
  // if launcher drag finished.
  EXPECT_FALSE(view_->is_in_drag());
  EXPECT_EQ(kSearchBoxBorderCornerRadiusFullscreen,
            search_box_view()->GetSearchBoxBorderCornerRadiusForState(
                AppListModel::STATE_APPS));
}

// Tests displaying the app list and performs a standard set of checks on its
// top level views. Then closes the window.
TEST_F(AppListViewTest, DisplayTest) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  Show();

  // Explicitly enforce the exact dimensions of the app list. Feel free to
  // change these if you need to (they are just here to prevent against
  // accidental changes to the window size).
  EXPECT_EQ("768x570", view_->bounds().size().ToString());

  EXPECT_EQ(2, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Checks on the main view.
  AppListMainView* main_view = view_->app_list_main_view();
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  AppListModel::State expected = AppListModel::STATE_START;
  EXPECT_TRUE(main_view->contents_view()->IsStateActive(expected));
  EXPECT_EQ(expected, delegate_->GetTestModel()->state());
}

// Tests that the main grid view is shown after hiding and reshowing the app
// list with a folder view open. This is a regression test for crbug.com/357058.
TEST_F(AppListViewTest, ReshowWithOpenFolderTest) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());

  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(kInitialItems);
  const std::string folder_id =
      model->MergeItems(model->top_level_item_list()->item_at(0)->id(),
                        model->top_level_item_list()->item_at(1)->id());

  AppListFolderItem* folder_item = model->FindFolderItem(folder_id);
  EXPECT_TRUE(folder_item);

  Show();

  // The main grid view should be showing initially.
  AppListMainView* main_view = view_->app_list_main_view();
  AppsContainerView* container_view =
      main_view->contents_view()->apps_container_view();
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(container_view->apps_grid_view()));
  EXPECT_FALSE(container_view->app_list_folder_view()->visible());

  AppsGridViewTestApi test_api(container_view->apps_grid_view());
  test_api.PressItemAt(0);

  // After pressing the folder item, the folder view should be showing.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(container_view->app_list_folder_view()));
  EXPECT_FALSE(container_view->apps_grid_view()->visible());

  view_->GetWidget()->Hide();
  EXPECT_FALSE(view_->GetWidget()->IsVisible());

  Show();

  // The main grid view should be showing after a reshow.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(container_view->apps_grid_view()));
  EXPECT_FALSE(container_view->app_list_folder_view()->visible());
}

// Tests that the start page view operates correctly.
TEST_F(AppListViewTest, StartPageTest) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(3);

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  StartPageView* start_page_view =
      main_view->contents_view()->start_page_view();
  // Checks on the main view.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));
  EXPECT_NO_FATAL_FAILURE(CheckView(start_page_view));

  // Show the start page view.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_START));
  gfx::Size view_size(view_->GetPreferredSize());

  // The "All apps" button should have its "parent background color" set
  // to the tiles container's background color.
  TileItemView* all_apps_button = start_page_view->all_apps_button();
  EXPECT_TRUE(all_apps_button->visible());
  EXPECT_EQ(kLabelBackgroundColor, all_apps_button->parent_background_color());

  // Simulate clicking the "All apps" button. Check that we navigate to the
  // apps grid view.
  SimulateClick(all_apps_button);
  main_view->contents_view()->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_APPS));

  // Hiding and showing the search box should not affect the app list's
  // preferred size. This is a regression test for http://crbug.com/386912.
  EXPECT_EQ(view_size.ToString(), view_->GetPreferredSize().ToString());

  // Check tiles hide and show on deletion and addition.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_START));
  model->results()->Add(base::MakeUnique<TestStartPageSearchResult>());
  start_page_view->UpdateForTesting();
  EXPECT_EQ(1u, GetVisibleViews(start_page_view->tile_views()));
  model->results()->DeleteAll();
  start_page_view->UpdateForTesting();
  EXPECT_EQ(0u, GetVisibleViews(start_page_view->tile_views()));

  // Tiles should not update when the start page is not active but should be
  // correct once the start page is shown.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_APPS));
  model->results()->Add(base::MakeUnique<TestStartPageSearchResult>());
  start_page_view->UpdateForTesting();
  EXPECT_EQ(0u, GetVisibleViews(start_page_view->tile_views()));
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_START));
  EXPECT_EQ(1u, GetVisibleViews(start_page_view->tile_views()));
}

// Tests switching rapidly between multiple pages of the launcher.
TEST_F(AppListViewTest, PageSwitchingAnimationTest) {
  AppListMainView* main_view = view_->app_list_main_view();
  // Checks on the main view.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  ContentsView* contents_view = main_view->contents_view();

  contents_view->SetActiveState(AppListModel::STATE_START);
  contents_view->Layout();

  IsStateShown(AppListModel::STATE_START);

  // Change pages. View should not have moved without Layout().
  contents_view->SetActiveState(AppListModel::STATE_SEARCH_RESULTS);
  IsStateShown(AppListModel::STATE_START);

  // Change to a third page. This queues up the second animation behind the
  // first.
  contents_view->SetActiveState(AppListModel::STATE_APPS);
  IsStateShown(AppListModel::STATE_START);

  // Call Layout(). Should jump to the third page.
  contents_view->Layout();
  IsStateShown(AppListModel::STATE_APPS);
}

// Tests that the correct views are displayed for showing search results.
TEST_F(AppListViewTest, SearchResultsTest) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(3);

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  ContentsView* contents_view = main_view->contents_view();
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_APPS));

  // Show the search results.
  contents_view->ShowSearchResults(true);
  contents_view->Layout();
  EXPECT_TRUE(contents_view->IsStateActive(AppListModel::STATE_SEARCH_RESULTS));

  EXPECT_TRUE(IsStateShown(AppListModel::STATE_SEARCH_RESULTS));

  // Hide the search results.
  contents_view->ShowSearchResults(false);
  contents_view->Layout();

  // Check that we return to the page that we were on before the search.
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_APPS));

  // Check that typing into the search box triggers the search page.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_START));
  view_->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_START));

  base::string16 search_text = base::UTF8ToUTF16("test");
  main_view->search_box_view()->search_box()->SetText(base::string16());
  main_view->search_box_view()->search_box()->InsertText(search_text);
  // Check that the current search is using |search_text|.
  EXPECT_EQ(search_text, delegate_->GetTestModel()->search_box()->text());
  EXPECT_EQ(search_text, main_view->search_box_view()->search_box()->text());
  contents_view->Layout();
  EXPECT_TRUE(contents_view->IsStateActive(AppListModel::STATE_SEARCH_RESULTS));
  EXPECT_TRUE(CheckSearchBoxWidget(contents_view->GetDefaultSearchBoxBounds()));

  // Check that typing into the search box triggers the search page.
  EXPECT_TRUE(SetAppListState(AppListModel::STATE_APPS));
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_APPS));
  EXPECT_TRUE(CheckSearchBoxWidget(contents_view->GetDefaultSearchBoxBounds()));

  base::string16 new_search_text = base::UTF8ToUTF16("apple");
  main_view->search_box_view()->search_box()->SetText(base::string16());
  main_view->search_box_view()->search_box()->InsertText(new_search_text);
  // Check that the current search is using |new_search_text|.
  EXPECT_EQ(new_search_text, delegate_->GetTestModel()->search_box()->text());
  EXPECT_EQ(new_search_text,
            main_view->search_box_view()->search_box()->text());
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_SEARCH_RESULTS));
  EXPECT_TRUE(CheckSearchBoxWidget(contents_view->GetDefaultSearchBoxBounds()));
}

// Tests that the back button navigates through the app list correctly.
TEST_F(AppListViewTest, BackTest) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  ContentsView* contents_view = main_view->contents_view();
  SearchBoxView* search_box_view = main_view->search_box_view();

  // Show the apps grid.
  SetAppListState(AppListModel::STATE_APPS);
  EXPECT_NO_FATAL_FAILURE(CheckView(search_box_view->back_button()));

  // The back button should return to the start page.
  EXPECT_TRUE(contents_view->Back());
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_START));
  EXPECT_FALSE(search_box_view->back_button()->visible());

  // Show the apps grid again.
  SetAppListState(AppListModel::STATE_APPS);
  EXPECT_NO_FATAL_FAILURE(CheckView(search_box_view->back_button()));

  // Pressing ESC should return to the start page.
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_START));
  EXPECT_FALSE(search_box_view->back_button()->visible());

  // Pressing ESC from the start page should close the app list.
  EXPECT_EQ(0, delegate_->dismiss_count());
  view_->AcceleratorPressed(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  EXPECT_EQ(1, delegate_->dismiss_count());

  // Show the search results.
  base::string16 new_search_text = base::UTF8ToUTF16("apple");
  search_box_view->search_box()->SetText(base::string16());
  search_box_view->search_box()->InsertText(new_search_text);
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_SEARCH_RESULTS));
  EXPECT_NO_FATAL_FAILURE(CheckView(search_box_view->back_button()));

  // The back button should return to the start page.
  EXPECT_TRUE(contents_view->Back());
  contents_view->Layout();
  EXPECT_TRUE(IsStateShown(AppListModel::STATE_START));
  EXPECT_FALSE(search_box_view->back_button()->visible());
}

// Tests that the correct views are displayed for showing search results.
TEST_F(AppListViewTest, AppListOverlayTest) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  SearchBoxView* search_box_view = main_view->search_box_view();

  // The search box should not be enabled when the app list overlay is shown.
  view_->SetAppListOverlayVisible(true);
  EXPECT_FALSE(search_box_view->enabled());

  // The search box should be refocused when the app list overlay is hidden.
  view_->SetAppListOverlayVisible(false);
  EXPECT_TRUE(search_box_view->enabled());
  EXPECT_EQ(search_box_view->search_box(),
            view_->GetWidget()->GetFocusManager()->GetFocusedView());
}

// Tests that even if initialize is called again with a different initial page,
// that different initial page is respected.
TEST_F(AppListViewTest, MultiplePagesReinitializeOnInputPage) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (features::IsFullscreenAppListEnabled())
    return;

  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  // Show and close the widget once.
  Show();
  view_->GetWidget()->Close();
  // Set it up again with a nonzero initial page.
  view_ = new AppListView(delegate_.get());
  AppListView::InitParams params;
  params.parent = GetContext();
  params.initial_apps_page = 1;
  view_->Initialize(params);
  const gfx::Size size = view_->bounds().size();
  view_->MaybeSetAnchorPoint(gfx::Point(size.width() / 2, size.height() / 2));
  Show();

  ASSERT_EQ(1, view_->GetAppsPaginationModel()->selected_page());
}

}  // namespace test
}  // namespace app_list
