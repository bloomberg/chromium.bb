// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/app_list/app_list_export.h"

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
class Size;
}

#if defined(TOOLKIT_VIEWS)
namespace views {
class View;
}
#endif

namespace app_list {

class AppListModel;
class AppListViewDelegateObserver;
class SearchResult;
class SpeechUIModel;

class APP_LIST_EXPORT AppListViewDelegate {
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
  typedef std::vector<User> Users;

  // AppListView owns the delegate.
  virtual ~AppListViewDelegate() {}

  // Whether to force the use of a native desktop widget when the app list
  // window is first created.
  virtual bool ForceNativeDesktop() const = 0;

  // Sets the delegate to use the profile at |profile_path|. This is currently
  // only used by non-Ash Windows.
  virtual void SetProfileByPath(const base::FilePath& profile_path) = 0;

  // Gets the model associated with the view delegate. The model may be owned
  // by the delegate, or owned elsewhere (e.g. a profile keyed service).
  virtual AppListModel* GetModel() = 0;

  // Gets the SpeechUIModel for the app list. Owned by the AppListViewDelegate.
  virtual SpeechUIModel* GetSpeechUI() = 0;

  // Gets a path to a shortcut for the given app. Returns asynchronously as the
  // shortcut may not exist yet.
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) = 0;

  // Invoked to start a new search. Delegate collects query input from
  // SearchBoxModel and populates SearchResults. Both models are sub models
  // of AppListModel.
  virtual void StartSearch() = 0;

  // Invoked to stop the current search.
  virtual void StopSearch() = 0;

  // Invoked to open the search result.
  virtual void OpenSearchResult(SearchResult* result,
                                bool auto_launch,
                                int event_flags) = 0;

  // Called to invoke a custom action on |result|.  |action_index| corresponds
  // to the index of an icon in |result.action_icons()|.
  virtual void InvokeSearchResultAction(SearchResult* result,
                                        int action_index,
                                        int event_flags) = 0;

  // Gets the timeout for auto-launching the first search result, or 0 if the
  //  auto-launch should not happen for the current search session.
  virtual base::TimeDelta GetAutoLaunchTimeout() = 0;

  // Invoked when the auto-launch is canceled by the user action.
  virtual void AutoLaunchCanceled() = 0;

  // Invoked when the app list UI is created.
  virtual void ViewInitialized() = 0;

  // Invoked to dismiss app list. This may leave the view open but hidden from
  // the user.
  virtual void Dismiss() = 0;

  // Invoked when the app list is closing.
  virtual void ViewClosing() = 0;

  // Returns the icon to be displayed in the window and taskbar.
  virtual gfx::ImageSkia GetWindowIcon() = 0;

  // Open the settings UI.
  virtual void OpenSettings() = 0;

  // Open the help UI.
  virtual void OpenHelp() = 0;

  // Open the feedback UI.
  virtual void OpenFeedback() = 0;

  // Invoked to toggle the status of speech recognition.
  virtual void ToggleSpeechRecognition() = 0;

  // Shows the app list for the profile specified by |profile_path|.
  virtual void ShowForProfileByPath(const base::FilePath& profile_path) = 0;

#if defined(TOOLKIT_VIEWS)
  // Creates the web view for the start page. The caller takes the ownership of
  // the returned view.
  virtual views::View* CreateStartPageWebView(const gfx::Size& size) = 0;

  // Creates the web view for the user-specified custom page. May return NULL.
  // The caller takes ownership of the returned view.
  virtual views::View* CreateCustomPageWebView(const gfx::Size& size) = 0;
#endif

  // Returns true if the delegate supports speech recognition.
  virtual bool IsSpeechRecognitionEnabled() = 0;

  // Returns the list of users (for AppListMenu).
  virtual const Users& GetUsers() const = 0;

  // Returns true if the app list should be centered and in landscape mode.
  virtual bool ShouldCenterWindow() const = 0;

  // Adds/removes an observer for profile changes.
  virtual void AddObserver(AppListViewDelegateObserver* observer) {}
  virtual void RemoveObserver(AppListViewDelegateObserver* observer) {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
