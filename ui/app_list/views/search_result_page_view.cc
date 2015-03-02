// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include <algorithm>

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/shadow_border.h"

namespace app_list {

namespace {

const int kGroupSpacing = 6;
const int kTopPadding = 8;

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
 public:
  explicit SearchCardView(views::View* content_view) {
    SetBorder(make_scoped_ptr(new views::ShadowBorder(GetShadowForZHeight(1))));
    SetLayoutManager(new views::FillLayout());
    content_view->set_background(
        views::Background::CreateSolidBackground(kCardBackgroundColor));
    AddChildView(content_view);
  }

  ~SearchCardView() override {}

  void ChildPreferredSizeChanged(views::View* child) override {
    Layout();
    PreferredSizeChanged();
  }
};

}  // namespace

SearchResultPageView::SearchResultPageView() : selected_index_(0) {
  if (switches::IsExperimentalAppListEnabled()) {
    gfx::ShadowValue shadow = GetShadowForZHeight(1);
    scoped_ptr<views::Border> border(new views::ShadowBorder(shadow));

    gfx::Insets insets = gfx::Insets(kTopPadding, kExperimentalSearchBoxPadding,
                                     0, kExperimentalSearchBoxPadding);
    insets += -border->GetInsets();

    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, kGroupSpacing);
    layout->set_inside_border_insets(insets);

    SetLayoutManager(layout);
  } else {
    SetLayoutManager(new views::FillLayout);
  }
}

SearchResultPageView::~SearchResultPageView() {
}

void SearchResultPageView::SetSelection(bool select) {
  if (select)
    SetSelectedIndex(0, false);
  else
    result_container_views_[selected_index_]->ClearSelectedIndex();
}

void SearchResultPageView::AddSearchResultContainerView(
    AppListModel::SearchResults* results_model,
    SearchResultContainerView* result_container) {
  views::View* view_to_add = result_container;
  if (switches::IsExperimentalAppListEnabled())
    view_to_add = new SearchCardView(result_container);

  AddChildView(view_to_add);
  result_container_views_.push_back(result_container);
  result_container->SetResults(results_model);
}

bool SearchResultPageView::OnKeyPressed(const ui::KeyEvent& event) {
  if (result_container_views_.at(selected_index_)->OnKeyPressed(event))
    return true;

  int dir = 0;
  bool directional_movement = false;
  switch (event.key_code()) {
    case ui::VKEY_TAB:
      dir = event.IsShiftDown() ? -1 : 1;
      break;
    case ui::VKEY_UP:
      dir = -1;
      directional_movement = true;
      break;
    case ui::VKEY_DOWN:
      dir = 1;
      directional_movement = true;
      break;
    default:
      return false;
  }

  // Find the next result container with results.
  int new_selected = selected_index_;
  do {
    new_selected += dir;
  } while (IsValidSelectionIndex(new_selected) &&
           result_container_views_[new_selected]->num_results() == 0);

  if (IsValidSelectionIndex(new_selected)) {
    SetSelectedIndex(new_selected, directional_movement);
    return true;
  }

  return false;
}

void SearchResultPageView::SetSelectedIndex(int index,
                                            bool directional_movement) {
  bool from_bottom = index < selected_index_;

  // Reset the old selected view's selection.
  result_container_views_[selected_index_]->ClearSelectedIndex();
  selected_index_ = index;
  // Set the new selected view's selection to its first result.
  result_container_views_[selected_index_]->OnContainerSelected(
      from_bottom, directional_movement);
}

bool SearchResultPageView::IsValidSelectionIndex(int index) {
  return index >= 0 && index < static_cast<int>(result_container_views_.size());
}

void SearchResultPageView::ChildPreferredSizeChanged(views::View* child) {
  DCHECK(!result_container_views_.empty());

  if (switches::IsExperimentalAppListEnabled()) {
    // Sort the result container views by their score.
    std::sort(result_container_views_.begin(), result_container_views_.end(),
              [](const SearchResultContainerView* a,
                 const SearchResultContainerView* b) -> bool {
                return a->container_score() > b->container_score();
              });

    for (size_t i = 0; i < result_container_views_.size(); ++i) {
      result_container_views_[i]->ClearSelectedIndex();
      ReorderChildView(result_container_views_[i]->parent(), i);
    }
  }

  Layout();
  SetSelectedIndex(0, false);
}

}  // namespace app_list
