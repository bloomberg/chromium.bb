// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_container_view.h"

#include <algorithm>

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/events/event.h"

namespace app_list {

AppsContainerView::AppsContainerView(AppListMainView* app_list_main_view,
                                     PaginationModel* pagination_model,
                                     AppListModel* model,
                                     content::WebContents* start_page_contents)
    : model_(model),
      show_state_(SHOW_APPS) {
  apps_grid_view_ = new AppsGridView(
      app_list_main_view, pagination_model, start_page_contents);
  apps_grid_view_->SetLayout(kPreferredIconDimension,
                             kPreferredCols,
                             kPreferredRows);
  AddChildView(apps_grid_view_);

  app_list_folder_view_ = new AppListFolderView(
      this,
      model,
      app_list_main_view,
      start_page_contents);
  AddChildView(app_list_folder_view_);

  apps_grid_view_->SetModel(model_);
  apps_grid_view_->SetItemList(model_->item_list());
}

AppsContainerView::~AppsContainerView() {
}

void AppsContainerView::ShowActiveFolder(AppListFolderItem* folder_item) {
  app_list_folder_view_->SetAppListFolderItem(folder_item);
  SetShowState(SHOW_ACTIVE_FOLDER);
}

void AppsContainerView::ShowApps() {
  SetShowState(SHOW_APPS);
}

gfx::Size AppsContainerView::GetPreferredSize() {
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
      app_list_folder_view_->SetVisible(false);
      apps_grid_view_->SetBoundsRect(rect);
      apps_grid_view_->SetVisible(true);
      break;
    case SHOW_ACTIVE_FOLDER:
      apps_grid_view_->SetVisible(false);
      app_list_folder_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetVisible(true);
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

void AppsContainerView::SetShowState(ShowState show_state) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;
  Layout();
}

}  // namespace app_list
