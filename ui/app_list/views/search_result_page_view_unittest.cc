// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_list_view_delegate.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

class SearchResultPageViewTest : public views::ViewsTestBase,
                                 public SearchResultListViewDelegate {
 public:
  SearchResultPageViewTest() {}
  ~SearchResultPageViewTest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    view_.reset(new SearchResultPageView());
    list_view_ = new SearchResultListView(this, &view_delegate_);
    view_->AddSearchResultContainerView(GetResults(), list_view_);
    textfield_.reset(new views::Textfield());
    tile_list_view_ =
        new SearchResultTileItemListView(textfield_.get(), &view_delegate_);
    view_->AddSearchResultContainerView(GetResults(), tile_list_view_);
  }

 protected:
  SearchResultPageView* view() { return view_.get(); }

  SearchResultListView* list_view() { return list_view_; }
  SearchResultTileItemListView* tile_list_view() { return tile_list_view_; }

  AppListModel::SearchResults* GetResults() {
    return view_delegate_.GetModel()->results();
  }

  void SetUpSearchResults(const std::vector<
      std::pair<SearchResult::DisplayType, int>> result_types) {
    AppListModel::SearchResults* results = GetResults();
    results->DeleteAll();
    double relevance = result_types.size();
    for (const auto& data : result_types) {
      // Set the relevance of the results in each group in decreasing order (so
      // the earlier groups have higher relevance, and therefore appear first).
      relevance -= 1.0;
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

  int GetSelectedIndex() { return view_->selected_index(); }

  bool KeyPress(ui::KeyboardCode key_code) { return KeyPress(key_code, false); }

  bool KeyPress(ui::KeyboardCode key_code, bool shift_down) {
    int flags = ui::EF_NONE;
    if (shift_down)
      flags |= ui::EF_SHIFT_DOWN;
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags);
    return view_->OnKeyPressed(event);
  }

 private:
  void OnResultInstalled(SearchResult* result) override {}

  SearchResultListView* list_view_;
  SearchResultTileItemListView* tile_list_view_;

  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultPageView> view_;
  std::unique_ptr<views::Textfield> textfield_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageViewTest);
};

TEST_F(SearchResultPageViewTest, DirectionalMovement) {
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

TEST_F(SearchResultPageViewTest, TabMovement) {
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
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
  EXPECT_EQ(1, tile_list_view()->selected_index());
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB, true));
  EXPECT_EQ(0, tile_list_view()->selected_index());
  EXPECT_FALSE(KeyPress(ui::VKEY_TAB, true));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());
}

TEST_F(SearchResultPageViewTest, ResultsSorted) {
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
  tile_result->set_relevance(0.4);

  results->NotifyItemsChanged(0, 1);
  RunPendingMessages();

  EXPECT_EQ(list_view(), view()->result_container_views()[0]);
  EXPECT_EQ(tile_list_view(), view()->result_container_views()[1]);
}

TEST_F(SearchResultPageViewTest, UpdateWithSelection) {
  {
    std::vector<std::pair<SearchResult::DisplayType, int>> result_types;
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_TILE, 3));
    result_types.push_back(std::make_pair(SearchResult::DISPLAY_LIST, 2));

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

    SetUpSearchResults(result_types);
  }

  // The selected container has vanished so we reset the selection to 0.
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(-1, tile_list_view()->selected_index());
  EXPECT_EQ(0, list_view()->selected_index());
}

}  // namespace test
}  // namespace app_list
