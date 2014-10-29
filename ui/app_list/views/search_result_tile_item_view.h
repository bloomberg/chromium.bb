// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_

#include "ui/app_list/search_result_observer.h"
#include "ui/app_list/views/tile_item_view.h"

namespace app_list {

class SearchResult;

// A TileItemView that displays a search result.
class APP_LIST_EXPORT SearchResultTileItemView : public TileItemView,
                                                 public SearchResultObserver {
 public:
  SearchResultTileItemView();
  ~SearchResultTileItemView() override;

  void SetSearchResult(SearchResult* item);

  // Overridden from TileItemView:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from SearchResultObserver:
  void OnIconChanged() override;
  void OnResultDestroying() override;

 private:
  // Owned by the model provided by the AppListViewDelegate.
  SearchResult* item_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
