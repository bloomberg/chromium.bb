// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kGroupSpacing = 20;

}  // namespace

SearchResultPageView::SearchResultPageView(AppListMainView* app_list_main_view,
                                           AppListViewDelegate* view_delegate)
    : results_view_(
          new SearchResultListView(app_list_main_view, view_delegate)),
      tiles_view_(new SearchResultTileItemListView()) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kExperimentalWindowPadding, 0,
                                        kGroupSpacing));

  // The view containing the search results.
  AddChildView(results_view_);

  // The view containing the start page tiles.
  AddChildView(tiles_view_);

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
