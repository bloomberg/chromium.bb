// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/views/apps_grid_view_delegate.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/app_list/views/search_result_list_view_delegate.h"
#include "ui/views/view.h"

namespace views {
class Widget;
}

namespace app_list {

class AppListItem;
class AppListModel;
class AppListViewDelegate;
class ApplicationDragAndDropHost;
class ContentsSwitcherView;
class ContentsView;
class PaginationModel;
class SearchBoxView;

// AppListMainView contains the normal view of the app list, which is shown
// when the user is signed in.
class APP_LIST_EXPORT AppListMainView : public views::View,
                                        public AppsGridViewDelegate,
                                        public SearchBoxViewDelegate,
                                        public SearchResultListViewDelegate {
 public:
  // Takes ownership of |delegate|.
  explicit AppListMainView(AppListViewDelegate* delegate,
                           int initial_apps_page,
                           gfx::NativeView parent);
  virtual ~AppListMainView();

  void ShowAppListWhenReady();

  void ResetForShow();

  void Close();

  void Prerender();

  void ModelChanged();

  void UpdateSearchBoxVisibility();

  void OnStartPageSearchTextfieldChanged(const base::string16& new_contents);

  SearchBoxView* search_box_view() const { return search_box_view_; }

  // If |drag_and_drop_host| is not NULL it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  ContentsView* contents_view() const { return contents_view_; }
  ContentsSwitcherView* contents_switcher_view() const {
    return contents_switcher_view_;
  }
  AppListModel* model() { return model_; }

  // Returns true if the app list should be centered and in landscape mode.
  bool ShouldCenterWindow() const;

  // Called when the search box's visibility is changed.
  void NotifySearchBoxVisibilityChanged();

 private:
  class IconLoader;

  // Adds the ContentsView and the ContentsSwitcherView.
  void AddContentsViews();

  // Gets the PaginationModel owned by the AppsGridView.
  PaginationModel* GetAppsPaginationModel();

  // Loads icon image for the apps in the selected page of |pagination_model_|.
  // |parent| is used to determine the image scale factor to use.
  void PreloadIcons(gfx::NativeView parent);

  // Invoked when |icon_loading_wait_timer_| fires.
  void OnIconLoadingWaitTimer();

  // Invoked from an IconLoader when icon loading is finished.
  void OnItemIconLoaded(IconLoader* loader);

  // Overridden from AppsGridViewDelegate:
  virtual void ActivateApp(AppListItem* item, int event_flags) OVERRIDE;
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) OVERRIDE;

  // Overridden from SearchBoxViewDelegate:
  virtual void QueryChanged(SearchBoxView* sender) OVERRIDE;

  // Overridden from SearchResultListViewDelegate:
  virtual void OnResultInstalled(SearchResult* result) OVERRIDE;
  virtual void OnResultUninstalled(SearchResult* result) OVERRIDE;

  AppListViewDelegate* delegate_;  // Owned by parent view (AppListView).
  AppListModel* model_;  // Unowned; ownership is handled by |delegate_|.

  SearchBoxView* search_box_view_;  // Owned by views hierarchy.
  ContentsView* contents_view_;  // Owned by views hierarchy.

  // Owned by views hierarchy. NULL in the non-experimental app list.
  ContentsSwitcherView* contents_switcher_view_;

  // A timer that fires when maximum allowed time to wait for icon loading has
  // passed.
  base::OneShotTimer<AppListMainView> icon_loading_wait_timer_;

  ScopedVector<IconLoader> pending_icon_loaders_;

  base::WeakPtrFactory<AppListMainView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListMainView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_MAIN_VIEW_H_
