// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include <map>

#include "ui/app_list/app_list_model.h"
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
    tile_list_view_ = new SearchResultTileItemListView(textfield_.get());
    view_->AddSearchResultContainerView(GetResults(), tile_list_view_);
  }

 protected:
  SearchResultPageView* view() { return view_.get(); }

  SearchResultListView* list_view() { return list_view_; }
  SearchResultTileItemListView* tile_list_view() { return tile_list_view_; }

  AppListModel::SearchResults* GetResults() {
    return view_delegate_.GetModel()->results();
  }

  void SetUpSearchResults(
      const std::map<SearchResult::DisplayType, int> result_types) {
    AppListModel::SearchResults* results = GetResults();
    for (const auto& data : result_types) {
      for (int i = 0; i < data.second; ++i) {
        TestSearchResult* result = new TestSearchResult();
        result->SetDisplayType(data.first);
        results->Add(result);
      }
    }

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  int GetSelectedIndex() { return view_->selected_index(); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return view_->OnKeyPressed(event);
  }

 private:
  void OnResultInstalled(SearchResult* result) override {}

  SearchResultListView* list_view_;
  SearchResultTileItemListView* tile_list_view_;

  AppListTestViewDelegate view_delegate_;
  scoped_ptr<SearchResultPageView> view_;
  scoped_ptr<views::Textfield> textfield_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageViewTest);
};

TEST_F(SearchResultPageViewTest, Basic) {
  std::map<SearchResult::DisplayType, int> result_types;
  const int kListResults = 2;
  const int kTileResults = 1;
  const int kNoneResults = 3;
  result_types[SearchResult::DISPLAY_LIST] = kListResults;
  result_types[SearchResult::DISPLAY_TILE] = kTileResults;
  result_types[SearchResult::DISPLAY_NONE] = kNoneResults;

  SetUpSearchResults(result_types);
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(0, list_view()->selected_index());

  // Navigate to the second result in the list view.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());

  // Navigate to the tile group.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(-1, list_view()->selected_index());
  EXPECT_EQ(0, tile_list_view()->selected_index());

  // Navigate off bottom of tile items.
  EXPECT_FALSE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(1, GetSelectedIndex());
  EXPECT_EQ(0, tile_list_view()->selected_index());

  // Navigate back to the list group.
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(1, list_view()->selected_index());
  EXPECT_EQ(-1, tile_list_view()->selected_index());

  // Navigate off top of list.
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, list_view()->selected_index());
  EXPECT_FALSE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, list_view()->selected_index());
  EXPECT_EQ(0, GetSelectedIndex());
}

}  // namespace test
}  // namespace app_list
