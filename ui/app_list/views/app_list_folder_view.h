// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_

#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/views/folder_header_view.h"
#include "ui/app_list/views/folder_header_view_delegate.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace views {
class ViewModel;
}

namespace app_list {

class AppsContainerView;
class AppsGridView;
class AppListFolderItem;
class AppListMainView;
class AppListModel;
class FolderHeaderView;
class PaginationModel;

class AppListFolderView : public views::View,
                          public FolderHeaderViewDelegate,
                          public AppListItemListObserver,
                          public ui::ImplicitAnimationObserver {
 public:
  AppListFolderView(AppsContainerView* container_view,
                    AppListModel* model,
                    AppListMainView* app_list_main_view,
                    content::WebContents* start_page_contents);
  virtual ~AppListFolderView();

  void SetAppListFolderItem(AppListFolderItem* folder);

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

  // Gets icon image bounds of the item at |index|, relative to
  // AppListFolderView.
  gfx::Rect GetItemIconBoundsAt(int index);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from AppListItemListObserver:
  virtual void OnListItemRemoved(size_t index, AppListItem* item) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

 private:
  void CalculateIdealBounds();

  // Overridden from FolderHeaderViewDelegate:
  virtual void NavigateBack(AppListFolderItem* item,
                            const ui::Event& event_flags) OVERRIDE;

  AppsContainerView* container_view_;  // Not owned.
  FolderHeaderView* folder_header_view_;  // Owned by views hierarchy.
  AppsGridView* items_grid_view_;  // Owned by the views hierarchy.

  scoped_ptr<views::ViewModel> view_model_;

  AppListModel* model_;  // Not owned.
  AppListFolderItem* folder_item_;  // Not owned.

  scoped_ptr<PaginationModel> pagination_model_;

  DISALLOW_COPY_AND_ASSIGN(AppListFolderView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
