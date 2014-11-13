// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_list_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/search_result_tile_item_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Layout constants.
const size_t kNumSearchResultTiles = 5;
const int kTileSpacing = 10;

}  // namespace

namespace app_list {

SearchResultTileItemListView::SearchResultTileItemListView() {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, kTileSpacing));
  for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
    SearchResultTileItemView* tile_item = new SearchResultTileItemView();
    tile_views_.push_back(tile_item);
    AddChildView(tile_item);
  }
}

SearchResultTileItemListView::~SearchResultTileItemListView() {
}

void SearchResultTileItemListView::Update() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_TILE, kNumSearchResultTiles);
  for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
    SearchResult* item =
        i < display_results.size() ? display_results[i] : nullptr;
    tile_views_[i]->SetSearchResult(item);
  }
}

}  // namespace app_list
