// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
#define UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_

#include <string>
#include <vector>

#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_item_model_observer.h"

namespace app_list {

class AppListItemList;

// AppListFolderItem implements the model/controller for folders.
class APP_LIST_EXPORT AppListFolderItem : public AppListItemModel,
                                          public AppListItemListObserver,
                                          public AppListItemModelObserver {
 public:
  explicit AppListFolderItem(const std::string& id);
  virtual ~AppListFolderItem();

  // Updates the folder's icon.
  void UpdateIcon();

  AppListItemList* item_list() { return item_list_.get(); }

  static const char kAppType[];

 private:
  // AppListItemModel
  virtual void Activate(int event_flags) OVERRIDE;
  virtual const char* GetAppType() const OVERRIDE;
  virtual ui::MenuModel* GetContextMenuModel() OVERRIDE;

  // AppListItemModelObserver
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemTitleChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;
  virtual void ItemIsInstallingChanged() OVERRIDE;
  virtual void ItemPercentDownloadedChanged() OVERRIDE;

  // AppListItemListObserver
  virtual void OnListItemAdded(size_t index, AppListItemModel* item) OVERRIDE;
  virtual void OnListItemRemoved(size_t index,
                                 AppListItemModel* item) OVERRIDE;;
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItemModel* item) OVERRIDE;

  void UpdateTopItems();

  scoped_ptr<AppListItemList> item_list_;
  // Top items for generating folder icon.
  std::vector<AppListItemModel*> top_items_;

  DISALLOW_COPY_AND_ASSIGN(AppListFolderItem);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
