// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_

#include "base/macros.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuRunner;
}

namespace app_list {

class AppListViewDelegate;
class SearchResult;
class SearchResultContainerView;

// A TileItemView that displays a search result.
class APP_LIST_EXPORT SearchResultTileItemView
    : public TileItemView,
      public views::ContextMenuController,
      public SearchResultObserver {
 public:
  explicit SearchResultTileItemView(SearchResultContainerView* result_container,
                                    AppListViewDelegate* view_delegate);
  ~SearchResultTileItemView() override;

  SearchResult* result() { return item_; }
  void SetSearchResult(SearchResult* item);

  // Overridden from TileItemView:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Overridden from SearchResultObserver:
  void OnIconChanged() override;
  void OnBadgeIconChanged() override;
  void OnResultDestroying() override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

 private:
  SearchResultContainerView* result_container_;  // Parent view

  // Owned by the model provided by the AppListViewDelegate.
  SearchResult* item_;

  AppListViewDelegate* view_delegate_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
