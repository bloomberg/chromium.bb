// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_page_view.h"

#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

const int kGroupSpacing = 20;

// Tile container constants.
const size_t kNumSearchResultTiles = 5;
const int kTileSpacing = 10;

}  // namespace

SearchResultPageView::SearchResultPageView(AppListMainView* app_list_main_view,
                                           AppListViewDelegate* view_delegate)
    : results_view_(
          new SearchResultListView(app_list_main_view, view_delegate)),
      tiles_container_(new views::View) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, kGroupSpacing));

  // The view containing the search results.
  AddChildView(results_view_);

  // The view containing the start page tiles.
  InitTilesContainer();
  AddChildView(tiles_container_);

  AppListModel::SearchResults* model = view_delegate->GetModel()->results();
  SetResults(model);
  results_view_->SetResults(model);
}

SearchResultPageView::~SearchResultPageView() {
}

void SearchResultPageView::InitTilesContainer() {
  views::BoxLayout* tiles_layout_manager =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, kTileSpacing);
  tiles_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  tiles_container_->SetLayoutManager(tiles_layout_manager);

  for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
    SearchResultTileItemView* tile_item = new SearchResultTileItemView();
    tiles_container_->AddChildView(tile_item);
    tile_views_.push_back(tile_item);
  }
}

bool SearchResultPageView::OnKeyPressed(const ui::KeyEvent& event) {
  return results_view_->OnKeyPressed(event);
}

void SearchResultPageView::Update() {
  results_view_->SetSelectedIndex(0);

  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_TILE, kNumSearchResultTiles);
  for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
    SearchResult* item =
        i < display_results.size() ? display_results[i] : nullptr;
    tile_views_[i]->SetSearchResult(item);
  }
  tiles_container_->Layout();
  Layout();
}

}  // namespace app_list
