// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_

#include "base/basictypes.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace app_list {

class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class TileItemView;

// The start page for the experimental app list.
class StartPageView : public views::View,
                      public views::ButtonListener,
                      public AppListViewDelegateObserver,
                      public AppListModelObserver {
 public:
  StartPageView(AppListMainView* app_list_main_view,
                AppListViewDelegate* view_delegate);
  virtual ~StartPageView();

  void Reset();

  const std::vector<TileItemView*>& tile_views() const { return tile_views_; }

 private:
  void SetModel(AppListModel* model);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

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

  views::View* instant_container_;  // Owned by views hierarchy.
  views::View* tiles_container_;    // Owned by views hierarchy.

  std::vector<TileItemView*> tile_views_;

  DISALLOW_COPY_AND_ASSIGN(StartPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
