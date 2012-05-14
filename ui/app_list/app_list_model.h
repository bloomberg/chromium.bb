// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_MODEL_H_
#define UI_APP_LIST_APP_LIST_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/base/models/list_model.h"

namespace app_list {

// Model for AppListModelView that owns a list of AppListItemModel.
class APP_LIST_EXPORT AppListModel {
 public:
  AppListModel();
  ~AppListModel();

  // Adds an item to the model. The model takes ownership of |item|.
  void AddItem(AppListItemModel* item);
  void AddItemAt(int index, AppListItemModel* item);

  void DeleteItemAt(int index);

  AppListItemModel* GetItemAt(int index);

  void AddObserver(ui::ListModelObserver* observer);
  void RemoveObserver(ui::ListModelObserver* observer);

  int item_count() const { return items_.item_count(); }

 private:
  typedef ui::ListModel<AppListItemModel> Items;
  Items items_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_MODEL_H_
