// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace test {

namespace {
int kDefaultSearchItems = 5;
}  // namespace

class SearchResultListViewTest : public views::test::WidgetTest {
 public:
  SearchResultListViewTest() = default;
  ~SearchResultListViewTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    views::test::WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
    view_ = std::make_unique<SearchResultListView>(nullptr, &view_delegate_);
    widget_->SetBounds(gfx::Rect(0, 0, 300, 200));
    widget_->GetContentsView()->AddChildView(view_.get());
    widget_->Show();
    view_->SetResults(view_delegate_.GetSearchModel()->results());
  }

  void TearDown() override {
    view_.reset();
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  SearchResultListView* view() const { return view_.get(); }

  SearchResultView* GetResultViewAt(int index) const {
    return view_->GetResultViewAt(index);
  }

  std::vector<SearchResultView*> GetAssistantResultViews() const {
    std::vector<SearchResultView*> results;
    for (auto* view : view_->search_result_views_) {
      auto* result = view->result();
      if (result &&
          result->result_type() == AppListSearchResultType::kAssistantText)
        results.push_back(view);
    }
    return results;
  }

  SearchModel::SearchResults* GetResults() {
    return view_delegate_.GetSearchModel()->results();
  }

  void AddAssistantSearchResult() {
    SearchModel::SearchResults* results = GetResults();

    std::unique_ptr<TestSearchResult> assistant_result =
        std::make_unique<TestSearchResult>();
    assistant_result->set_result_type(
        ash::AppListSearchResultType::kAssistantText);
    assistant_result->set_display_type(ash::SearchResultDisplayType::kList);
    assistant_result->set_title(u"assistant result");
    results->Add(std::move(assistant_result));

    RunPendingMessages();
  }

  void SetUpSearchResults() {
    SearchModel::SearchResults* results = GetResults();
    for (int i = 0; i < kDefaultSearchItems; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_display_type(ash::SearchResultDisplayType::kList);
      result->set_title(base::UTF8ToUTF16(base::StringPrintf("Result %d", i)));
      if (i < 2)
        result->set_details(u"Detail");
      results->Add(std::move(result));
    }

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  int GetOpenResultCountAndReset(int ranking) {
    EXPECT_GT(view_delegate_.open_search_result_counts().count(ranking), 0u);
    int result = view_delegate_.open_search_result_counts()[ranking];
    view_delegate_.open_search_result_counts().clear();
    return result;
  }

  int GetResultCount() const { return view_->num_results(); }

  void AddTestResultAtIndex(int index) {
    GetResults()->Add(std::make_unique<TestSearchResult>());
  }

  void DeleteResultAt(int index) { GetResults()->DeleteAt(index); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return view_->OnKeyPressed(event);
  }

  void ExpectConsistent() {
    // Adding results will schedule Update().
    RunPendingMessages();

    SearchModel::SearchResults* results = GetResults();
    for (size_t i = 0; i < results->item_count(); ++i) {
      EXPECT_EQ(results->GetItemAt(i), GetResultViewAt(i)->result());
    }
  }

 private:
  TestAppListColorProvider color_provider_;  // Needed by AppListView.
  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultListView> view_;
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultListViewTest);
};

TEST_F(SearchResultListViewTest, SpokenFeedback) {
  SetUpSearchResults();

  // Result 0 has a detail text. Expect that the detail is appended to the
  // accessibility name.
  EXPECT_EQ(u"Result 0, Detail", GetResultViewAt(0)->ComputeAccessibleName());

  // Result 2 has no detail text.
  EXPECT_EQ(u"Result 2", GetResultViewAt(2)->ComputeAccessibleName());
}

TEST_F(SearchResultListViewTest, ModelObservers) {
  SetUpSearchResults();
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems - 1);
  ExpectConsistent();

  // Insert at start.
  AddTestResultAtIndex(0);
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems - 1);
  ExpectConsistent();

  // Insert at end.
  AddTestResultAtIndex(kDefaultSearchItems);
  ExpectConsistent();

  // Delete from start.
  DeleteResultAt(0);
  ExpectConsistent();
}

TEST_F(SearchResultListViewTest, HidesAssistantResultWhenTilesVisible) {
  SetUpSearchResults();

  // No assistant results available.
  EXPECT_TRUE(GetAssistantResultViews().empty());

  AddAssistantSearchResult();

  // Assistant result should be set and visible.
  for (const auto* view : GetAssistantResultViews()) {
    EXPECT_TRUE(view->GetVisible());
    EXPECT_EQ(view->result()->title(), u"assistant result");
  }

  // Add a tile result
  std::unique_ptr<TestSearchResult> tile_result =
      std::make_unique<TestSearchResult>();
  tile_result->set_display_type(ash::SearchResultDisplayType::kTile);
  GetResults()->Add(std::move(tile_result));

  RunPendingMessages();

  // Assistant result should be gone.
  EXPECT_TRUE(GetAssistantResultViews().empty());
}

}  // namespace test
}  // namespace ash
