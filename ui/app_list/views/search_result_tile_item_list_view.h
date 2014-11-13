// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "ui/app_list/views/search_result_container_view.h"

namespace app_list {

class SearchResultTileItemView;

// Displays a list of SearchResultTileItemView.
class APP_LIST_EXPORT SearchResultTileItemListView
    : public SearchResultContainerView {
 public:
  SearchResultTileItemListView();
  ~SearchResultTileItemListView() override;

  // Overridden from SearchResultContainerView:
  void Update() override;

 private:
  std::vector<SearchResultTileItemView*> tile_views_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_
