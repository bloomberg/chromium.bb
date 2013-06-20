// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
}

namespace app_list {

class AppListItemModel;
class AppListModel;
class SearchResult;
class SigninDelegate;

class APP_LIST_EXPORT AppListViewDelegate {
 public:
  // AppListView owns the delegate.
  virtual ~AppListViewDelegate() {}

  // Invoked to set the model that AppListView uses.
  // Note that AppListView owns the model.
  virtual void SetModel(AppListModel* model) = 0;

  // Gets the SigninDelegate for the app list. Owned by the AppListViewDelegate.
  virtual SigninDelegate* GetSigninDelegate() = 0;

  // Gets a path to a shortcut for the given app. Returns asynchronously as the
  // shortcut may not exist yet.
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) = 0;

  // Invoked when an AppListeItemModelView is activated by click or enter key.
  virtual void ActivateAppListItem(AppListItemModel* item,
                                   int event_flags) = 0;

  // Invoked to start a new search. Delegate collects query input from
  // SearchBoxModel and populates SearchResults. Both models are sub models
  // of AppListModel.
  virtual void StartSearch() = 0;

  // Invoked to stop the current search.
  virtual void StopSearch() = 0;

  // Invoked to open the search result.
  virtual void OpenSearchResult(SearchResult* result, int event_flags) = 0;

  // Called to invoke a custom action on |result|.  |action_index| corresponds
  // to the index of an icon in |result.action_icons()|.
  virtual void InvokeSearchResultAction(SearchResult* result,
                                        int action_index,
                                        int event_flags) = 0;

  // Invoked to dismiss app list. This may leave the view open but hidden from
  // the user.
  virtual void Dismiss() = 0;

  // Invoked when the app list is closing.
  virtual void ViewClosing() = 0;

  // Invoked when the app list's activated state changes.
  virtual void ViewActivationChanged(bool active) = 0;

  // Returns the icon to be displayed in the window and taskbar.
  virtual gfx::ImageSkia GetWindowIcon() = 0;

  // Returns the name of the current user.
  virtual base::string16 GetCurrentUserName() = 0;

  // Returns the email of the current user.
  virtual base::string16 GetCurrentUserEmail() = 0;

  // Open the settings UI.
  virtual void OpenSettings() = 0;

  // Open the help UI.
  virtual void OpenHelp() = 0;

  // Open the feedback UI.
  virtual void OpenFeedback() = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
