// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_item_list.h"

#include "ui/app_list/app_list_item.h"

namespace app_list {

AppListItemList::AppListItemList() {
}

AppListItemList::~AppListItemList() {
}

void AppListItemList::AddObserver(AppListItemListObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItemList::RemoveObserver(AppListItemListObserver* observer) {
  observers_.RemoveObserver(observer);
}

AppListItem* AppListItemList::FindItem(const std::string& id) {
  for (size_t i = 0; i < app_list_items_.size(); ++i) {
    AppListItem* item = app_list_items_[i];
    if (item->id() == id)
      return item;
  }
  return NULL;
}

bool AppListItemList::FindItemIndex(const std::string& id, size_t* index) {
  for (size_t i = 0; i < app_list_items_.size(); ++i) {
    AppListItem* item = app_list_items_[i];
    if (item->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

void AppListItemList::MoveItem(size_t from_index, size_t to_index) {
  DCHECK_LT(from_index, item_count());
  DCHECK_LT(to_index, item_count());
  if (from_index == to_index)
    return;

  AppListItem* target_item = app_list_items_[from_index];
  app_list_items_.weak_erase(app_list_items_.begin() + from_index);
  app_list_items_.insert(app_list_items_.begin() + to_index, target_item);

  // Update position
  AppListItem* prev = to_index > 0 ? app_list_items_[to_index - 1] : NULL;
  AppListItem* next = to_index < app_list_items_.size() - 1 ?
      app_list_items_[to_index + 1] : NULL;
  CHECK_NE(prev, next);

  // It is possible that items were added with the same ordinal. Rather than
  // resolving a potentially complicated chain of conflicts, just set the
  // ordinal before |next| (which will place it before both items).
  if (prev && next && prev->position().Equals(next->position()))
    prev = NULL;

  syncer::StringOrdinal new_position;
  if (!prev)
    new_position = next->position().CreateBefore();
  else if (!next)
    new_position = prev->position().CreateAfter();
  else
    new_position = prev->position().CreateBetween(next->position());
  VLOG(2) << "Move: " << target_item->position().ToDebugString()
          << " Prev: " << (prev ? prev->position().ToDebugString() : "(none)")
          << " Next: " << (next ? next->position().ToDebugString() : "(none)")
          << " -> " << new_position.ToDebugString();
  target_item->set_position(new_position);
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemMoved(from_index, to_index, target_item));
}

void AppListItemList::SetItemPosition(
    AppListItem* item,
    const syncer::StringOrdinal& new_position) {
  DCHECK(item);
  size_t from_index;
  if (!FindItemIndex(item->id(), &from_index)) {
    LOG(ERROR) << "SetItemPosition: Not in list: " << item->id().substr(0, 8);
    return;
  }
  DCHECK(app_list_items_[from_index] == item);
  // First check if the order would remain the same, in which case just update
  // the position.
  size_t to_index = GetItemSortOrderIndex(new_position, item->id());
  if (to_index == from_index) {
    VLOG(2) << "SetItemPosition: No change: " << item->id().substr(0, 8);
    item->set_position(new_position);
    return;
  }
  // Remove the item and get the updated to index.
  app_list_items_.weak_erase(app_list_items_.begin() + from_index);
  to_index = GetItemSortOrderIndex(new_position, item->id());
  VLOG(2) << "SetItemPosition: " << item->id().substr(0, 8)
          << " -> " << new_position.ToDebugString()
          << " From: " << from_index << " To: " << to_index;
  item->set_position(new_position);
  app_list_items_.insert(app_list_items_.begin() + to_index, item);
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemMoved(from_index, to_index, item));
}

// AppListItemList private

syncer::StringOrdinal AppListItemList::CreatePositionBefore(
    const syncer::StringOrdinal& position) {
  if (app_list_items_.empty())
    return syncer::StringOrdinal::CreateInitialOrdinal();

  size_t nitems = app_list_items_.size();
  size_t index;
  if (!position.IsValid()) {
    index = nitems;
  } else {
    for (index = 0; index < nitems; ++index) {
      if (!app_list_items_[index]->position().LessThan(position))
        break;
    }
  }
  if (index == 0)
    return app_list_items_[0]->position().CreateBefore();
  if (index == nitems)
    return app_list_items_[nitems - 1]->position().CreateAfter();
  return app_list_items_[index - 1]->position().CreateBetween(
      app_list_items_[index]->position());
}

AppListItem* AppListItemList::AddItem(scoped_ptr<AppListItem> item_ptr) {
  AppListItem* item = item_ptr.get();
  CHECK(std::find(app_list_items_.begin(), app_list_items_.end(), item)
        == app_list_items_.end());
  EnsureValidItemPosition(item);
  size_t index = GetItemSortOrderIndex(item->position(), item->id());
  app_list_items_.insert(app_list_items_.begin() + index, item_ptr.release());
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemAdded(index, item));
  return item;
}

void AppListItemList::DeleteItem(const std::string& id) {
  scoped_ptr<AppListItem> item = RemoveItem(id);
  // |item| will be deleted on destruction.
}

scoped_ptr<AppListItem> AppListItemList::RemoveItem(
    const std::string& id) {
  size_t index;
  if (FindItemIndex(id, &index))
    return RemoveItemAt(index);

  return scoped_ptr<AppListItem>();
}

scoped_ptr<AppListItem> AppListItemList::RemoveItemAt(size_t index) {
  DCHECK_LT(index, item_count());
  AppListItem* item = app_list_items_[index];
  app_list_items_.weak_erase(app_list_items_.begin() + index);
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemRemoved(index, item));
  return make_scoped_ptr<AppListItem>(item);
}

void AppListItemList::DeleteItemAt(size_t index) {
  scoped_ptr<AppListItem> item = RemoveItemAt(index);
  // |item| will be deleted on destruction.
}

void AppListItemList::EnsureValidItemPosition(AppListItem* item) {
  syncer::StringOrdinal position = item->position();
  if (position.IsValid())
    return;
  size_t nitems = app_list_items_.size();
  if (nitems == 0) {
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  } else {
    position = app_list_items_[nitems - 1]->position().CreateAfter();
  }
  item->set_position(position);
}

size_t AppListItemList::GetItemSortOrderIndex(
    const syncer::StringOrdinal& position,
    const std::string& id) {
  DCHECK(position.IsValid());
  for (size_t index = 0; index < app_list_items_.size(); ++index) {
    if (position.LessThan(app_list_items_[index]->position()) ||
        (position.Equals(app_list_items_[index]->position()) &&
         (id < app_list_items_[index]->id()))) {
      return index;
    }
  }
  return app_list_items_.size();
}

}  // namespace app_list
