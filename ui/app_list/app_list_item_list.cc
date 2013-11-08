// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_item_list.h"

#include "ui/app_list/app_list_item_model.h"

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

AppListItemModel* AppListItemList::FindItem(const std::string& id) {
  for (size_t i = 0; i < app_list_items_.size(); ++i) {
    AppListItemModel* item = app_list_items_[i];
    if (item->id() == id)
      return item;
  }
  return NULL;
}

bool AppListItemList::FindItemIndex(const std::string& id, size_t* index) {
  for (size_t i = 0; i < app_list_items_.size(); ++i) {
    AppListItemModel* item = app_list_items_[i];
    if (item->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

size_t AppListItemList::AddItem(AppListItemModel* item) {
  size_t index = GetItemSortOrderIndex(item);
  app_list_items_.insert(app_list_items_.begin() + index, item);
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemAdded(index, item));
  return index;
}

void AppListItemList::InsertItemAt(AppListItemModel* item, size_t index) {
  DCHECK_LE(index, item_count());
  if (item_count() == 0) {
    AddItem(item);
    return;
  }

  AppListItemModel* prev =
      index > 0 ? app_list_items_[index - 1] : NULL;
  AppListItemModel* next = index <= app_list_items_.size() - 1 ?
      app_list_items_[index] : NULL;
  CHECK_NE(prev, next);

  if (prev && next && prev->position().Equals(next->position()))
    prev = NULL;

  if (!prev)
    item->set_position(next->position().CreateBefore());
  else if (!next)
    item->set_position(prev->position().CreateAfter());
  else
    item->set_position(prev->position().CreateBetween(next->position()));

  app_list_items_.insert(app_list_items_.begin() + index, item);

  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemAdded(index, item));
}

void AppListItemList::DeleteItem(const std::string& id) {
  scoped_ptr<AppListItemModel> item = RemoveItem(id);
  // |item| will be deleted on destruction.
}

void AppListItemList::DeleteItemsByType(const char* type) {
  for (int i = static_cast<int>(app_list_items_.size()) - 1;
       i >= 0; --i) {
    AppListItemModel* item = app_list_items_[i];
    if (!type || item->GetAppType() == type)
      DeleteItemAt(i);
  }
}

scoped_ptr<AppListItemModel> AppListItemList::RemoveItem(
    const std::string& id) {
  size_t index;
  if (FindItemIndex(id, &index))
    return RemoveItemAt(index);

  return scoped_ptr<AppListItemModel>();
}

scoped_ptr<AppListItemModel> AppListItemList::RemoveItemAt(size_t index) {
  DCHECK_LT(index, item_count());
  AppListItemModel* item = app_list_items_[index];
  app_list_items_.weak_erase(app_list_items_.begin() + index);
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemRemoved(index, item));
  return make_scoped_ptr<AppListItemModel>(item);
}

void AppListItemList::MoveItem(size_t from_index, size_t to_index) {
  DCHECK_LT(from_index, item_count());
  DCHECK_LT(to_index, item_count());
  if (from_index == to_index)
    return;

  AppListItemModel* target_item = app_list_items_[from_index];
  app_list_items_.weak_erase(app_list_items_.begin() + from_index);
  app_list_items_.insert(app_list_items_.begin() + to_index, target_item);

  // Update position
  AppListItemModel* prev = to_index > 0 ? app_list_items_[to_index - 1] : NULL;
  AppListItemModel* next = to_index < app_list_items_.size() - 1 ?
      app_list_items_[to_index + 1] : NULL;
  CHECK_NE(prev, next);

  // It is possible that items were added with the same ordinal. Rather than
  // resolving a potentially complicated chain of conflicts, just set the
  // ordinal before |next| (which will place it before both items).
  if (prev && next && prev->position().Equals(next->position()))
    prev = NULL;

  VLOG(2) << "Move: " << target_item->position().ToDebugString()
          << " Prev: " << (prev ? prev->position().ToDebugString() : "(none)")
          << " Next: " << (next ? next->position().ToDebugString() : "(none)");
  if (!prev)
    target_item->set_position(next->position().CreateBefore());
  else if (!next)
    target_item->set_position(prev->position().CreateAfter());
  else
    target_item->set_position(prev->position().CreateBetween(next->position()));
  FOR_EACH_OBSERVER(AppListItemListObserver,
                    observers_,
                    OnListItemMoved(from_index, to_index, target_item));
}

// AppListItemList private

void AppListItemList::DeleteItemAt(size_t index) {
  scoped_ptr<AppListItemModel> item = RemoveItemAt(index);
  // |item| will be deleted on destruction.
}

size_t AppListItemList::GetItemSortOrderIndex(AppListItemModel* item) {
  syncer::StringOrdinal position = item->position();
  if (!position.IsValid()) {
    size_t nitems = app_list_items_.size();
    if (nitems == 0) {
      position = syncer::StringOrdinal::CreateInitialOrdinal();
    } else {
      position = app_list_items_[nitems - 1]->position().CreateAfter();
    }
    item->set_position(position);
    return nitems;
  }
  // Note: app_list_items_ is sorted by convention, but sorting is not enforced
  // (items' positions might be changed outside this class).
  size_t index = 0;
  for (; index < app_list_items_.size(); ++index) {
    if (position.LessThan(app_list_items_[index]->position()))
      break;
  }
  return index;
}

}  // namespace app_list
