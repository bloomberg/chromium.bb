// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_container_view.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/folder_background_view.h"
#include "ui/events/event.h"

namespace app_list {

AppsContainerView::AppsContainerView(AppListMainView* app_list_main_view,
                                     AppListModel* model)
    : show_state_(SHOW_NONE), top_icon_animation_pending_count_(0) {
  apps_grid_view_ = new AppsGridView(app_list_main_view);
  apps_grid_view_->SetLayout(kPreferredCols, kPreferredRows);
  AddChildView(apps_grid_view_);

  folder_background_view_ = new FolderBackgroundView();
  AddChildView(folder_background_view_);

  app_list_folder_view_ =
      new AppListFolderView(this, model, app_list_main_view);
  // The folder view is initially hidden.
  app_list_folder_view_->SetVisible(false);
  AddChildView(app_list_folder_view_);

  apps_grid_view_->SetModel(model);
  apps_grid_view_->SetItemList(model->top_level_item_list());
  SetShowState(SHOW_APPS, false);
}

AppsContainerView::~AppsContainerView() {
}

void AppsContainerView::ShowActiveFolder(AppListFolderItem* folder_item) {
  // Prevent new animations from starting if there are currently animations
  // pending. This fixes crbug.com/357099.
  if (top_icon_animation_pending_count_)
    return;

  app_list_folder_view_->SetAppListFolderItem(folder_item);
  SetShowState(SHOW_ACTIVE_FOLDER, false);

  CreateViewsForFolderTopItemsAnimation(folder_item, true);

  apps_grid_view_->ClearAnySelectedView();
}

void AppsContainerView::ShowApps(AppListFolderItem* folder_item) {
  if (top_icon_animation_pending_count_)
    return;

  PrepareToShowApps(folder_item);
  SetShowState(SHOW_APPS, true);
}

void AppsContainerView::ResetForShowApps() {
  SetShowState(SHOW_APPS, false);
  folder_background_view_->UpdateFolderContainerBubble(
      FolderBackgroundView::NO_BUBBLE);
}

void AppsContainerView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_grid_view()->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
  app_list_folder_view()->items_grid_view()->
      SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppsContainerView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  if (top_icon_animation_pending_count_)
    return;

  PrepareToShowApps(folder_item);
  SetShowState(SHOW_ITEM_REPARENT, false);
}

bool AppsContainerView::IsInFolderView() const {
  return show_state_ == SHOW_ACTIVE_FOLDER;
}

void AppsContainerView::ReparentDragEnded() {
  DCHECK_EQ(SHOW_ITEM_REPARENT, show_state_);
  show_state_ = AppsContainerView::SHOW_APPS;
}

gfx::Size AppsContainerView::GetPreferredSize() const {
  const gfx::Size grid_size = apps_grid_view_->GetPreferredSize();
  const gfx::Size folder_view_size = app_list_folder_view_->GetPreferredSize();

  int width = std::max(grid_size.width(), folder_view_size.width());
  int height = std::max(grid_size.height(), folder_view_size.height());
  return gfx::Size(width, height);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  switch (show_state_) {
    case SHOW_APPS:
      apps_grid_view_->SetBoundsRect(rect);
      break;
    case SHOW_ACTIVE_FOLDER:
      folder_background_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetBoundsRect(rect);
      break;
    case SHOW_ITEM_REPARENT:
      break;
    default:
      NOTREACHED();
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

void AppsContainerView::OnWillBeShown() {
  apps_grid_view()->ClearAnySelectedView();
  app_list_folder_view()->items_grid_view()->ClearAnySelectedView();
}

gfx::Rect AppsContainerView::GetPageBoundsForState(
    AppListModel::State state) const {
  gfx::Rect onscreen_bounds = GetDefaultContentsBounds();
  if (state == AppListModel::STATE_APPS)
    return onscreen_bounds;

  return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
}

void AppsContainerView::OnTopIconAnimationsComplete() {
  --top_icon_animation_pending_count_;

  if (!top_icon_animation_pending_count_) {
    // Clean up the transitional views used for top item icon animation.
    top_icon_views_.clear();

    // Show the folder icon when closing the folder.
    if ((show_state_ == SHOW_APPS || show_state_ == SHOW_ITEM_REPARENT) &&
        apps_grid_view_->activated_folder_item_view()) {
      apps_grid_view_->activated_folder_item_view()->SetVisible(true);
    }
  }
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  switch (show_state_) {
    case SHOW_APPS:
      folder_background_view_->SetVisible(false);
      if (show_apps_with_animation) {
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
        apps_grid_view_->ScheduleShowHideAnimation(true);
      } else {
        app_list_folder_view_->HideViewImmediately();
        apps_grid_view_->ResetForShowApps();
      }
      break;
    case SHOW_ACTIVE_FOLDER:
      folder_background_view_->SetVisible(true);
      apps_grid_view_->ScheduleShowHideAnimation(false);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      folder_background_view_->SetVisible(false);
      folder_background_view_->UpdateFolderContainerBubble(
          FolderBackgroundView::NO_BUBBLE);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      apps_grid_view_->ScheduleShowHideAnimation(true);
      break;
    default:
      NOTREACHED();
  }

  app_list_folder_view_->SetBackButtonLabel(IsInFolderView());
  Layout();
}

std::vector<gfx::Rect> AppsContainerView::GetTopItemIconBoundsInActiveFolder() {
  // Get the active folder's icon bounds relative to AppsContainerView.
  AppListItemView* folder_item_view =
      apps_grid_view_->activated_folder_item_view();
  gfx::Rect to_grid_view = folder_item_view->ConvertRectToParent(
      folder_item_view->GetIconBounds());
  gfx::Rect to_container = apps_grid_view_->ConvertRectToParent(to_grid_view);

  return FolderImage::GetTopIconsBounds(to_container);
}

void AppsContainerView::CreateViewsForFolderTopItemsAnimation(
    AppListFolderItem* active_folder,
    bool open_folder) {
  top_icon_views_.clear();
  std::vector<gfx::Rect> top_items_bounds =
      GetTopItemIconBoundsInActiveFolder();
  top_icon_animation_pending_count_ =
      std::min(kNumFolderTopItems, active_folder->item_list()->item_count());
  for (size_t i = 0; i < top_icon_animation_pending_count_; ++i) {
    if (active_folder->GetTopIcon(i).isNull())
      continue;

    TopIconAnimationView* icon_view = new TopIconAnimationView(
        active_folder->GetTopIcon(i), top_items_bounds[i], open_folder);
    icon_view->AddObserver(this);
    top_icon_views_.push_back(icon_view);

    // Add the transitional views into child views, and set its bounds to the
    // same location of the item in the folder list view.
    AddChildView(top_icon_views_[i]);
    top_icon_views_[i]->SetBoundsRect(
        app_list_folder_view_->ConvertRectToParent(
            app_list_folder_view_->GetItemIconBoundsAt(i)));
    static_cast<TopIconAnimationView*>(top_icon_views_[i])->TransformView();
  }
}

void AppsContainerView::PrepareToShowApps(AppListFolderItem* folder_item) {
  if (folder_item)
    CreateViewsForFolderTopItemsAnimation(folder_item, false);

  // Hide the active folder item until the animation completes.
  if (apps_grid_view_->activated_folder_item_view())
    apps_grid_view_->activated_folder_item_view()->SetVisible(false);
}

}  // namespace app_list
