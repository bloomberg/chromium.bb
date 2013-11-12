// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
#define UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_

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

// AppsContainerView contains a root level AppsGridView to render the root level
// app items, and a AppListFolderView to render the app items inside the
// active folder. Only one if them is visible to user at any time.
class AppsContainerView : public views::View {
 public:
  AppsContainerView(AppListMainView* app_list_main_view,
                    PaginationModel* pagination_model,
                    AppListModel* model,
                    content::WebContents* start_page_contents);
  virtual ~AppsContainerView();

  // Shows the active folder content specified by |folder_item|.
  void ShowActiveFolder(AppListFolderItem* folder_item);

  // Shows the apps list from root.
  void ShowApps();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  AppsGridView* apps_grid_view() { return apps_grid_view_; }

 private:
  enum ShowState {
    SHOW_APPS,
    SHOW_ACTIVE_FOLDER,
  };

  void SetShowState(ShowState show_state);

  AppListModel* model_;
  AppsGridView* apps_grid_view_;  // Owned by views hierarchy.
  AppListFolderView* app_list_folder_view_;  // Owned by views hierarchy.
  ShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(AppsContainerView);
};

}  // namespace app_list


#endif  // UI_APP_LIST_VIEWS_APPS_CONTAINER_VIEW_H_
