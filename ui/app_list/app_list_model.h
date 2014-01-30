// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_MODEL_H_
#define UI_APP_LIST_APP_LIST_MODEL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/base/models/list_model.h"

namespace app_list {

class AppListFolderItem;
class AppListItem;
class AppListItemList;
class AppListModelObserver;
class SearchBoxModel;
class SearchResult;

// Master model of app list that consists of three sub models: AppListItemList,
// SearchBoxModel and SearchResults. The AppListItemList sub model owns a list
// of AppListItems and is displayed in the grid view. SearchBoxModel is
// the model for SearchBoxView. SearchResults owns a list of SearchResult.
// NOTE: Currently this class observes |item_list_|. The View code may
// move entries in the item list directly (but can not add or remove them) and
// the model needs to notify its observers when this occurs.

class APP_LIST_EXPORT AppListModel : public AppListItemListObserver {
 public:
  enum Status {
    STATUS_NORMAL,
    STATUS_SYNCING,  // Syncing apps or installing synced apps.
  };

  typedef ui::ListModel<SearchResult> SearchResults;

  AppListModel();
  virtual ~AppListModel();

  void AddObserver(AppListModelObserver* observer);
  void RemoveObserver(AppListModelObserver* observer);

  void SetStatus(Status status);

  // Finds the item matching |id|.
  AppListItem* FindItem(const std::string& id);

  // Find a folder item matching |id|.
  AppListFolderItem* FindFolderItem(const std::string& id);

  // Adds |item| to the model. The model takes ownership of |item|.
  void AddItem(AppListItem* item);

  // Adds |item| to an existing folder or creates a new folder. If |folder_id|
  // is empty, calls AddItem() instead. The model takes ownership of |item|.
  void AddItemToFolder(AppListItem* item, const std::string& folder_id);

  // Merges two items. If the target item is a folder, the source item is added
  // to that folder, otherwise a new folder is created in the same position as
  // the target item.  Returns the id of the target folder.
  const std::string& MergeItems(const std::string& target_item_id,
                                const std::string& source_item_id);

  // Move |item| to the folder matching |folder_id| or out of any current
  // folder if |folder_id| is empty.
  void MoveItemToFolder(AppListItem* item, const std::string& folder_id);

  // Sets the position of |item| either in |item_list_| or the folder specified
  // by |item|->folder_id().
  void SetItemPosition(AppListItem* item,
                       const syncer::StringOrdinal& new_position);

  // Deletes the item matching |id| from |item_list_| or from its folder.
  void DeleteItem(const std::string& id);

  AppListItemList* item_list() { return item_list_.get(); }
  SearchBoxModel* search_box() { return search_box_.get(); }
  SearchResults* results() { return results_.get(); }
  Status status() const { return status_; }

 private:
  // AppListItemListObserver
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) OVERRIDE;

  // Returns the position of the last item in |item_list_| or an initial one.
  syncer::StringOrdinal GetLastItemPosition();

  // Returns an existing folder matching |folder_id| or creates a new folder.
  AppListFolderItem* FindOrCreateFolderItem(const std::string& folder_id);

  // Adds |item| to |item_list_| and notifies observers.
  void AddItemToItemList(AppListItem* item);

  // Adds |item| to |folder| and notifies observers.
  void AddItemToFolderItem(AppListFolderItem* folder, AppListItem* item);

  // Removes |item| from |folder|. If |folder| becomes empty, deletes |folder|
  // from |item_list_|. Does NOT trigger observers, calling function must do so.
  scoped_ptr<AppListItem> RemoveItemFromFolder(AppListFolderItem* folder,
                                               AppListItem* item);

  scoped_ptr<AppListItemList> item_list_;
  scoped_ptr<SearchBoxModel> search_box_;
  scoped_ptr<SearchResults> results_;

  Status status_;
  ObserverList<AppListModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_MODEL_H_
