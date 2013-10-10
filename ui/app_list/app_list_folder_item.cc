// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_folder_item.h"

namespace app_list {

AppListFolderItem::AppListFolderItem(const std::string& id)
    : AppListItemModel(id),
      apps_(new Apps) {
}

AppListFolderItem::~AppListFolderItem() {
}

void AppListFolderItem::AddItem(AppListItemModel* item) {
  std::string sort_order = item->GetSortOrder();
  // Note: ui::ListModel is not a sorted list.
  size_t index = 0;
  for (; index < apps_->item_count(); ++index) {
    if (sort_order < apps_->GetItemAt(index)->GetSortOrder())
      break;
  }
  apps_->AddAt(index, item);
}

void AppListFolderItem::DeleteItem(const std::string& id) {
  for (size_t i = 0; i < apps_->item_count(); ++i) {
    AppListItemModel* item = apps_->GetItemAt(i);
    if (item->id() == id) {
      apps_->DeleteAt(i);
      return;
    }
  }
}

std::string AppListFolderItem::GetSortOrder() const {
  // For now, put folders at the end of the list.
  // TODO(stevenjb): Implement synced app list ordering.
  return "zzzzzzzz";
}

void AppListFolderItem::Activate(int event_flags) {
  // TODO(stevenjb/jennyz): Implement.
  VLOG(1) << "AppListFolderItem::Activate";
}

// static
const char AppListFolderItem::kAppType[] = "FolderItem";

const char* AppListFolderItem::GetAppType() const {
  return AppListFolderItem::kAppType;
}

ui::MenuModel* AppListFolderItem::GetContextMenuModel() {
  // TODO(stevenjb/jennyz): Implement.
  return NULL;
}

}  // namespace app_list
