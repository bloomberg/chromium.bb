// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_folder_view.h"

#include <algorithm>

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/folder_header_view.h"
#include "ui/events/event.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

namespace app_list {

namespace {

// Indexes of interesting views in ViewModel of AppListFolderView.
const int kIndexFolderHeader = 0;
const int kIndexChildItems = 1;

}  // namespace

AppListFolderView::AppListFolderView(AppsContainerView* container_view,
                                     AppListModel* model,
                                     AppListMainView* app_list_main_view,
                                     content::WebContents* start_page_contents)
    : container_view_(container_view),
      folder_header_view_(new FolderHeaderView(this)),
      view_model_(new views::ViewModel),
      folder_item_(NULL),
      pagination_model_(new PaginationModel) {
  AddChildView(folder_header_view_);
  view_model_->Add(folder_header_view_, kIndexFolderHeader);

  items_grid_view_ = new AppsGridView(
      app_list_main_view, pagination_model_.get(), NULL);
  items_grid_view_->set_is_root_level(false);
  items_grid_view_->SetLayout(kPreferredIconDimension,
                              kPreferredCols,
                              kPreferredRows);
  items_grid_view_->SetModel(model);
  AddChildView(items_grid_view_);
  view_model_->Add(items_grid_view_, kIndexChildItems);
}

AppListFolderView::~AppListFolderView() {
  // Make sure |items_grid_view_| is deleted before |pagination_model_|.
  RemoveAllChildViews(true);
}

void AppListFolderView::SetAppListFolderItem(AppListFolderItem* folder) {
  folder_item_ = folder;
  items_grid_view_->SetItemList(folder_item_->item_list());
  folder_header_view_->SetFolderItem(folder_item_);
}

gfx::Size AppListFolderView::GetPreferredSize() {
  const gfx::Size header_size = folder_header_view_->GetPreferredSize();
  const gfx::Size grid_size = items_grid_view_->GetPreferredSize();
  int width = std::max(header_size.width(), grid_size.width());
  int height = header_size.height() + grid_size.height();
  return gfx::Size(width, height);
}

void AppListFolderView::Layout() {
  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
}

bool AppListFolderView::OnKeyPressed(const ui::KeyEvent& event) {
  return items_grid_view_->OnKeyPressed(event);
}

void AppListFolderView::CalculateIdealBounds() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect header_frame(rect);
  gfx::Size size = folder_header_view_->GetPreferredSize();
  header_frame.set_height(size.height());
  view_model_->set_ideal_bounds(kIndexFolderHeader, header_frame);

  gfx::Rect grid_frame(rect);
  grid_frame.set_y(header_frame.height());
  view_model_->set_ideal_bounds(kIndexChildItems, grid_frame);
}

void AppListFolderView::NavigateBack(AppListFolderItem* item,
                                     const ui::Event& event_flags) {
  container_view_->ShowApps();
}

}  // namespace app_list
