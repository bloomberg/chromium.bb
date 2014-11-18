// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_

#include "base/basictypes.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/app_list/views/search_result_container_view.h"

namespace app_list {

class AllAppsTileItemView;
class AppListMainView;
class AppListViewDelegate;
class SearchResultTileItemView;
class TileItemView;

// The start page for the experimental app list.
class APP_LIST_EXPORT StartPageView : public SearchResultContainerView,
                                      public SearchBoxViewDelegate {
 public:
  StartPageView(AppListMainView* app_list_main_view,
                AppListViewDelegate* view_delegate);
  ~StartPageView() override;

  void Reset();

  void UpdateForTesting();

  const std::vector<SearchResultTileItemView*>& tile_views() const {
    return search_result_tile_views_;
  }
  TileItemView* all_apps_button() const;
  SearchBoxView* dummy_search_box_view() { return search_box_view_; }

  // Overridden from views::View:
  void Layout() override;

  // Overridden from SearchResultContainerView:
  void OnContainerSelected(bool from_bottom) override;

 private:
  // Overridden from SearchResultContainerView:
  int Update() override;
  void UpdateSelectedIndex(int old_selected, int new_selected) override;

  void InitInstantContainer();
  void InitTilesContainer();

  // Overridden from SearchBoxViewDelegate:
  void QueryChanged(SearchBoxView* sender) override;

  // The parent view of ContentsView which is the parent of this view.
  AppListMainView* app_list_main_view_;

  AppListViewDelegate* view_delegate_;  // Owned by AppListView.

  SearchBoxView* search_box_view_;      // Owned by views hierarchy.
  views::View* instant_container_;  // Owned by views hierarchy.
  views::View* tiles_container_;    // Owned by views hierarchy.

  std::vector<SearchResultTileItemView*> search_result_tile_views_;
  AllAppsTileItemView* all_apps_button_;

  DISALLOW_COPY_AND_ASSIGN(StartPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
