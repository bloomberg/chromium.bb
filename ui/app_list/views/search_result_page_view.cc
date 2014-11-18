// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/shadow_border.h"

namespace app_list {

namespace {

const int kGroupSpacing = 20;
const int kTopPadding = 5;

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
 public:
  SearchCardView(views::View* content_view) {
    SetBorder(make_scoped_ptr(new views::ShadowBorder(
        kCardShadowBlur, kCardShadowColor, kCardShadowYOffset, 0)));
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

SearchResultPageView::SearchResultPageView(AppListMainView* app_list_main_view,
                                           AppListViewDelegate* view_delegate) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kExperimentalWindowPadding, kTopPadding,
                                        kGroupSpacing));

  AppListModel::SearchResults* results = view_delegate->GetModel()->results();
  AddSearchResultContainerView(
      results, new SearchResultListView(app_list_main_view, view_delegate));
  AddSearchResultContainerView(results, new SearchResultTileItemListView());
}

SearchResultPageView::~SearchResultPageView() {
}

bool SearchResultPageView::OnKeyPressed(const ui::KeyEvent& event) {
  DCHECK(!result_container_views_.empty());
  // Capture the Tab key to prevent defocusing of the search box.
  return result_container_views_[0]->OnKeyPressed(event) ||
         event.key_code() == ui::VKEY_TAB;
}

void SearchResultPageView::ChildPreferredSizeChanged(views::View* child) {
  DCHECK(!result_container_views_.empty());
  Layout();
  result_container_views_[0]->OnContainerSelected(false);
}

void SearchResultPageView::AddSearchResultContainerView(
    AppListModel::SearchResults* results_model,
    SearchResultContainerView* result_container) {
  AddChildView(new SearchCardView(result_container));
  result_container_views_.push_back(result_container);
  result_container->SetResults(results_model);
}

}  // namespace app_list
