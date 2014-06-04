// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include <string>

#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"

namespace app_list {

AppListModel::AppListModel()
    : top_level_item_list_(new AppListItemList),
      search_box_(new SearchBoxModel),
      results_(new SearchResults),
      status_(STATUS_NORMAL),
      folders_enabled_(false) {
  top_level_item_list_->AddObserver(this);
}

AppListModel::~AppListModel() { top_level_item_list_->RemoveObserver(this); }

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
  AppListItem* item = top_level_item_list_->FindItem(id);
  if (item)
    return item;
  for (size_t i = 0; i < top_level_item_list_->item_count(); ++i) {
    AppListItem* child_item =
        top_level_item_list_->item_at(i)->FindChildItem(id);
    if (child_item)
      return child_item;
  }
  return NULL;
}

AppListFolderItem* AppListModel::FindFolderItem(const std::string& id) {
  AppListItem* item = top_level_item_list_->FindItem(id);
  if (item && item->GetItemType() == AppListFolderItem::kItemType)
    return static_cast<AppListFolderItem*>(item);
  DCHECK(!item);
  return NULL;
}

AppListItem* AppListModel::AddItem(scoped_ptr<AppListItem> item) {
  DCHECK(!item->IsInFolder());
  DCHECK(!top_level_item_list()->FindItem(item->id()));
  return AddItemToItemListAndNotify(item.Pass());
}

AppListItem* AppListModel::AddItemToFolder(scoped_ptr<AppListItem> item,
                                           const std::string& folder_id) {
  if (folder_id.empty())
    return AddItem(item.Pass());
  DVLOG(2) << "AddItemToFolder: " << item->id() << ": " << folder_id;
  DCHECK(!item->IsInFolder() || item->folder_id() == folder_id);
  DCHECK(item->GetItemType() != AppListFolderItem::kItemType);
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  if (!dest_folder)
    return NULL;
  DCHECK(!dest_folder->item_list()->FindItem(item->id()))
      << "Already in folder: " << dest_folder->id();
  return AddItemToFolderItemAndNotify(dest_folder, item.Pass());
}

const std::string AppListModel::MergeItems(const std::string& target_item_id,
                                           const std::string& source_item_id) {
  if (!folders_enabled()) {
    LOG(ERROR) << "MergeItems called with folders disabled.";
    return "";
  }
  DVLOG(2) << "MergeItems: " << source_item_id << " -> " << target_item_id;
  // Find the target item.
  AppListItem* target_item = FindItem(target_item_id);
  if (!target_item) {
    LOG(ERROR) << "MergeItems: Target no longer exists.";
    return "";
  }
  CHECK(target_item->folder_id().empty());

  AppListItem* source_item = FindItem(source_item_id);
  if (!source_item) {
    LOG(ERROR) << "MergeItems: Source no longer exists.";
    return "";
  }

  // If the target item is a folder, just add the source item to it.
  if (target_item->GetItemType() == AppListFolderItem::kItemType) {
    AppListFolderItem* target_folder =
        static_cast<AppListFolderItem*>(target_item);
    if (target_folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM) {
      LOG(WARNING) << "MergeItems called with OEM folder as target";
      return "";
    }
    scoped_ptr<AppListItem> source_item_ptr = RemoveItem(source_item);
    source_item_ptr->set_position(
        target_folder->item_list()->CreatePositionBefore(
            syncer::StringOrdinal()));
    AddItemToFolderItemAndNotify(target_folder, source_item_ptr.Pass());
    return target_folder->id();
  }

  // Otherwise remove the source item and target item from their current
  // location, they will become owned by the new folder.
  scoped_ptr<AppListItem> source_item_ptr = RemoveItem(source_item);
  CHECK(source_item_ptr);
  scoped_ptr<AppListItem> target_item_ptr =
      top_level_item_list_->RemoveItem(target_item_id);
  CHECK(target_item_ptr);

  // Create a new folder in the same location as the target item.
  std::string new_folder_id = AppListFolderItem::GenerateId();
  DVLOG(2) << "Creating folder for merge: " << new_folder_id;
  scoped_ptr<AppListItem> new_folder_ptr(new AppListFolderItem(
      new_folder_id, AppListFolderItem::FOLDER_TYPE_NORMAL));
  new_folder_ptr->set_position(target_item_ptr->position());
  AppListFolderItem* new_folder = static_cast<AppListFolderItem*>(
      AddItemToItemListAndNotify(new_folder_ptr.Pass()));

  // Add the items to the new folder.
  target_item_ptr->set_position(
      new_folder->item_list()->CreatePositionBefore(
          syncer::StringOrdinal()));
  AddItemToFolderItemAndNotify(new_folder, target_item_ptr.Pass());
  source_item_ptr->set_position(
      new_folder->item_list()->CreatePositionBefore(
          syncer::StringOrdinal()));
  AddItemToFolderItemAndNotify(new_folder, source_item_ptr.Pass());

  return new_folder->id();
}

void AppListModel::MoveItemToFolder(AppListItem* item,
                                    const std::string& folder_id) {
  DVLOG(2) << "MoveItemToFolder: " << folder_id
           << " <- " << item->ToDebugString();
  if (item->folder_id() == folder_id)
    return;
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  scoped_ptr<AppListItem> item_ptr = RemoveItem(item);
  if (dest_folder)
    AddItemToFolderItemAndNotify(dest_folder, item_ptr.Pass());
  else
    AddItemToItemListAndNotifyUpdate(item_ptr.Pass());
}

bool AppListModel::MoveItemToFolderAt(AppListItem* item,
                                      const std::string& folder_id,
                                      syncer::StringOrdinal position) {
  DVLOG(2) << "MoveItemToFolderAt: " << folder_id
           << "[" << position.ToDebugString() << "]"
           << " <- " << item->ToDebugString();
  if (item->folder_id() == folder_id)
    return false;
  AppListFolderItem* src_folder = FindOrCreateFolderItem(item->folder_id());
  if (src_folder &&
      src_folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM) {
    LOG(WARNING) << "MoveItemToFolderAt called with OEM folder as source";
    return false;
  }
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  scoped_ptr<AppListItem> item_ptr = RemoveItem(item);
  if (dest_folder) {
    item_ptr->set_position(
        dest_folder->item_list()->CreatePositionBefore(position));
    AddItemToFolderItemAndNotify(dest_folder, item_ptr.Pass());
  } else {
    item_ptr->set_position(
        top_level_item_list_->CreatePositionBefore(position));
    AddItemToItemListAndNotifyUpdate(item_ptr.Pass());
  }
  return true;
}

void AppListModel::SetItemPosition(AppListItem* item,
                                   const syncer::StringOrdinal& new_position) {
  if (!item->IsInFolder()) {
    top_level_item_list_->SetItemPosition(item, new_position);
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

void AppListModel::SetItemName(AppListItem* item, const std::string& name) {
  item->SetName(name);
  DVLOG(2) << "AppListModel::SetItemName: " << item->ToDebugString();
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
}

void AppListModel::SetItemNameAndShortName(AppListItem* item,
                                           const std::string& name,
                                           const std::string& short_name) {
  item->SetNameAndShortName(name, short_name);
  DVLOG(2) << "AppListModel::SetItemNameAndShortName: "
           << item->ToDebugString();
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
    top_level_item_list_->DeleteItem(id);
    FOR_EACH_OBSERVER(AppListModelObserver, observers_, OnAppListItemDeleted());
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
  FOR_EACH_OBSERVER(AppListModelObserver, observers_, OnAppListItemDeleted());
}

void AppListModel::NotifyExtensionPreferenceChanged() {
  for (size_t i = 0; i < top_level_item_list_->item_count(); ++i)
    top_level_item_list_->item_at(i)->OnExtensionPreferenceChanged();
}

void AppListModel::SetFoldersEnabled(bool folders_enabled) {
  folders_enabled_ = folders_enabled;
  if (folders_enabled)
    return;
  // Remove child items from folders.
  std::vector<std::string> folder_ids;
  for (size_t i = 0; i < top_level_item_list_->item_count(); ++i) {
    AppListItem* item = top_level_item_list_->item_at(i);
    if (item->GetItemType() != AppListFolderItem::kItemType)
      continue;
    AppListFolderItem* folder = static_cast<AppListFolderItem*>(item);
    if (folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM)
      continue;  // Do not remove OEM folders.
    while (folder->item_list()->item_count()) {
      scoped_ptr<AppListItem> child = folder->item_list()->RemoveItemAt(0);
      child->set_folder_id("");
      AddItemToItemListAndNotifyUpdate(child.Pass());
    }
    folder_ids.push_back(folder->id());
  }
  // Delete folders.
  for (size_t i = 0; i < folder_ids.size(); ++i)
    DeleteItem(folder_ids[i]);
}

// Private methods

void AppListModel::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
}

AppListFolderItem* AppListModel::FindOrCreateFolderItem(
    const std::string& folder_id) {
  if (folder_id.empty())
    return NULL;

  AppListFolderItem* dest_folder = FindFolderItem(folder_id);
  if (dest_folder)
    return dest_folder;

  if (!folders_enabled()) {
    LOG(ERROR) << "Attempt to create folder item when disabled: " << folder_id;
    return NULL;
  }

  DVLOG(2) << "Creating new folder: " << folder_id;
  scoped_ptr<AppListFolderItem> new_folder(
      new AppListFolderItem(folder_id, AppListFolderItem::FOLDER_TYPE_NORMAL));
  new_folder->set_position(
      top_level_item_list_->CreatePositionBefore(syncer::StringOrdinal()));
  AppListItem* new_folder_item =
      AddItemToItemListAndNotify(new_folder.PassAs<AppListItem>());
  return static_cast<AppListFolderItem*>(new_folder_item);
}

AppListItem* AppListModel::AddItemToItemListAndNotify(
    scoped_ptr<AppListItem> item_ptr) {
  DCHECK(!item_ptr->IsInFolder());
  AppListItem* item = top_level_item_list_->AddItem(item_ptr.Pass());
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemAdded(item));
  return item;
}

AppListItem* AppListModel::AddItemToItemListAndNotifyUpdate(
    scoped_ptr<AppListItem> item_ptr) {
  DCHECK(!item_ptr->IsInFolder());
  AppListItem* item = top_level_item_list_->AddItem(item_ptr.Pass());
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
  return item;
}

AppListItem* AppListModel::AddItemToFolderItemAndNotify(
    AppListFolderItem* folder,
    scoped_ptr<AppListItem> item_ptr) {
  AppListItem* item = folder->item_list()->AddItem(item_ptr.Pass());
  item->set_folder_id(folder->id());
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListItemUpdated(item));
  return item;
}

scoped_ptr<AppListItem> AppListModel::RemoveItem(AppListItem* item) {
  if (!item->IsInFolder())
    return top_level_item_list_->RemoveItem(item->id());

  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  return RemoveItemFromFolder(folder, item);
}

scoped_ptr<AppListItem> AppListModel::RemoveItemFromFolder(
    AppListFolderItem* folder,
    AppListItem* item) {
  std::string folder_id = folder->id();
  DCHECK_EQ(item->folder_id(), folder_id);
  scoped_ptr<AppListItem> result = folder->item_list()->RemoveItem(item->id());
  result->set_folder_id("");
  if (folder->item_list()->item_count() == 0) {
    DVLOG(2) << "Deleting empty folder: " << folder->ToDebugString();
    DeleteItem(folder_id);
  }
  return result.Pass();
}

}  // namespace app_list
