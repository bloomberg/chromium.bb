// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/app_list/app_list_model.h"
#include "ui/views/view.h"

namespace app_list {

// SearchResultContainerView is a base class that batches updates from a
// ListModelObserver.
class APP_LIST_EXPORT SearchResultContainerView : public views::View,
                                                  public ui::ListModelObserver {
 public:
  SearchResultContainerView();
  ~SearchResultContainerView() override;

  void SetResults(AppListModel::SearchResults* results);

  AppListModel::SearchResults* results() { return results_; }

  // Schedules an Update call using |update_factory_|. Do nothing if there is a
  // pending call.
  void ScheduleUpdate();

  // Overridden from ui::ListModelObserver:
  void ListItemsAdded(size_t start, size_t count) override;
  void ListItemsRemoved(size_t start, size_t count) override;
  void ListItemMoved(size_t index, size_t target_index) override;
  void ListItemsChanged(size_t start, size_t count) override;

  // Updates UI with model.
  virtual void Update() = 0;

 private:
  void DoUpdate();

  AppListModel::SearchResults* results_;  // Owned by AppListModel.

  // The factory that consolidates multiple Update calls into one.
  base::WeakPtrFactory<SearchResultContainerView> update_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultContainerView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
