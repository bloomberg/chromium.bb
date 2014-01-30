// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
#define UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_

#include "ui/app_list/app_list_folder_item.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace app_list {

class AppsGridView;
class AppListFolderItem;
class AppListFolderView;
class AppListMainView;
class AppListModel;
class ContentsView;
class PaginationModel;

class TopIconAnimationObserver {
 public:
  // Called when top icon animation completes.
  virtual void OnTopIconAnimationsComplete(views::View* icon_view) {}

 protected:
  TopIconAnimationObserver() {}
  virtual ~TopIconAnimationObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TopIconAnimationObserver);
};

// AppsContainerView contains a root level AppsGridView to render the root level
// app items, and a AppListFolderView to render the app items inside the
// active folder. Only one if them is visible to user at any time.
class AppsContainerView : public views::View,
                          public TopIconAnimationObserver {
 public:
  AppsContainerView(AppListMainView* app_list_main_view,
                    PaginationModel* pagination_model,
                    AppListModel* model,
                    content::WebContents* start_page_contents);
  virtual ~AppsContainerView();

  // Shows the active folder content specified by |folder_item|.
  void ShowActiveFolder(AppListFolderItem* folder_item);

  // Shows the root level apps list. This is called when UI navigate back from
  // a folder view with |folder_item|. If |folder_item| is NULL skips animation.
  void ShowApps(AppListFolderItem* folder_item);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from TopIconAnimationObserver.
  virtual void OnTopIconAnimationsComplete(views::View* icon_view) OVERRIDE;

  AppsGridView* apps_grid_view() { return apps_grid_view_; }

 private:
  enum ShowState {
    SHOW_APPS,
    SHOW_ACTIVE_FOLDER,
  };

  void SetShowState(ShowState show_state);

  // Calculates the top item icon bounds in the active folder icon. The bounds
  // is relative to AppsContainerView.
  // Returns the bounds of top items' icon in sequence of top left, top right,
  // bottom left, bottom right.
  Rects GetTopItemIconBoundsInActiveFolder();

  // Creates the transitional views for animating the top items in the folder
  // when opening or closing a folder.
  void CreateViewsForFolderTopItemsAnimation(
      AppListFolderItem* active_folder, bool open_folder);

  AppListModel* model_;
  AppsGridView* apps_grid_view_;  // Owned by views hierarchy.
  AppListFolderView* app_list_folder_view_;  // Owned by views hierarchy.
  ShowState show_state_;

  // The transitional views for animating the top items in folder
  // when opening or closing a folder.
  std::vector<views::View*> top_icon_views_;

  DISALLOW_COPY_AND_ASSIGN(AppsContainerView);
};

}  // namespace app_list


#endif  // UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
