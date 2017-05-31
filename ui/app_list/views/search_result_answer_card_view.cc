// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_answer_card_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/views/search_result_page_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

// Answer card relevance is high to always have it first.
constexpr double kSearchAnswerCardRelevance = 100;

}  // namespace

// Container of the search answer view.
class SearchResultAnswerCardView::SearchAnswerContainerView
    : public views::CustomButton {
 public:
  explicit SearchAnswerContainerView(views::View* search_results_page_view)
      : CustomButton(nullptr),
        search_results_page_view_(search_results_page_view) {
    // Center the card horizontally in the container.
    views::BoxLayout* answer_container_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    answer_container_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(answer_container_layout);
  }

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;
    selected_ = selected;
    UpdateBackgroundColor();
  }

  // views::CustomButton overrides:
  void ChildPreferredSizeChanged(View* child) override {
    // Card size changed.
    if (visible())
      search_results_page_view_->Layout();
  }

  int GetHeightForWidth(int w) const override {
    return visible() ? CustomButton::GetHeightForWidth(w) : 0;
  }

  const char* GetClassName() const override {
    return "SearchAnswerContainerView";
  }

  void StateChanged(ButtonState old_state) override { UpdateBackgroundColor(); }

 private:
  void UpdateBackgroundColor() {
    views::Background* background = nullptr;

    if (selected_) {
      background = views::Background::CreateSolidBackground(kSelectedColor);
    } else if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
      background = views::Background::CreateSolidBackground(kHighlightedColor);
    }

    set_background(background);
    SchedulePaint();
  }

  views::View* const search_results_page_view_;
  bool selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchAnswerContainerView);
};

SearchResultAnswerCardView::SearchResultAnswerCardView(
    AppListModel* model,
    SearchResultPageView* search_results_page_view,
    views::View* search_answer_view)
    : model_(model),
      search_answer_container_view_(
          new SearchAnswerContainerView(search_results_page_view)) {
  search_answer_container_view_->SetVisible(false);
  search_answer_container_view_->AddChildView(search_answer_view);
  AddChildView(search_answer_container_view_);
  model->AddObserver(this);
  SetLayoutManager(new views::FillLayout);
}

SearchResultAnswerCardView::~SearchResultAnswerCardView() {
  model_->RemoveObserver(this);
}

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
  const bool have_result = search_answer_container_view_->visible();
  set_container_score(have_result ? kSearchAnswerCardRelevance : 0);
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

void SearchResultAnswerCardView::OnSearchAnswerAvailableChanged(
    bool has_answer) {
  const bool visible = has_answer && !features::IsAnswerCardDarkRunEnabled();
  if (visible == search_answer_container_view_->visible())
    return;

  search_answer_container_view_->SetVisible(visible);
  ScheduleUpdate();
}

}  // namespace app_list
