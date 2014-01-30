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
  AppListItem* item = item_list_->FindItem(id);
  if (item)
    return item;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    AppListItem* child_item = item_list_->item_at(i)->FindChildItem(id);
    if (child_item)
      return child_item;
  }
  return NULL;
}

AppListFolderItem* AppListModel::FindFolderItem(const std::string& id) {
  AppListItem* item = item_list_->FindItem(id);
  if (item && item->GetItemType() == AppListFolderItem::kItemType)
    return static_cast<AppListFolderItem*>(item);
  DCHECK(!item);
  return NULL;
}

void AppListModel::AddItem(AppListItem* item) {
  DCHECK(!item->IsInFolder());
  DCHECK(!item_list()->FindItem(item->id()));
  AddItemToItemList(item);
}

void AppListModel::AddItemToFolder(AppListItem* item,
                                   const std::string& folder_id) {
  if (folder_id.empty()) {
    AddItem(item);
    return;
  }
  DCHECK(!item->IsInFolder() || item->folder_id() == folder_id);
  DCHECK(item->GetItemType() != AppListFolderItem::kItemType);
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  DCHECK(!dest_folder->item_list()->FindItem(item->id()));
  AddItemToFolderItem(dest_folder, item);
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
    AddItemToFolderItem(target_folder, source_item_ptr.release());
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
  AddItemToFolderItem(new_folder, target_item_ptr.release());
  AddItemToFolderItem(new_folder, source_item_ptr.release());

  return new_folder->id();
}

void AppListModel::MoveItemToFolder(AppListItem* item,
                                    const std::string& folder_id) {
  if (item->folder_id() == folder_id)
    return;

  // First, get or create the destination folder.
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);

  // Second, remove the item from its current location.
  scoped_ptr<AppListItem> item_ptr;
  if (item->IsInFolder()) {
    AppListFolderItem* cur_folder = FindFolderItem(item->folder_id());
    DCHECK(cur_folder);
    item_ptr = RemoveItemFromFolder(cur_folder, item);
  } else {
    item_ptr = item_list_->RemoveItem(item->id());
  }
  DCHECK(item_ptr);
  DCHECK(!item_ptr->IsInFolder());

  // Finally, add the item to the new location and trigger observers.
  if (dest_folder) {
    AddItemToFolderItem(dest_folder, item_ptr.release());
  } else {
    item_list_->AddItem(item_ptr.release());
    FOR_EACH_OBSERVER(AppListModelObserver,
                      observers_,
                      OnAppListItemUpdated(item));
  }
}

void AppListModel::SetItemPosition(AppListItem* item,
                                   const syncer::StringOrdinal& new_position) {
  if (!item->IsInFolder()) {
    item_list_->SetItemPosition(item, new_position);
    // Note: this will trigger OnListItemMoved which will signal observers.
    // (This is done this way because some View code still moves items within
    // the item list directly).
    return;
  }
  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  DCHECK(folder);
  folder->item_list()->SetItemPosition(item, new_position);
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
}

void AppListModel::DeleteItem(const std::string& id) {
  AppListItem* item = FindItem(id);
  if (!item)
    return;
  if (!item->IsInFolder()) {
    DCHECK_EQ(0u, item->ChildItemCount())
        << "Invalid call to DeleteItem for item with children: " << id;
    FOR_EACH_OBSERVER(AppListModelObserver,
                      observers_,
                      OnAppListItemWillBeDeleted(item));
    item_list_->DeleteItem(id);
    return;
  }
  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  DCHECK(folder) << "Folder not found for item: " << item->ToDebugString();
  scoped_ptr<AppListItem> child_item = RemoveItemFromFolder(folder, item);
  DCHECK_EQ(item, child_item.get());
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemWillBeDeleted(item));
  child_item.reset();  // Deletes item.
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

AppListFolderItem* AppListModel::FindOrCreateFolderItem(
    const std::string& folder_id) {
  if (folder_id.empty())
    return NULL;

  AppListFolderItem* dest_folder = FindFolderItem(folder_id);
  if (!dest_folder) {
    dest_folder = new AppListFolderItem(folder_id);
    dest_folder->set_position(GetLastItemPosition().CreateAfter());
    AddItemToItemList(dest_folder);
  }
  return dest_folder;
}

void AppListModel::AddItemToItemList(AppListItem* item) {
  DCHECK(!item->IsInFolder());
  item_list_->AddItem(item);
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemAdded(item));
}

void AppListModel::AddItemToFolderItem(AppListFolderItem* folder,
                                       AppListItem* item) {
  folder->item_list()->AddItem(item);
  item->set_folder_id(folder->id());
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
}

scoped_ptr<AppListItem> AppListModel::RemoveItemFromFolder(
    AppListFolderItem* folder,
    AppListItem* item) {
  DCHECK_EQ(item->folder_id(), folder->id());
  scoped_ptr<AppListItem> result = folder->item_list()->RemoveItem(item->id());
  if (folder->item_list()->item_count() == 0)
    DeleteItem(folder->id());
  item->set_folder_id("");
  return result.Pass();
}

}  // namespace app_list
