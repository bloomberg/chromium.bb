// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/views/view.h"

namespace app_list {

class AppListMainView;
class AppListViewDelegate;
class SearchResultListView;
class TileItemView;

// The start page for the experimental app list.
class APP_LIST_EXPORT StartPageView : public views::View,
                                      public ui::ListModelObserver,
                                      public SearchBoxViewDelegate {
 public:
  StartPageView(AppListMainView* app_list_main_view,
                AppListViewDelegate* view_delegate);
  virtual ~StartPageView();

  void Reset();
  void ShowSearchResults();

  bool IsShowingSearchResults() const;

  void UpdateForTesting();

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

  // Updates UI with model.
  void Update();

  // Schedules an Update() call using |update_factory_|. Does nothing if there
  // is a pending call.
  void ScheduleUpdate();

  // Overridden from SearchBoxViewDelegate:
  virtual void QueryChanged(SearchBoxView* sender) OVERRIDE;

  // Overridden from ui::ListModelObserver:
  virtual void ListItemsAdded(size_t start, size_t count) OVERRIDE;
  virtual void ListItemsRemoved(size_t start, size_t count) OVERRIDE;
  virtual void ListItemMoved(size_t index, size_t target_index) OVERRIDE;
  virtual void ListItemsChanged(size_t start, size_t count) OVERRIDE;

  // The parent view of ContentsView which is the parent of this view.
  AppListMainView* app_list_main_view_;

  AppListModel::SearchResults*
      search_results_model_;  // Owned by AppListSyncableService.

  AppListViewDelegate* view_delegate_;  // Owned by AppListView.

  SearchBoxView* search_box_view_;      // Owned by views hierarchy.
  SearchResultListView* results_view_;  // Owned by views hierarchy.
  views::View* instant_container_;  // Owned by views hierarchy.
  views::View* tiles_container_;    // Owned by views hierarchy.

  std::vector<TileItemView*> tile_views_;

  ShowState show_state_;

  // ScheduleUpdate() generates a single weak pointer; if one exists then an
  // update is pending. Further calls to ScheduleUpdate() will have no effect.
  // Once the Update() completes, the weak pointer is invalidated.
  base::WeakPtrFactory<StartPageView> update_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
