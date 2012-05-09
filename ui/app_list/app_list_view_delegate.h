// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#pragma once

#include <string>

#include "ui/app_list/app_list_export.h"

namespace app_list {

class AppListItemModel;
class AppListModel;

class APP_LIST_EXPORT AppListViewDelegate {
 public:
  // AppListView owns the delegate.
  virtual ~AppListViewDelegate() {}

  // Invoked to set the model that AppListView uses.
  // Note that AppListView owns the model.
  virtual void SetModel(AppListModel* model) = 0;

  // Invoked to ask the delegate to populate the model for given |query|.
  virtual void UpdateModel(const std::string& query) = 0;

  // Invoked an AppListeItemModelView is  activated by click or enter key.
  virtual void OnAppListItemActivated(AppListItemModel* item,
                                      int event_flags) = 0;

  // Invoked to close app list.
  virtual void Close() = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
