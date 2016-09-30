// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_list_view.h"

#include <stddef.h>

#include <map>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/search_result_list_view_delegate.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

namespace {
int kDefaultSearchItems = 5;
}  // namespace

class SearchResultListViewTest : public views::ViewsTestBase,
                                 public SearchResultListViewDelegate {
 public:
  SearchResultListViewTest() {}
  ~SearchResultListViewTest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    view_.reset(new SearchResultListView(this, &view_delegate_));
    view_->SetResults(view_delegate_.GetModel()->results());
  }

 protected:
  SearchResultListView* view() { return view_.get(); }

  SearchResultView* GetResultViewAt(int index) {
    return view_->GetResultViewAt(index);
  }

  AppListModel::SearchResults* GetResults() {
    return view_delegate_.GetModel()->results();
  }

  void SetLongAutoLaunchTimeout() {
    // Sets a long timeout that lasts longer than the test run.
    view_delegate_.set_auto_launch_timeout(base::TimeDelta::FromDays(1));
  }

  base::TimeDelta GetAutoLaunchTimeout() {
    return view_delegate_.GetAutoLaunchTimeout();
  }

  void SetUpSearchResults() {
    AppListModel::SearchResults* results = GetResults();
    for (int i = 0; i < kDefaultSearchItems; ++i) {
      std::unique_ptr<TestSearchResult> result =
          base::MakeUnique<TestSearchResult>();
      result->set_display_type(SearchResult::DISPLAY_LIST);
      result->set_title(base::UTF8ToUTF16(base::StringPrintf("Result %d", i)));
      if (i < 2)
        result->set_details(base::ASCIIToUTF16("Detail"));
      results->Add(std::move(result));
    }

    // Adding results will schedule Update().
    RunPendingMessages();
    view_->OnContainerSelected(false, false);
  }

  int GetOpenResultCountAndReset(int ranking) {
    int result = view_delegate_.open_search_result_counts()[ranking];
    view_delegate_.open_search_result_counts().clear();
    return result;
  }

  int GetResultCount() { return view_->num_results(); }

  int GetSelectedIndex() { return view_->selected_index(); }

  void ResetSelectedIndex() {
    view_->SetSelectedIndex(0);
  }

  void AddTestResultAtIndex(int index) {
    GetResults()->Add(base::MakeUnique<TestSearchResult>());
  }

  void DeleteResultAt(int index) { GetResults()->DeleteAt(index); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return view_->OnKeyPressed(event);
  }

  bool IsAutoLaunching() { return !!view_->auto_launch_animation_; }

  void ForceAutoLaunch() {
    view_->ForceAutoLaunchForTest();
  }

  void ExpectConsistent() {
    // Adding results will schedule Update().
    RunPendingMessages();

    AppListModel::SearchResults* results = GetResults();
    for (size_t i = 0; i < results->item_count(); ++i) {
      EXPECT_EQ(results->GetItemAt(i), GetResultViewAt(i)->result());
    }
  }

  views::ProgressBar* GetProgressBarAt(size_t index) {
    return GetResultViewAt(index)->progress_bar_;
  }

 private:
  void OnResultInstalled(SearchResult* result) override {}

  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultListView> view_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultListViewTest);
};

TEST_F(SearchResultListViewTest, Basic) {
  SetUpSearchResults();

  const int results = GetResultCount();
  EXPECT_EQ(kDefaultSearchItems, results);
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_FALSE(IsAutoLaunching());

  EXPECT_TRUE(KeyPress(ui::VKEY_RETURN));
  EXPECT_EQ(1, GetOpenResultCountAndReset(0));

  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
    EXPECT_EQ(i, GetSelectedIndex());
  }
  // When navigating off the end of the list, pass the event to the parent to
  // handle.
  EXPECT_FALSE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(results - 1, GetSelectedIndex());

  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_UP));
    EXPECT_EQ(results - i - 1, GetSelectedIndex());
  }
  // Navigate off top of list.
  EXPECT_FALSE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());
  ResetSelectedIndex();

  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
    EXPECT_EQ(i, GetSelectedIndex());
  }
  // Navigate off bottom of list.
  EXPECT_FALSE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(results - 1, GetSelectedIndex());
}

TEST_F(SearchResultListViewTest, AutoLaunch) {
  SetLongAutoLaunchTimeout();
  SetUpSearchResults();

  EXPECT_TRUE(IsAutoLaunching());
  ForceAutoLaunch();

  EXPECT_FALSE(IsAutoLaunching());
  EXPECT_EQ(1, GetOpenResultCountAndReset(0));

  // The timeout has to be cleared after the auto-launch, to prevent opening
  // the search result twice. See the comment in AnimationEnded().
  EXPECT_EQ(base::TimeDelta(), GetAutoLaunchTimeout());
}

TEST_F(SearchResultListViewTest, CancelAutoLaunch) {
  SetLongAutoLaunchTimeout();
  SetUpSearchResults();

  EXPECT_TRUE(IsAutoLaunching());

  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_FALSE(IsAutoLaunching());

  SetLongAutoLaunchTimeout();
  view()->UpdateAutoLaunchState();
  EXPECT_TRUE(IsAutoLaunching());

  view()->SetVisible(false);
  EXPECT_FALSE(IsAutoLaunching());

  SetLongAutoLaunchTimeout();
  view()->SetVisible(true);
  EXPECT_TRUE(IsAutoLaunching());
}

TEST_F(SearchResultListViewTest, SpokenFeedback) {
  SetUpSearchResults();

  // Result 0 has a detail text. Expect that the detail is appended to the
  // accessibility name.
  EXPECT_EQ(base::ASCIIToUTF16("Result 0, Detail"),
            GetResultViewAt(0)->ComputeAccessibleName());

  // Result 2 has no detail text.
  EXPECT_EQ(base::ASCIIToUTF16("Result 2"),
            GetResultViewAt(2)->ComputeAccessibleName());
}

TEST_F(SearchResultListViewTest, ModelObservers) {
  SetUpSearchResults();
  ExpectConsistent();

  // Insert at start.
  AddTestResultAtIndex(0);
  ExpectConsistent();

  // Remove from end.
  DeleteResultAt(kDefaultSearchItems);
  ExpectConsistent();

  // Insert at end.
  AddTestResultAtIndex(kDefaultSearchItems);
  ExpectConsistent();

  // Delete from start.
  DeleteResultAt(0);
  ExpectConsistent();
}

// Regression test for http://crbug.com/402859 to ensure ProgressBar is
// initialized properly in SearchResultListView::SetResult().
TEST_F(SearchResultListViewTest, ProgressBar) {
  SetUpSearchResults();

  GetResults()->GetItemAt(0)->SetIsInstalling(true);
  EXPECT_EQ(0.0f, GetProgressBarAt(0)->current_value());
  GetResults()->GetItemAt(0)->SetPercentDownloaded(10);

  DeleteResultAt(0);
  RunPendingMessages();
  EXPECT_EQ(0.0f, GetProgressBarAt(0)->current_value());
}

}  // namespace test
}  // namespace app_list
