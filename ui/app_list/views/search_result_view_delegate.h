// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_DELEGATE_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_DELEGATE_H_

namespace app_list {

class SearchResultView;

class SearchResultViewDelegate {
 public:
  // Called when the search result is activated.
  virtual void SearchResultActivated(SearchResultView* view,
                                     int event_flags) = 0;

  // Called when one of the search result's optional action icons is activated.
  // |action_index| contains the 0-based index of the action.
  virtual void SearchResultActionActivated(SearchResultView* view,
                                           size_t action_index,
                                           int event_flags) = 0;

  // Called when the app represented by the search result is installed.
  virtual void OnSearchResultInstalled(SearchResultView* view) = 0;

  // Called when the app represented by the search result is uninstalled.
  virtual void OnSearchResultUninstalled(SearchResultView* view) = 0;

 protected:
  virtual ~SearchResultViewDelegate() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_DELEGATE_H_
