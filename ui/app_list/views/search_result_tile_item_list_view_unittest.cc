// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_list_view.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {

namespace {
constexpr int kMaxNumSearchResultTiles = 6;
constexpr int kInstalledApps = 4;
constexpr int kPlayStoreApps = 2;
}  // namespace

class SearchResultTileItemListViewTest
    : public views::ViewsTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SearchResultTileItemListViewTest() = default;
  ~SearchResultTileItemListViewTest() override = default;

 protected:
  void CreateSearchResultTileItemListView() {
    // Enable fullscreen app list for parameterized Play Store app search
    // feature.
    if (IsPlayStoreAppSearchEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kEnableFullscreenAppList,
           features::kEnablePlayStoreAppSearch},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kEnableFullscreenAppList},
          {features::kEnablePlayStoreAppSearch});
    }
    ASSERT_EQ(IsPlayStoreAppSearchEnabled(),
              features::IsPlayStoreAppSearchEnabled());

    // Sets up the views.
    textfield_ = base::MakeUnique<views::Textfield>();
    view_ = base::MakeUnique<SearchResultTileItemListView>(
        nullptr, textfield_.get(), &view_delegate_);
    view_->SetResults(view_delegate_.GetModel()->results());
  }

  bool IsPlayStoreAppSearchEnabled() const { return GetParam(); }

  SearchResultTileItemListView* view() { return view_.get(); }

  AppListModel::SearchResults* GetResults() {
    return view_delegate_.GetModel()->results();
  }

  void SetUpSearchResults() {
    AppListModel::SearchResults* results = GetResults();

    // Populate results for installed applications.
    for (int i = 0; i < kInstalledApps; ++i) {
      std::unique_ptr<TestSearchResult> result =
          base::MakeUnique<TestSearchResult>();
      result->set_display_type(SearchResult::DISPLAY_TILE);
      result->set_result_type(SearchResult::RESULT_INSTALLED_APP);
      result->set_title(
          base::UTF8ToUTF16(base::StringPrintf("InstalledApp %d", i)));
      results->Add(std::move(result));
    }

    // Populate results for Play Store search applications.
    if (IsPlayStoreAppSearchEnabled()) {
      for (int i = 0; i < kPlayStoreApps; ++i) {
        std::unique_ptr<TestSearchResult> result =
            base::MakeUnique<TestSearchResult>();
        result->set_display_type(SearchResult::DISPLAY_TILE);
        result->set_result_type(SearchResult::RESULT_PLAYSTORE_APP);
        result->set_title(
            base::UTF8ToUTF16(base::StringPrintf("PlayStoreApp %d", i)));
        result->SetRating(1 + i);
        result->SetFormattedPrice(
            base::UTF8ToUTF16(base::StringPrintf("Price %d", i)));
        results->Add(std::move(result));
      }
    }

    // Adding results calls SearchResultContainerView::ScheduleUpdate().
    // It will post a delayed task to update the results and relayout.
    RunPendingMessages();
    view_->OnContainerSelected(false, false);
  }

  int GetOpenResultCount(int ranking) {
    int result = view_delegate_.open_search_result_counts()[ranking];
    return result;
  }

  void ResetOpenResultCount() {
    view_delegate_.open_search_result_counts().clear();
  }

  int GetResultCount() const { return view_->num_results(); }

  int GetSelectedIndex() const { return view_->selected_index(); }

  void ResetSelectedIndex() const { view_->SetSelectedIndex(0); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return view_->OnKeyPressed(event);
  }

 private:
  test::AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultTileItemListView> view_;
  std::unique_ptr<views::Textfield> textfield_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemListViewTest);
};

TEST_P(SearchResultTileItemListViewTest, Basic) {
  CreateSearchResultTileItemListView();
  SetUpSearchResults();

  const int results = GetResultCount();
  const int expected_results = IsPlayStoreAppSearchEnabled()
                                   ? kInstalledApps + kPlayStoreApps
                                   : kInstalledApps;
  EXPECT_EQ(expected_results, results);
  // When the Play Store app search feature is enabled, for each results,
  // we added a separator for result type grouping.
  const int expected_child_count = IsPlayStoreAppSearchEnabled()
                                       ? kMaxNumSearchResultTiles * 2
                                       : kMaxNumSearchResultTiles;
  EXPECT_EQ(expected_child_count, view()->child_count());

  /// Test accessibility descriptions of tile views.
  const int first_child = IsPlayStoreAppSearchEnabled() ? 1 : 0;
  const int child_step = IsPlayStoreAppSearchEnabled() ? 2 : 1;

  for (int i = 0; i < kInstalledApps; ++i) {
    ui::AXNodeData node_data;
    view()
        ->child_at(first_child + i * child_step)
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ui::AX_ROLE_BUTTON, node_data.role);
    EXPECT_EQ(base::StringPrintf("InstalledApp %d", i),
              node_data.GetStringAttribute(ui::AX_ATTR_NAME));
  }

  for (int i = kInstalledApps; i < expected_results; ++i) {
    ui::AXNodeData node_data;
    view()
        ->child_at(first_child + i * child_step)
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ui::AX_ROLE_BUTTON, node_data.role);
    EXPECT_EQ(base::StringPrintf("PlayStoreApp %d, Star rating %d.0, Price %d",
                                 i - kInstalledApps, i + 1 - kInstalledApps,
                                 i - kInstalledApps),
              node_data.GetStringAttribute(ui::AX_ATTR_NAME));
  }

  // Tests item indexing by pressing TAB.
  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
    EXPECT_EQ(i, GetSelectedIndex());
  }

  // Extra TAB events won't be handled by the view.
  EXPECT_FALSE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(results - 1, GetSelectedIndex());

  // Tests app opening.
  ResetSelectedIndex();
  ResetOpenResultCount();
  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
    EXPECT_EQ(i, GetSelectedIndex());
    for (int j = 0; j < i; j++)
      EXPECT_TRUE(KeyPress(ui::VKEY_RETURN));
    EXPECT_EQ(i, GetOpenResultCount(i));
  }
}

INSTANTIATE_TEST_CASE_P(, SearchResultTileItemListViewTest, testing::Bool());

}  // namespace app_list
