// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
#define UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_

#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/base/models/list_model.h"

namespace app_list {

// AppListFolderItem implements the model/controller for folders.
class APP_LIST_EXPORT AppListFolderItem : public AppListItemModel {
 public:
  typedef ui::ListModel<AppListItemModel> Apps;

  explicit AppListFolderItem(const std::string& id);
  virtual ~AppListFolderItem();

  // Adds |item| to |apps_|. Takes ownership of |item|.
  void AddItem(AppListItemModel* item);

  // Finds |item| in |apps_| and deletes it.
  void DeleteItem(const std::string& id);

  Apps* apps() { return apps_.get(); }

  // AppListItemModel
  virtual std::string GetSortOrder() const OVERRIDE;
  virtual void Activate(int event_flags) OVERRIDE;
  virtual const char* GetAppType() const OVERRIDE;
  virtual ui::MenuModel* GetContextMenuModel() OVERRIDE;

  static const char kAppType[];

 private:
  scoped_ptr<Apps> apps_;

  DISALLOW_COPY_AND_ASSIGN(AppListFolderItem);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
