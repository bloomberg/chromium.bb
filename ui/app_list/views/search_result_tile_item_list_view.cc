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

void SearchResultTileItemListView::OnContainerSelected(bool from_bottom) {
  if (num_results() == 0)
    return;

  // TODO(calamity): come in from the back when tab navigating backwards.
  SetSelectedIndex(0);
}

int SearchResultTileItemListView::Update() {
  std::vector<SearchResult*> display_results =
      AppListModel::FilterSearchResultsByDisplayType(
          results(), SearchResult::DISPLAY_TILE, kNumSearchResultTiles);
  for (size_t i = 0; i < kNumSearchResultTiles; ++i) {
    SearchResult* item =
        i < display_results.size() ? display_results[i] : nullptr;
    tile_views_[i]->SetSearchResult(item);
  }
  return display_results.size();
}

void SearchResultTileItemListView::UpdateSelectedIndex(int old_selected,
                                                       int new_selected) {
  if (old_selected >= 0) {
    tile_views_[old_selected]->set_background(nullptr);
    tile_views_[old_selected]->SchedulePaint();
  }

  if (new_selected >= 0) {
    tile_views_[new_selected]->set_background(
        views::Background::CreateSolidBackground(kSelectedColor));
    tile_views_[new_selected]->SchedulePaint();
  }
}

bool SearchResultTileItemListView::OnKeyPressed(const ui::KeyEvent& event) {
  if (selected_index() >= 0 && child_at(selected_index())->OnKeyPressed(event))
    return true;

  int selection_index = -1;
  switch (event.key_code()) {
    case ui::VKEY_TAB:
      if (event.IsShiftDown())
        selection_index = selected_index() - 1;
      else
        selection_index = selected_index() + 1;
      break;
    default:
      break;
  }
  if (IsValidSelectionIndex(selection_index)) {
    SetSelectedIndex(selection_index);
    return true;
  }

  return false;
}

}  // namespace app_list
