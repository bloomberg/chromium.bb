// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_ITEM_LIST_OBSERVER_H_
#define UI_APP_LIST_APP_LIST_ITEM_LIST_OBSERVER_H_

#include "base/basictypes.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {

class AppListItem;

class APP_LIST_EXPORT AppListItemListObserver {
 public:
  // Triggered after |item| has been added to the list at |index|.
  virtual void OnListItemAdded(size_t index, AppListItem* item) {}

  // Triggered after an item has been removed from the list at |index|, just
  // before the item is deleted.
  virtual void OnListItemRemoved(size_t index, AppListItem* item) {}

  // Triggered after |item| has been moved from |from_index| to |to_index|.
  // Note: |from_index| may equal |to_index| if only the ordinal has changed.
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) {}

 protected:
  virtual ~AppListItemListObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_ITEM_LIST_OBSERVER_H_
