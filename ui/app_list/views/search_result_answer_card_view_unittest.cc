// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_answer_card_view.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/search_result_list_view_delegate.h"
#include "ui/app_list/views/search_result_view.h"
#include "ui/views/background.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

namespace {
constexpr char kResultTitle[] = "The weather is fine";
constexpr double kRelevance = 13.0;
}  // namespace

class SearchResultAnswerCardViewTest : public views::ViewsTestBase {
 public:
  SearchResultAnswerCardViewTest() {}

  // Overridden from testing::Test:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    search_card_view_ = base::MakeUnique<views::View>();

    result_container_view_ = new SearchResultAnswerCardView(&view_delegate_);
    search_card_view_->AddChildView(result_container_view_);
    result_container_view_->SetResults(view_delegate_.GetModel()->results());

    result_view_ = base::MakeUnique<views::View>();
    result_view_->set_owned_by_client();

    SetUpSearchResult();
  }

 protected:
  void SetUpSearchResult() {
    AppListModel::SearchResults* results = GetResults();
    std::unique_ptr<TestSearchResult> result =
        base::MakeUnique<TestSearchResult>();
    result->set_display_type(SearchResult::DISPLAY_CARD);
    result->set_title(base::UTF8ToUTF16(kResultTitle));
    result->set_view(result_view_.get());
    result->set_relevance(kRelevance);
    results->Add(std::move(result));

    // Adding results will schedule Update().
    RunPendingMessages();
    result_container_view_->OnContainerSelected(false, false);
  }

  int GetOpenResultCountAndReset(int ranking) {
    int result = view_delegate_.open_search_result_counts()[ranking];
    view_delegate_.open_search_result_counts().clear();
    return result;
  }

  void ClearSelectedIndex() { result_container_view_->ClearSelectedIndex(); }

  void DeleteResult() {
    GetResults()->DeleteAt(0);
    RunPendingMessages();
  }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return result_container_view_->OnKeyPressed(event);
  }

  AppListModel::SearchResults* GetResults() {
    return view_delegate_.GetModel()->results();
  }

  views::View* search_card_view() const { return search_card_view_.get(); }

  int GetYSize() const { return result_container_view_->GetYSize(); }

  int GetResultCountFromView() { return result_container_view_->num_results(); }

  int GetSelectedIndex() { return result_container_view_->selected_index(); }

  double GetContainerScore() const {
    return result_container_view_->container_score();
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) {
    result_container_view_->GetAccessibleNodeData(node_data);
  }

  views::View* result_view() const { return result_view_.get(); }

 private:
  AppListTestViewDelegate view_delegate_;

  // The root of the test's view hierarchy. In the real view hierarchy it's
  // SearchCardView.
  std::unique_ptr<views::View> search_card_view_;
  // Result container that we are testing. It's a child of search_card_view_.
  // Owned by the view hierarchy.
  SearchResultAnswerCardView* result_container_view_;
  // View sent within the search result. May be shown within
  // result_container_view_. Has set_owned_by_client() called.
  std::unique_ptr<views::View> result_view_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultAnswerCardViewTest);
};

TEST_F(SearchResultAnswerCardViewTest, Basic) {
  EXPECT_EQ(kRelevance, GetContainerScore());

  EXPECT_EQ(1, GetResultCountFromView());

  // Result view should be added to the hierarchy.
  EXPECT_EQ(search_card_view(), result_view()->parent()->parent()->parent());
  ASSERT_TRUE(search_card_view()->visible());

  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_EQ(1, GetYSize());
}

TEST_F(SearchResultAnswerCardViewTest, ButtonBackground) {
  views::View* button = result_view()->parent();
  EXPECT_EQ(kSelectedColor, button->background()->get_color());

  ClearSelectedIndex();
  EXPECT_EQ(nullptr, button->background());

  GetResults()->GetItemAt(0)->SetIsMouseInView(true);
  EXPECT_EQ(kHighlightedColor, button->background()->get_color());

  GetResults()->GetItemAt(0)->SetIsMouseInView(false);
  EXPECT_EQ(nullptr, button->background());
}

TEST_F(SearchResultAnswerCardViewTest, KeyboardEvents) {
  EXPECT_TRUE(KeyPress(ui::VKEY_RETURN));
  EXPECT_EQ(1, GetOpenResultCountAndReset(0));

  // When navigating up/down/next off the the result, pass the event to the
  // parent to handle.
  EXPECT_FALSE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(0, GetSelectedIndex());

  EXPECT_FALSE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());

  EXPECT_FALSE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(0, GetSelectedIndex());
}

TEST_F(SearchResultAnswerCardViewTest, SpokenFeedback) {
  ui::AXNodeData node_data;
  GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ui::AX_ROLE_BUTTON, node_data.role);
  EXPECT_EQ(kResultTitle, node_data.GetStringAttribute(ui::AX_ATTR_NAME));
}

TEST_F(SearchResultAnswerCardViewTest, DeleteResult) {
  DeleteResult();
  EXPECT_EQ(0UL, GetResults()->item_count());
  EXPECT_EQ(nullptr, result_view()->parent());
  EXPECT_EQ(0, GetYSize());
  ASSERT_FALSE(search_card_view()->visible());
  EXPECT_EQ(0, GetContainerScore());
}

}  // namespace test
}  // namespace app_list
