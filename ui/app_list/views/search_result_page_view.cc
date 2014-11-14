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
                                           AppListViewDelegate* view_delegate)
    : results_view_(
          new SearchResultListView(app_list_main_view, view_delegate)),
      tiles_view_(new SearchResultTileItemListView()) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kExperimentalWindowPadding, kTopPadding,
                                        kGroupSpacing));

  // The view containing the search results.
  AddChildView(new SearchCardView(results_view_));

  // The view containing the start page tiles.
  AddChildView(new SearchCardView(tiles_view_));

  AppListModel::SearchResults* model = view_delegate->GetModel()->results();
  results_view_->SetResults(model);
  tiles_view_->SetResults(model);
}

SearchResultPageView::~SearchResultPageView() {
}

bool SearchResultPageView::OnKeyPressed(const ui::KeyEvent& event) {
  return results_view_->OnKeyPressed(event);
}

void SearchResultPageView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

}  // namespace app_list
