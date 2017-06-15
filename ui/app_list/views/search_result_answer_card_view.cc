// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_answer_card_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {
constexpr int kVerticalPadding = 11;
constexpr int kHorizontalPadding = 16;
}

// Container of the search answer view.
class SearchResultAnswerCardView::SearchAnswerContainerView
    : public views::CustomButton,
      public views::ButtonListener,
      public SearchResultObserver {
 public:
  explicit SearchAnswerContainerView(AppListViewDelegate* view_delegate)
      : CustomButton(this), view_delegate_(view_delegate) {
    // Center the card horizontally in the container.
    views::BoxLayout* answer_container_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             gfx::Insets(kVerticalPadding, kHorizontalPadding));
    answer_container_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    SetLayoutManager(answer_container_layout);
  }

  ~SearchAnswerContainerView() override {
    if (search_result_)
      search_result_->RemoveObserver(this);
  }

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;
    selected_ = selected;
    UpdateBackgroundColor();
    if (selected)
      ScrollRectToVisible(GetLocalBounds());
  }

  void SetSearchResult(SearchResult* search_result) {
    views::View* const old_result_view = child_count() ? child_at(0) : nullptr;
    views::View* const new_result_view =
        search_result ? search_result->view() : nullptr;

    if (old_result_view != new_result_view) {
      if (old_result_view != nullptr)
        RemoveChildView(old_result_view);
      if (new_result_view != nullptr)
        AddChildView(new_result_view);
    }

    if (search_result_)
      search_result_->RemoveObserver(this);
    search_result_ = search_result;
    if (search_result_) {
      search_result_->AddObserver(this);
      SetAccessibleName(search_result_->title());
    }
  }

  // views::CustomButton overrides:
  const char* GetClassName() const override {
    return "SearchAnswerContainerView";
  }

  void StateChanged(ButtonState old_state) override { UpdateBackgroundColor(); }

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(sender == this);
    if (search_result_)
      view_delegate_->OpenSearchResult(search_result_, false, event.flags());
  }

  // SearchResultObserver overrides:
  void OnViewHoverStateChanged() override { UpdateBackgroundColor(); }

  void OnResultDestroying() override { search_result_ = nullptr; }

 private:
  void UpdateBackgroundColor() {
    if (selected_) {
      SetBackground(views::CreateSolidBackground(kSelectedColor));
    } else if (state() == STATE_HOVERED || state() == STATE_PRESSED ||
               (search_result_ && search_result_->is_mouse_in_view())) {
      SetBackground(views::CreateSolidBackground(kHighlightedColor));
    } else {
      SetBackground(nullptr);
    }

    SchedulePaint();
  }

  AppListViewDelegate* const view_delegate_;  // Not owned.
  bool selected_ = false;
  SearchResult* search_result_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(SearchAnswerContainerView);
};

SearchResultAnswerCardView::SearchResultAnswerCardView(
    AppListViewDelegate* view_delegate)
    : search_answer_container_view_(
          new SearchAnswerContainerView(view_delegate)) {
  AddChildView(search_answer_container_view_);
  SetLayoutManager(new views::FillLayout);
}

SearchResultAnswerCardView::~SearchResultAnswerCardView() {}

const char* SearchResultAnswerCardView::GetClassName() const {
  return "SearchResultAnswerCardView";
}

void SearchResultAnswerCardView::OnContainerSelected(
    bool from_bottom,
    bool directional_movement) {
  if (num_results() == 0)
    return;

  SetSelectedIndex(0);
}

int SearchResultAnswerCardView::GetYSize() {
  return num_results();
}

int SearchResultAnswerCardView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_CARD, 1);

  const bool have_result =
      !display_results.empty() && !features::IsAnswerCardDarkRunEnabled();

  search_answer_container_view_->SetSearchResult(
      have_result ? display_results[0] : nullptr);
  parent()->SetVisible(have_result);

  set_container_score(have_result ? display_results.front()->relevance() : 0);
  return have_result ? 1 : 0;
}

void SearchResultAnswerCardView::UpdateSelectedIndex(int old_selected,
                                                     int new_selected) {
  if (new_selected == old_selected)
    return;

  const bool is_selected = new_selected == 0;
  search_answer_container_view_->SetSelected(is_selected);
  if (is_selected)
    NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
}

bool SearchResultAnswerCardView::OnKeyPressed(const ui::KeyEvent& event) {
  if (selected_index() == 0 &&
      search_answer_container_view_->OnKeyPressed(event)) {
    return true;
  }

  return SearchResultContainerView::OnKeyPressed(event);
}

void SearchResultAnswerCardView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  search_answer_container_view_->GetAccessibleNodeData(node_data);
}

}  // namespace app_list
