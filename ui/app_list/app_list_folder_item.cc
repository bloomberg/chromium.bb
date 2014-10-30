// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_folder_item.h"

#include "base/guid.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/folder_image_source.h"
#include "ui/gfx/geometry/rect.h"

namespace app_list {

AppListFolderItem::AppListFolderItem(const std::string& id,
                                     FolderType folder_type)
    : AppListItem(id),
      folder_type_(folder_type),
      item_list_(new AppListItemList) {
  item_list_->AddObserver(this);
}

AppListFolderItem::~AppListFolderItem() {
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_items_[i]->RemoveObserver(this);
  item_list_->RemoveObserver(this);
}

void AppListFolderItem::UpdateIcon() {
  FolderImageSource::Icons top_icons;
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_icons.push_back(top_items_[i]->icon());

  const gfx::Size icon_size = gfx::Size(kGridIconDimension, kGridIconDimension);
  gfx::ImageSkia icon = gfx::ImageSkia(
      new FolderImageSource(top_icons, icon_size),
      icon_size);
  SetIcon(icon, false);
}

const gfx::ImageSkia& AppListFolderItem::GetTopIcon(size_t item_index) {
  CHECK_LT(item_index, top_items_.size());
  return top_items_[item_index]->icon();
}

gfx::Rect AppListFolderItem::GetTargetIconRectInFolderForItem(
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) {
  for (size_t i = 0; i < top_items_.size(); ++i) {
    if (item->id() == top_items_[i]->id()) {
      std::vector<gfx::Rect> rects =
          FolderImageSource::GetTopIconsBounds(folder_icon_bounds);
      return rects[i];
    }
  }

  gfx::Rect target_rect(folder_icon_bounds);
  target_rect.ClampToCenteredSize(FolderImageSource::ItemIconSize());
  return target_rect;
}

void AppListFolderItem::Activate(int event_flags) {
  // Folder handling is implemented by the View, so do nothing.
}

// static
const char AppListFolderItem::kItemType[] = "FolderItem";

const char* AppListFolderItem::GetItemType() const {
  return AppListFolderItem::kItemType;
}

ui::MenuModel* AppListFolderItem::GetContextMenuModel() {
  // TODO(stevenjb/jennyz): Implement.
  return NULL;
}

AppListItem* AppListFolderItem::FindChildItem(const std::string& id) {
  return item_list_->FindItem(id);
}

size_t AppListFolderItem::ChildItemCount() const {
  return item_list_->item_count();
}

void AppListFolderItem::OnExtensionPreferenceChanged() {
  for (size_t i = 0; i < item_list_->item_count(); ++i)
    item_list_->item_at(i)->OnExtensionPreferenceChanged();
}

bool AppListFolderItem::CompareForTest(const AppListItem* other) const {
  if (!AppListItem::CompareForTest(other))
    return false;
  const AppListFolderItem* other_folder =
      static_cast<const AppListFolderItem*>(other);
  if (other_folder->item_list()->item_count() != item_list_->item_count())
    return false;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    if (!item_list()->item_at(i)->CompareForTest(
            other_folder->item_list()->item_at(i)))
      return false;
  }
  return true;
}

std::string AppListFolderItem::GenerateId() {
  return base::GenerateGUID();
}

void AppListFolderItem::ItemIconChanged() {
  UpdateIcon();
}

void AppListFolderItem::OnListItemAdded(size_t index,
                                        AppListItem* item) {
  if (index < kNumFolderTopItems)
    UpdateTopItems();
}

void AppListFolderItem::OnListItemRemoved(size_t index,
                                          AppListItem* item) {
  if (index < kNumFolderTopItems)
    UpdateTopItems();
}

void AppListFolderItem::OnListItemMoved(size_t from_index,
                                        size_t to_index,
                                        AppListItem* item) {
  if (from_index < kNumFolderTopItems || to_index < kNumFolderTopItems)
    UpdateTopItems();
}

void AppListFolderItem::UpdateTopItems() {
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_items_[i]->RemoveObserver(this);
  top_items_.clear();

  for (size_t i = 0;
       i < kNumFolderTopItems && i < item_list_->item_count(); ++i) {
    AppListItem* item = item_list_->item_at(i);
    item->AddObserver(this);
    top_items_.push_back(item);
  }
  UpdateIcon();
}

}  // namespace app_list
