// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_ITEM_OBSERVER_H_
#define UI_APP_LIST_APP_LIST_ITEM_OBSERVER_H_

#include "ui/app_list/app_list_export.h"

namespace app_list {

class APP_LIST_EXPORT AppListItemObserver {
 public:
  // Invoked after item's icon is changed.
  virtual void ItemIconChanged() {}

  // Invoked after item's name is changed.
  virtual void ItemNameChanged() {}

  // Invoked after item's highlighted state is changed.
  virtual void ItemHighlightedChanged() {}

  // Invoked after item begins or finishes installing.
  virtual void ItemIsInstallingChanged() {}

  // Invoked after item's download percentage changes.
  virtual void ItemPercentDownloadedChanged() {}

  // Invoked when the item is about to be destroyed.
  virtual void ItemBeingDestroyed() {}

 protected:
  virtual ~AppListItemObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_ITEM_OBSERVER_H_
