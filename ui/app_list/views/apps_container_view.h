// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
#define UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_

#include <vector>

#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/views/top_icon_animation_view.h"
#include "ui/views/view.h"

namespace app_list {

class AppsGridView;
class ApplicationDragAndDropHost;
class AppListFolderItem;
class AppListFolderView;
class AppListMainView;
class AppListModel;
class ContentsView;
class FolderBackgroundView;

// AppsContainerView contains a root level AppsGridView to render the root level
// app items, and a AppListFolderView to render the app items inside the
// active folder. Only one if them is visible to user at any time.
class AppsContainerView : public views::View,
                          public TopIconAnimationObserver {
 public:
  AppsContainerView(AppListMainView* app_list_main_view,
                    AppListModel* model);
  virtual ~AppsContainerView();

  // Shows the active folder content specified by |folder_item|.
  void ShowActiveFolder(AppListFolderItem* folder_item);

  // Shows the root level apps list. This is called when UI navigate back from
  // a folder view with |folder_item|. If |folder_item| is NULL skips animation.
  void ShowApps(AppListFolderItem* folder_item);

  // Resets the app list to a state where it shows the main grid view. This is
  // called when the user opens the launcher for the first time or when the user
  // hides and then shows it. This is necessary because we only hide and show
  // the launcher on Windows and Linux so we need to reset to a fresh state.
  void ResetForShowApps();

  // Sets |drag_and_drop_host_| for the current app list in both
  // app_list_folder_view_ and root level apps_grid_view_.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Transits the UI from folder view to root lelve apps grid view when
  // re-parenting a child item of |folder_item|.
  void ReparentFolderItemTransit(AppListFolderItem* folder_item);

  // Returns true if it is currently showing an active folder page.
  bool IsInFolderView() const;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // TopIconAnimationObserver overrides:
  virtual void OnTopIconAnimationsComplete() OVERRIDE;

  AppsGridView* apps_grid_view() { return apps_grid_view_; }
  FolderBackgroundView* folder_background_view() {
     return folder_background_view_;
  }
  AppListFolderView* app_list_folder_view() { return app_list_folder_view_; }

 private:
  enum ShowState {
    SHOW_NONE,  // initial state
    SHOW_APPS,
    SHOW_ACTIVE_FOLDER,
    SHOW_ITEM_REPARENT,
  };

  void SetShowState(ShowState show_state, bool show_apps_with_animation);

  // Calculates the top item icon bounds in the active folder icon. The bounds
  // is relative to AppsContainerView.
  // Returns the bounds of top items' icon in sequence of top left, top right,
  // bottom left, bottom right.
  Rects GetTopItemIconBoundsInActiveFolder();

  // Creates the transitional views for animating the top items in the folder
  // when opening or closing a folder.
  void CreateViewsForFolderTopItemsAnimation(
      AppListFolderItem* active_folder, bool open_folder);

  void PrepareToShowApps(AppListFolderItem* folder_item);

  AppListModel* model_;
  AppsGridView* apps_grid_view_;  // Owned by views hierarchy.
  AppListFolderView* app_list_folder_view_;  // Owned by views hierarchy.
  FolderBackgroundView* folder_background_view_;  // Owned by views hierarchy.
  ShowState show_state_;

  // The transitional views for animating the top items in folder
  // when opening or closing a folder.
  std::vector<views::View*> top_icon_views_;

  size_t top_icon_animation_pending_count_;

  DISALLOW_COPY_AND_ASSIGN(AppsContainerView);
};

}  // namespace app_list


#endif  // UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
