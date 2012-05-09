// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

namespace app_list {

AppListModel::AppListModel() {
}

AppListModel::~AppListModel() {
}

void AppListModel::AddItem(AppListItemModel* item) {
  items_.Add(item);
}

void AppListModel::AddItemAt(int index, AppListItemModel* item) {
  items_.AddAt(index, item);
}

void AppListModel::DeleteItemAt(int index) {
  items_.DeleteAt(index);
}

AppListItemModel* AppListModel::GetItem(int index) {
  return items_.item_at(index);
}

void AppListModel::AddObserver(ui::ListModelObserver* observer) {
  items_.AddObserver(observer);
}

void AppListModel::RemoveObserver(ui::ListModelObserver* observer) {
  items_.RemoveObserver(observer);
}

}  // namespace app_list
