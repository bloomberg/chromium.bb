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

void AppListModel::AddItemAt(size_t index, AppListItemModel* item) {
  items_.AddAt(index, item);
}

void AppListModel::DeleteItemAt(size_t index) {
  items_.DeleteAt(index);
}

AppListItemModel* AppListModel::GetItemAt(size_t index) {
  return items_.GetItemAt(index);
}

void AppListModel::AddObserver(ui::ListModelObserver* observer) {
  items_.AddObserver(observer);
}

void AppListModel::RemoveObserver(ui::ListModelObserver* observer) {
  items_.RemoveObserver(observer);
}

}  // namespace app_list
