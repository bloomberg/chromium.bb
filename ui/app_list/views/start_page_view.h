// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_

#include "base/basictypes.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/views/view.h"

namespace app_list {

class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class SearchResultListView;
class TileItemView;

// The start page for the experimental app list.
class StartPageView : public views::View,
                      public SearchBoxViewDelegate,
                      public AppListViewDelegateObserver,
                      public AppListModelObserver {
 public:
  StartPageView(AppListMainView* app_list_main_view,
                AppListViewDelegate* view_delegate);
  virtual ~StartPageView();

  void Reset();
  void ShowSearchResults();

  bool IsShowingSearchResults() const;

  const std::vector<TileItemView*>& tile_views() const { return tile_views_; }
  SearchBoxView* dummy_search_box_view() { return search_box_view_; }

  // Overridden from views::View:
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  enum ShowState {
    SHOW_START_PAGE,
    SHOW_SEARCH_RESULTS,
  };

  void InitInstantContainer();
  void InitTilesContainer();

  void SetShowState(ShowState show_state);
  void SetModel(AppListModel* model);

  // Overridden from SearchBoxViewDelegate:
  virtual void QueryChanged(SearchBoxView* sender) OVERRIDE;

  // Overridden from AppListViewDelegateObserver:
  virtual void OnProfilesChanged() OVERRIDE;

  // Overridden from AppListModelObserver:
  virtual void OnAppListModelStatusChanged() OVERRIDE;
  virtual void OnAppListItemAdded(AppListItem* item) OVERRIDE;
  virtual void OnAppListItemDeleted() OVERRIDE;
  virtual void OnAppListItemUpdated(AppListItem* item) OVERRIDE;

  // The parent view of ContentsView which is the parent of this view.
  AppListMainView* app_list_main_view_;

  AppListModel* model_;  // Owned by AppListSyncableService.

  AppListViewDelegate* view_delegate_;  // Owned by AppListView.

  SearchBoxView* search_box_view_;      // Owned by views hierarchy.
  SearchResultListView* results_view_;  // Owned by views hierarchy.
  views::View* instant_container_;  // Owned by views hierarchy.
  views::View* tiles_container_;    // Owned by views hierarchy.

  std::vector<TileItemView*> tile_views_;

  ShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(StartPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
