// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
#define UI_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_

#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/views/apps_grid_view.h"
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
class AppListItemView;
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
  // If |show| is false, the view should be set to invisible after the
  // animation is done unless |hide_for_reparent| is true.
  void ScheduleShowHideAnimation(bool show, bool hide_for_reparent);

  // Gets icon image bounds of the item at |index|, relative to
  // AppListFolderView.
  gfx::Rect GetItemIconBoundsAt(int index);

  // Updates the folder view background to show or hide folder container ink
  // bubble.
  void UpdateFolderViewBackground(bool show_bubble);

  void UpdateFolderNameVisibility(bool visible);

  // Returns true if |point| falls outside of the folder container ink bubble.
  bool IsPointOutsideOfFolderBoundray(const gfx::Point& point);

  // Called when a folder item is dragged out of the folder to be re-parented.
  // |original_drag_view| is the |drag_view_| inside the folder's grid view.
  // |drag_point_in_folder_grid| is the last drag point in coordinate of the
  // AppsGridView inside the folder.
  void ReparentItem(AppListItemView* original_drag_view,
      const gfx::Point& drag_point_in_folder_grid);

  // Dispatches drag event from the hidden grid view to the root level grid view
  // for re-parenting a folder item.
  void DispatchDragEventForReparent(AppsGridView::Pointer pointer,
      const ui::LocatedEvent& event);

  // Dispatches EndDrag event from the hidden grid view to the root level grid
  // view for reparenting a folder item.
  // |events_forwarded_to_drag_drop_host|: True if the dragged item is dropped
  // to the drag_drop_host, eg. dropped on shelf.
  void DispatchEndDragEventForReparent(bool events_forwarded_to_drag_drop_host);

  // Hides the view immediately without animation.
  void HideViewImmediately();

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from AppListItemListObserver:
  virtual void OnListItemRemoved(size_t index, AppListItem* item) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  AppsGridView* items_grid_view() { return items_grid_view_; }

 private:
  void CalculateIdealBounds();

  // Starts setting up drag in root level apps grid view for re-parenting a
  // folder item.
  // |drag_point_in_root_grid| is in the cooridnates of root level AppsGridView.
  void StartSetupDragInRootLevelAppsGridView(
      AppListItemView* original_drag_view,
      const gfx::Point& drag_point_in_root_grid);

  // Overridden from FolderHeaderViewDelegate:
  virtual void NavigateBack(AppListFolderItem* item,
                            const ui::Event& event_flags) OVERRIDE;
  virtual void GiveBackFocusToSearchBox() OVERRIDE;

  AppsContainerView* container_view_;  // Not owned.
  AppListMainView* app_list_main_view_;   // Not Owned.
  FolderHeaderView* folder_header_view_;  // Owned by views hierarchy.
  AppsGridView* items_grid_view_;  // Owned by the views hierarchy.

  scoped_ptr<views::ViewModel> view_model_;

  AppListModel* model_;  // Not owned.
  AppListFolderItem* folder_item_;  // Not owned.

  scoped_ptr<PaginationModel> pagination_model_;

  bool hide_for_reparent_;

  DISALLOW_COPY_AND_ASSIGN(AppListFolderView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
