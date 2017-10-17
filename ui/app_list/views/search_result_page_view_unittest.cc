// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"

namespace {

enum class AnswerCardState {
  ANSWER_CARD_OFF,
  ANSWER_CARD_ON_WITH_RESULT,
  ANSWER_CARD_ON_WITHOUT_RESULT,
};

}  // namespace

namespace app_list {
namespace test {

class SearchResultPageViewTest : public views::ViewsTestBase,
                                 public testing::WithParamInterface<
                                     ::testing::tuple<bool, AnswerCardState>> {
 public:
  SearchResultPageViewTest() = default;
  ~SearchResultPageViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Reading test parameters.
    bool test_with_answer_card = true;
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      test_with_fullscreen_ = testing::get<0>(GetParam());
      const AnswerCardState answer_card_state = testing::get<1>(GetParam());
      test_with_answer_card =
          answer_card_state != AnswerCardState::ANSWER_CARD_OFF;
      test_with_answer_card_result_ =
          answer_card_state == AnswerCardState::ANSWER_CARD_ON_WITH_RESULT;
    }

    // Setting up the feature set.
    if (test_with_fullscreen_) {
      if (test_with_answer_card) {
        scoped_feature_list_.InitWithFeatures(
            {features::kEnableFullscreenAppList, features::kEnableAnswerCard},
            {});
      } else {
        scoped_feature_list_.InitWithFeatures(
            {features::kEnableFullscreenAppList},
            {features::kEnableAnswerCard});
      }
    } else {
      if (test_with_answer_card) {
        scoped_feature_list_.InitWithFeatures(
            {features::kEnableAnswerCard},
            {features::kEnableFullscreenAppList});
      } else {
        scoped_feature_list_.InitWithFeatures(
            {},
            {features::kEnableFullscreenAppList, features::kEnableAnswerCard});
      }
    }
    ASSERT_EQ(test_with_fullscreen_, features::IsFullscreenAppListEnabled());
    ASSERT_EQ(test_with_answer_card, features::IsAnswerCardEnabled());

    // Setting up views.
    delegate_.reset(new AppListTestViewDelegate);
    app_list_view_ = new AppListView(delegate_.get());
    AppListView::InitParams params;
    params.parent = GetContext();
    app_list_view_->Initialize(params);
    // TODO(warx): remove MaybeSetAnchorPoint setup when bubble launcher is
    // removed from code base.
    app_list_view_->MaybeSetAnchorPoint(
        params.parent->GetBoundsInRootWindow().CenterPoint());
    app_list_view_->GetWidget()->Show();

    ContentsView* contents_view =
        app_list_view_->app_list_main_view()->contents_view();
    view_ = contents_view->search_results_page_view();
    tile_list_view_ =
        contents_view->search_result_tile_item_list_view_for_test();
    list_view_ = contents_view->search_result_list_view_for_test();
  }
  void TearDown() override {
    app_list_view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
  }

 protected:
  SearchResultPageView* view() const { return view_; }

  SearchResultTileItemListView* tile_list_view() const {
    return tile_list_view_;
  }
  SearchResultListView* list_view() const { return list_view_; }

  AppListModel::SearchResults* GetResults() const {
    return delegate_->GetModel()->results();
  }

  void SetUpSearchResults(
      const std::vector<std::pair<SearchResult::DisplayType, int>>&
          result_types) {
    AppListModel::SearchResults* results = GetResults();
    results->DeleteAll();
    double relevance = result_types.size();
    for (const auto& data : result_types) {
      // Set the relevance of the results in each group in decreasing order (so
      // the earlier groups have higher relevance, and therefore appear first).
      relevance -= 1.0;
      for (int i = 0; i < data.second; ++i) {
        std::unique_ptr<TestSearchResult> result =
            std::make_unique<TestSearchResult>();
        result->set_display_type(data.first);
        result->set_relevance(relevance);
        results->Add(std::move(result));
      }
    }

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  // Add search results for test on focus movement.
  void SetUpFocusTestEnv() {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    // 3 tile results, followed by 2 list results.
    const int kTileResults = 3;
    const int kListResults = 2;
    const int kNoneResults = 3;
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_TILE, kTileResults));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_LIST, kListResults));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_NONE, kNoneResults));
    SetUpSearchResults(result_types);
  }

  int GetSelectedIndex() const { return view_->selected_index(); }

  bool KeyPress(ui::KeyboardCode key_code) { return KeyPress(key_code, false); }

  bool KeyPress(ui::KeyboardCode key_code, bool shift_down) {
    int flags = ui::EF_NONE;
    if (shift_down)
      flags |= ui::EF_SHIFT_DOWN;
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags);
    return view_->OnKeyPressed(event);
  }

  bool test_with_fullscreen() const { return test_with_fullscreen_; }
  bool test_with_answer_card_result() const {
    return test_with_answer_card_result_;
  }

 private:
  AppListView* app_list_view_ = nullptr;  // Owned by native widget.
  SearchResultPageView* view_ = nullptr;  // Owned by views hierarchy.
  SearchResultTileItemListView* tile_list_view_ =
      nullptr;                                 // Owned by views hierarchy.
  SearchResultListView* list_view_ = nullptr;  // Owned by views hierarchy.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  bool test_with_fullscreen_ = true;
  bool test_with_answer_card_result_ = true;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageViewTest);
};

// Instantiate the Boolean which is used to toggle the Fullscreen app list in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(
    ,
    SearchResultPageViewTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(AnswerCardState::ANSWER_CARD_OFF,
                          AnswerCardState::ANSWER_CARD_ON_WITHOUT_RESULT,
                          AnswerCardState::ANSWER_CARD_ON_WITH_RESULT)));

// TODO(warx): This test applies to bubble launcher only. Remove this test once
// bubble launcher is removed from code base.
TEST_P(SearchResultPageViewTest, DirectionalMovement) {
  if (test_with_fullscreen())
    return;

  SetUpFocusTestEnv();
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());

  // Navigate to the second tile in the tile group.
  EXPECT_TRUE(KeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(1, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate to the list group.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());

  // Navigate to the second result in the list view.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Attempt to navigate off bottom of list items.
  EXPECT_FALSE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Navigate back to the tile group (should select the first tile result).
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(0, list_view()->selected_index());
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate off top of list.
  EXPECT_FALSE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
}

TEST_P(SearchResultPageViewTest, TabMovement) {
  SetUpFocusTestEnv();
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());

  // Navigate to the second tile in the tile group.
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(1, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate to the list group.
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(2, tile_list_view()->selected_index());
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());

  // Navigate to the second result in the list view.
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Attempt to navigate off bottom of list items.
  EXPECT_FALSE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Navigate back to the tile group (should select the last tile result).
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(0, list_view()->selected_index());
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(2, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate off top of list.
  if (test_with_fullscreen()) {
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
    EXPECT_FALSE(KeyPress(ui::VKEY_TAB, true));
    EXPECT_EQ(-1, GetSelectedIndex());
    EXPECT_EQ(-1, tile_list_view()->selected_index());
    EXPECT_EQ(-1, list_view()->selected_index());
  } else {
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
    EXPECT_EQ(1, tile_list_view()->selected_index());
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
    EXPECT_EQ(0, tile_list_view()->selected_index());
    EXPECT_FALSE(KeyPress(ui::VKEY_TAB, true));
    EXPECT_EQ(0, GetSelectedIndex());
    EXPECT_EQ(0, tile_list_view()->selected_index());
  }
}

TEST_P(SearchResultPageViewTest, ResultsSorted) {
  AppListModel::SearchResults* results = GetResults();

  // Add 3 results and expect the tile list view to be the first result
  // container view.
  TestSearchResult* tile_result = new TestSearchResult();
  tile_result->set_display_type(SearchResult::DISPLAY_TILE);
  tile_result->set_relevance(1.0);
  results->Add(base::WrapUnique(tile_result));
  {
    TestSearchResult* list_result = new TestSearchResult();
    list_result->set_display_type(SearchResult::DISPLAY_LIST);
    list_result->set_relevance(0.5);
    results->Add(base::WrapUnique(list_result));
  }
  {
    TestSearchResult* list_result = new TestSearchResult();
    list_result->set_display_type(SearchResult::DISPLAY_LIST);
    list_result->set_relevance(0.3);
    results->Add(base::WrapUnique(list_result));
  }

  // Adding results will schedule Update().
  RunPendingMessages();

  EXPECT_EQ(tile_list_view(), view()->result_container_views()[0]);
  EXPECT_EQ(list_view(), view()->result_container_views()[1]);

  // Change the relevance of the tile result and expect the list results to be
  // displayed first.
  // TODO(warx): fullscreen launcher should always have tile list view to be
  // displayed first over list view.
  tile_result->set_relevance(0.4);

  results->NotifyItemsChanged(0, 1);
  RunPendingMessages();

  EXPECT_EQ(list_view(), view()->result_container_views()[0]);
  EXPECT_EQ(tile_list_view(), view()->result_container_views()[1]);
}

TEST_P(SearchResultPageViewTest, UpdateWithSelection) {
  const int kCardResultNum = test_with_answer_card_result() ? 1 : 0;
  {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_TILE, 3));
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_LIST, 2));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_CARD, kCardResultNum));

    SetUpSearchResults(result_types);
  }

  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate to the second result in the list group.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());

  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(1, list_view()->selected_index());

  {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_TILE, 3));
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_LIST, 3));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_CARD, kCardResultNum));

    SetUpSearchResults(result_types);
  }

  // The second list result should still be selected after the update.
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(1, list_view()->selected_index());

  {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_TILE, 3));
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_LIST, 1));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_CARD, kCardResultNum));

    SetUpSearchResults(result_types);
  }

  // The first list result should be selected after the update as the second
  // result has vanished.
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());

  {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_LIST, 1));
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_TILE, 3));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_CARD, kCardResultNum));

    SetUpSearchResults(result_types);
  }

  // The tile container should be selected because we hold the selected
  // container index constant.
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_LIST, 3));
    result_types.push_back(
        std::make_pair(SearchResult::DISPLAY_CARD, kCardResultNum));

    SetUpSearchResults(result_types);
  }

  // The selected container has vanished so we reset the selection to 0.
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());
}

using SearchResultPageViewFullscreenTest = SearchResultPageViewTest;

TEST_F(SearchResultPageViewFullscreenTest, LeftRightMovement) {
  SetUpFocusTestEnv();
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate to the second tile in the tile group.
  EXPECT_TRUE(KeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(1, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate to the list group.
  EXPECT_TRUE(KeyPress(ui::VKEY_RIGHT));
  EXPECT_TRUE(KeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());

  // Navigate to the second result in the list view.
  EXPECT_TRUE(KeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Attempt to navigate off bottom of list items.
  EXPECT_FALSE(KeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Navigate back to the tile group (should select the last tile result).
  EXPECT_TRUE(KeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(0, list_view()->selected_index());
  EXPECT_TRUE(KeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(2, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate off top of list.
  EXPECT_TRUE(KeyPress(ui::VKEY_LEFT));
  EXPECT_TRUE(KeyPress(ui::VKEY_LEFT));
  EXPECT_FALSE(KeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(-1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());
}

TEST_F(SearchResultPageViewFullscreenTest, UpDownMovement) {
  SetUpFocusTestEnv();
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate to the first result in the list view.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());

  // Navigate to the second result in the list view.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Attempt to navigate off bottom of list items.
  EXPECT_FALSE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Navigate back to the tile group (should select the first tile result).
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(0, list_view()->selected_index());
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());

  // Navigate off top of list.
  EXPECT_FALSE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(-1, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(-1, list_view()->selected_index());
}

}  // namespace test
}  // namespace app_list
