// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "ui/app_list/views/search_result_container_view.h"

namespace views {
class Textfield;
}

namespace app_list {

class AppListViewDelegate;
class SearchResultTileItemView;

// Displays a list of SearchResultTileItemView.
class APP_LIST_EXPORT SearchResultTileItemListView
    : public SearchResultContainerView {
 public:
  explicit SearchResultTileItemListView(views::Textfield* search_box,
                                        AppListViewDelegate* view_delegate);
  ~SearchResultTileItemListView() override;

  // Overridden from SearchResultContainerView:
  void OnContainerSelected(bool from_bottom,
                           bool directional_movement) override;
  void NotifyFirstResultYIndex(int y_index) override;
  int GetYSize() override;

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  // Overridden from SearchResultContainerView:
  int DoUpdate() override;
  void UpdateSelectedIndex(int old_selected, int new_selected) override;

  std::vector<SearchResultTileItemView*> tile_views_;

  views::Textfield* search_box_;  // Owned by the views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_
