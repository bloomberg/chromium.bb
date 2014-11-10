// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_view.h"

#include "ui/app_list/search_result.h"

namespace app_list {

SearchResultTileItemView::SearchResultTileItemView() : item_(NULL) {
  // When |item_| is null, the tile is invisible. Calling SetSearchResult with a
  // non-null item makes the tile visible.
  SetVisible(false);
}

SearchResultTileItemView::~SearchResultTileItemView() {
  if (item_)
    item_->RemoveObserver(this);
}

void SearchResultTileItemView::SetSearchResult(SearchResult* item) {
  SetVisible(item != NULL);

  SearchResult* old_item = item_;
  if (old_item)
    old_item->RemoveObserver(this);

  item_ = item;

  if (!item)
    return;

  item_->AddObserver(this);

  SetTitle(item_->title());

  // Only refresh the icon if it's different from the old one. This prevents
  // flickering.
  if (old_item == NULL ||
      !item->icon().BackedBySameObjectAs(old_item->icon())) {
    OnIconChanged();
  }
}

void SearchResultTileItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  item_->Open(event.flags());
}

void SearchResultTileItemView::OnIconChanged() {
  SetIcon(item_->icon());
}

void SearchResultTileItemView::OnResultDestroying() {
  if (item_)
    item_->RemoveObserver(this);
  item_ = NULL;
}

}  // namespace app_list
