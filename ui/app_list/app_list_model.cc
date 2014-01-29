// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"

namespace app_list {

AppListModel::AppListModel()
    : item_list_(new AppListItemList),
      search_box_(new SearchBoxModel),
      results_(new SearchResults),
      status_(STATUS_NORMAL) {
  item_list_->AddObserver(this);
}

AppListModel::~AppListModel() {
  item_list_->RemoveObserver(this);
}

void AppListModel::AddObserver(AppListModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListModel::RemoveObserver(AppListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListModel::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListModelStatusChanged());
}

AppListItem* AppListModel::FindItem(const std::string& id) {
  return item_list_->FindItem(id);
}

void AppListModel::AddItem(AppListItem* item) {
  DCHECK(!item_list()->FindItem(item->id()));
  AddItemToItemList(item);
}

const std::string& AppListModel::MergeItems(const std::string& target_item_id,
                                            const std::string& source_item_id) {
  // First, remove the source item from |item_list_|.
  scoped_ptr<AppListItem> source_item_ptr =
      item_list_->RemoveItem(source_item_id);

  // Next, find the target item.
  AppListItem* target_item = item_list_->FindItem(target_item_id);
  DCHECK(target_item);

  // If the target item is a folder, just add |source_item| to it.
  if (target_item->GetItemType() == AppListFolderItem::kItemType) {
    AppListFolderItem* target_folder =
        static_cast<AppListFolderItem*>(target_item);
    AddItemToFolder(target_folder, source_item_ptr.release());
    return target_folder->id();
  }

  // Otherwise, remove the target item from |item_list_|, it will become owned
  // by the new folder.
  scoped_ptr<AppListItem> target_item_ptr =
      item_list_->RemoveItem(target_item_id);

  // Create a new folder in the same location as the target item.
  AppListFolderItem* new_folder =
      new AppListFolderItem(AppListFolderItem::GenerateId());
  new_folder->set_position(target_item->position());
  AddItemToItemList(new_folder);

  // Add the items to the new folder.
  AddItemToFolder(new_folder, target_item_ptr.release());
  AddItemToFolder(new_folder, source_item_ptr.release());

  return new_folder->id();
}

void AppListModel::SetItemPosition(AppListItem* item,
                                   const syncer::StringOrdinal& new_position) {
  item_list_->SetItemPosition(item, new_position);
  // Note: this will trigger OnListItemMoved which will signal observers.
  // (This is done this way because some View code still moves items within
  // the item list directly).
}

void AppListModel::DeleteItem(const std::string& id) {
  AppListItem* item = FindItem(id);
  if (!item)
    return;
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemWillBeDeleted(item));
  item_list_->DeleteItem(id);
}

// Private methods

void AppListModel::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
}

syncer::StringOrdinal AppListModel::GetLastItemPosition() {
  size_t count = item_list_->item_count();
  if (count == 0)
    return syncer::StringOrdinal::CreateInitialOrdinal();
  return item_list_->item_at(count - 1)->position();
}

void AppListModel::AddItemToItemList(AppListItem* item) {
  item_list_->AddItem(item);
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemAdded(item));
}

void AppListModel::AddItemToFolder(AppListFolderItem* folder,
                                   AppListItem* item) {
  folder->item_list()->AddItem(item);
}

}  // namespace app_list
