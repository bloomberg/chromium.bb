// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_LIST_MODEL_H_
#define UI_BASE_MODELS_LIST_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/base/models/list_model_observer.h"

namespace ui {

// A list model that manages a list of ItemType pointers. Items added to the
// model are owned by the model. An item can be taken out of the model by
// RemoveAt.
template <class ItemType>
class ListModel {
 public:
  ListModel() {}
  virtual ~ListModel() {}

  // Adds |item| to the model at given |index|.
  virtual void AddAt(int index, ItemType* item) {
    DCHECK(index >= 0 && index <= item_count());
    items_->insert(items_.begin() + index, item);
    NotifyItemsAdded(index, 1);
  }

  // Removes an item at given |index| from the model. Note the removed item
  // is NOT deleted and it's up to the caller to delete it.
  virtual ItemType* RemoveAt(int index) {
    DCHECK(index >= 0 && index < item_count());
    ItemType* item = items_[index];
    items_->erase(items_.begin() + index);
    NotifyItemsRemoved(index, 1);
    return item;
  }

  // Removes all items from the model. This does NOT delete the items.
  virtual void RemoveAll() {
    int count = item_count();
    items_->clear();
    NotifyItemsRemoved(0, count);
  }

  // Removes an item at given |index| from the model and deletes it.
  virtual void DeleteAt(int index) {
    delete RemoveAt(index);
  }

  // Removes and deletes all items from the model.
  virtual void DeleteAll() {
    int count = item_count();
    items_.reset();
    NotifyItemsRemoved(0, count);
  }

  // Convenience function to append an item to the model.
  void Add(ItemType* item) {
    AddAt(item_count(), item);
  }

  void AddObserver(ListModelObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(ListModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyItemsAdded(int start, int count) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemsAdded(start, count));
  }

  void NotifyItemsRemoved(int start, int count) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemsRemoved(start, count));
  }

  void NotifyItemsChanged(int start, int count) {
    FOR_EACH_OBSERVER(ListModelObserver,
                      observers_,
                      ListItemsChanged(start, count));
  }

  int item_count() const { return static_cast<int>(items_.size()); }

  const ItemType* GetItemAt(int index) const {
    DCHECK(index >= 0 && index < item_count());
    return items_[index];
  }
  ItemType* GetItemAt(int index) {
    return const_cast<ItemType*>(
        const_cast<const ListModel<ItemType>*>(this)->GetItemAt(index));
  }

 private:
  ScopedVector<ItemType> items_;
  ObserverList<ListModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ListModel<ItemType>);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_LIST_MODEL_H_
