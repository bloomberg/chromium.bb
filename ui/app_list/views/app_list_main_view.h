// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/views/view.h"

namespace app_list {

class AppListItem;
class AppListModel;
class AppListView;
class AppListViewDelegate;
class ApplicationDragAndDropHost;
class ContentsView;
class PaginationModel;
class SearchBoxView;

// AppListMainView contains the normal view of the app list, which is shown
// when the user is signed in.
class APP_LIST_EXPORT AppListMainView : public views::View,
                                        public AppListModelObserver,
                                        public SearchBoxViewDelegate {
 public:
  AppListMainView(AppListViewDelegate* delegate, AppListView* app_list_view);
  ~AppListMainView() override;

  void Init(gfx::NativeView parent,
            int initial_apps_page,
            SearchBoxView* search_box_view);

  void ShowAppListWhenReady();

  void ResetForShow();

  void Close();

  void ModelChanged();

  SearchBoxView* search_box_view() const { return search_box_view_; }

  // If |drag_and_drop_host| is not NULL it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  ContentsView* contents_view() const { return contents_view_; }
  AppListModel* model() { return model_; }
  AppListViewDelegate* view_delegate() { return delegate_; }

  // Called when the search box's visibility is changed.
  void NotifySearchBoxVisibilityChanged();

  bool ShouldShowCustomLauncherPage() const;
  void UpdateCustomLauncherPageVisibility();

  // Overridden from views::View:
  const char* GetClassName() const override;

  // Overridden from AppListModelObserver:
  void OnCustomLauncherPageEnabledStateChanged(bool enabled) override;
  void OnSearchEngineIsGoogleChanged(bool is_google) override;

  // Invoked when an item is activated on the grid view. |event_flags| contains
  // the flags of the keyboard/mouse event that triggers the activation request.
  void ActivateApp(AppListItem* item, int event_flags);

  // Called by the root grid view to cancel a drag that started inside a folder.
  // This can occur when the root grid is visible for a reparent and its model
  // changes, necessitating a cancel of the drag operation.
  void CancelDragInActiveFolder();

  // Called when the app represented by |result| is installed.
  void OnResultInstalled(SearchResult* result);

 private:
  // Adds the ContentsView.
  void AddContentsViews();

  // Gets the PaginationModel owned by the AppsGridView.
  PaginationModel* GetAppsPaginationModel();

  // Overridden from SearchBoxViewDelegate:
  void QueryChanged(SearchBoxView* sender) override;
  void BackButtonPressed() override;
  void SetSearchResultSelection(bool select) override;

  AppListViewDelegate* delegate_;  // Owned by parent view (AppListView).
  AppListModel* model_;  // Unowned; ownership is handled by |delegate_|.

  // Created by AppListView. Owned by views hierarchy.
  SearchBoxView* search_box_view_;
  ContentsView* contents_view_;  // Owned by views hierarchy.
  AppListView* const app_list_view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AppListMainView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_
