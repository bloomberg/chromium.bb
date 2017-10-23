// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuRunner;
class Label;
}  // namespace views

namespace app_list {

class AppListViewDelegate;
class SearchResult;
class SearchResultContainerView;
class PaginationModel;

// A TileItemView that displays a search result.
class APP_LIST_EXPORT SearchResultTileItemView
    : public TileItemView,
      public views::ContextMenuController,
      public SearchResultObserver {
 public:
  SearchResultTileItemView(SearchResultContainerView* result_container,
                           AppListViewDelegate* view_delegate,
                           PaginationModel* pagination_model,
                           bool is_suggested_app,
                           bool is_fullscreen_app_list_enabled,
                           bool is_play_store_search_enabled);
  ~SearchResultTileItemView() override;

  SearchResult* result() { return item_; }
  void SetSearchResult(SearchResult* item);

  // Overridden from TileItemView:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnFocus() override;

  // Overridden from SearchResultObserver:
  void OnIconChanged() override;
  void OnBadgeIconChanged() override;
  void OnRatingChanged() override;
  void OnFormattedPriceChanged() override;
  void OnResultDestroying() override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

 private:
  // Shows rating in proper format if |rating| is not negative. Otherwise, hides
  // the rating label.
  void SetRating(float rating);

  // Shows price if |price| is not empty. Otherwise, hides the price label.
  void SetPrice(const base::string16& price);

  // Records an app being launched.
  void LogAppLaunch() const;

  // Overridden from views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // Whether the tile item view is a suggested app, used in StartPageView.
  const bool is_suggested_app_;

  SearchResultContainerView* result_container_;  // Parent view

  // Owned by the model provided by the AppListViewDelegate.
  SearchResult* item_ = nullptr;

  views::Label* rating_ = nullptr;           // Owned by views hierarchy.
  views::Label* price_ = nullptr;            // Owned by views hierarchy.
  views::ImageView* rating_star_ = nullptr;  // Owned by views hierarchy.

  AppListViewDelegate* view_delegate_;

  PaginationModel* const pagination_model_;  // Owned by AppsGridView.

  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  const bool is_fullscreen_app_list_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
