// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_export.h"

namespace gfx {
class Size;
}

namespace views {
class View;
}

namespace ui {
class MenuModel;
}

namespace app_list {

class AppListModel;
class AppListViewDelegateObserver;
class SearchModel;
class SearchResult;
class SpeechUIModel;

class APP_LIST_EXPORT AppListViewDelegate {
 public:
  virtual ~AppListViewDelegate() {}
  // Gets the model associated with the view delegate. The model may be owned
  // by the delegate, or owned elsewhere (e.g. a profile keyed service).
  // Note: Don't call this method under //chrome/browser/.
  virtual AppListModel* GetModel() = 0;

  // Gets the search model associated with the view delegate. The model may be
  // owned by the delegate, or owned elsewhere (e.g. a profile keyed service).
  // Note: Don't call this method under //chrome/browser/.
  virtual SearchModel* GetSearchModel() = 0;

  // Gets the SpeechUIModel for the app list. Owned by the AppListViewDelegate.
  virtual SpeechUIModel* GetSpeechUI() = 0;

  // Invoked to start a new search. This collects a list of search results
  // matching the raw query, which is an unhandled string typed into the search
  // box by the user.
  virtual void StartSearch(const base::string16& raw_query) = 0;

  // Invoked to open the search result.
  virtual void OpenSearchResult(SearchResult* result,
                                int event_flags) = 0;

  // Called to invoke a custom action on |result|.  |action_index| corresponds
  // to the index of an icon in |result.action_icons()|.
  virtual void InvokeSearchResultAction(SearchResult* result,
                                        int action_index,
                                        int event_flags) = 0;

  // Invoked when the app list UI is created.
  virtual void ViewInitialized() = 0;

  // Invoked to dismiss app list. This may leave the view open but hidden from
  // the user.
  virtual void Dismiss() = 0;

  // Invoked when the app list is closing.
  virtual void ViewClosing() = 0;

  // Invoked to toggle the status of speech recognition.
  virtual void StartSpeechRecognition() = 0;
  virtual void StopSpeechRecognition() = 0;

  // Creates the web view for the start page. The caller takes the ownership of
  // the returned view.
  virtual views::View* CreateStartPageWebView(const gfx::Size& size) = 0;

  // Returns true if the delegate supports speech recognition.
  virtual bool IsSpeechRecognitionEnabled() = 0;

  // Gets the wallpaper prominent colors.
  virtual void GetWallpaperProminentColors(std::vector<SkColor>* colors) = 0;

  // Activates (opens) the item.
  virtual void ActivateItem(const std::string& id, int event_flags) = 0;

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install).
  // Note the returned menu model is owned by this item.
  virtual ui::MenuModel* GetContextMenuModel(const std::string& id) = 0;

  // Add/remove observer for AppListViewDelegate.
  virtual void AddObserver(AppListViewDelegateObserver* observer) = 0;
  virtual void RemoveObserver(AppListViewDelegateObserver* observer) = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
