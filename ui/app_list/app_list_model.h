// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_MODEL_H_
#define UI_APP_LIST_APP_LIST_MODEL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"
#include "ui/base/models/list_model.h"

namespace app_list {

class AppListItemModel;
class AppListModelObserver;
class SearchBoxModel;
class SearchResult;

// Master model of app list that consists of three sub models: Apps,
// SearchBoxModel and SearchResults. The Apps sub model owns a list of
// AppListItemModel and is displayed in the grid view. SearchBoxModel is
// the model for SearchBoxView. SearchResults owns a list of SearchResult.
class APP_LIST_EXPORT AppListModel {
 public:
  // A user of the app list.
  struct APP_LIST_EXPORT User {
    User();
    ~User();

    // Whether or not this user is the current user of the app list.
    bool active;

    // The name of this user.
    base::string16 name;

    // The email address of this user.
    base::string16 email;

    // The path to this user's profile directory.
    base::FilePath profile_path;
  };

  enum Status {
    STATUS_NORMAL,
    STATUS_SYNCING,  // Syncing apps or installing synced apps.
  };

  typedef ui::ListModel<AppListItemModel> Apps;
  typedef ui::ListModel<SearchResult> SearchResults;
  typedef std::vector<User> Users;

  AppListModel();
  ~AppListModel();

  void AddObserver(AppListModelObserver* observer);
  void RemoveObserver(AppListModelObserver* observer);

  void SetStatus(Status status);
  void SetUsers(const Users& profile_menu_items);
  void SetSignedIn(bool signed_in);

  // Find item matching |id|. NOTE: Requires a linear search.
  AppListItemModel* FindItem(const std::string& id);

  // Adds |item| to |apps_|. Takes ownership of |item|. Uses item->SortOrder()
  // to insert in the correct place. TODO(stevenjb): Use synced app list order
  // instead of SortOrder when available: crbug.com/305024.
  void AddItem(AppListItemModel* item);

  // Finds |item| in |apps_| and deletes it.
  void DeleteItem(const std::string& id);

  Apps* apps() { return apps_.get(); }
  SearchBoxModel* search_box() { return search_box_.get(); }
  SearchResults* results() { return results_.get(); }
  Status status() const { return status_; }
  bool signed_in() const { return signed_in_; }

  const Users& users() const {
    return users_;
  }

 private:
  scoped_ptr<Apps> apps_;

  scoped_ptr<SearchBoxModel> search_box_;
  scoped_ptr<SearchResults> results_;

  Users users_;

  bool signed_in_;
  Status status_;
  ObserverList<AppListModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_MODEL_H_
