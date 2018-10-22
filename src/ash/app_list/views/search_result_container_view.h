// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_

#include <stddef.h>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace app_list {

class SearchResultBaseView;

// SearchResultContainerView is a base class for views that contain multiple
// search results. SearchPageView holds these in a list and manages which one is
// selected. There can be one result within one SearchResultContainerView
// selected at a time; moving off the end of one container view selects the
// first element of the next container view, and vice versa
class APP_LIST_EXPORT SearchResultContainerView : public views::View,
                                                  public ui::ListModelObserver {
 public:
  class Delegate {
   public:
    virtual void OnSearchResultContainerResultsChanged() = 0;
  };
  SearchResultContainerView();
  ~SearchResultContainerView() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Sets the search results to listen to.
  void SetResults(SearchModel::SearchResults* results);
  SearchModel::SearchResults* results() { return results_; }

  int num_results() const { return num_results_; }

  void set_container_score(double score) { container_score_ = score; }
  double container_score() const { return container_score_; }

  // Updates the distance_from_origin() properties of the results in this
  // container. |y_index| is the absolute y-index of the first result of this
  // container (counting from the top of the app list).
  virtual void NotifyFirstResultYIndex(int y_index);

  // Gets the number of down keystrokes from the beginning to the end of this
  // container.
  virtual int GetYSize();

  // Batching method that actually performs the update and updates layout.
  void Update();

  // Returns whether an update is currently scheduled for this container.
  bool UpdateScheduled();

  // Overridden from views::View:
  const char* GetClassName() const override;

  // Overridden from ui::ListModelObserver:
  void ListItemsAdded(size_t start, size_t count) override;
  void ListItemsRemoved(size_t start, size_t count) override;
  void ListItemMoved(size_t index, size_t target_index) override;
  void ListItemsChanged(size_t start, size_t count) override;

  // Returns the first result in the container view. Returns NULL if it does not
  // exist.
  virtual SearchResultBaseView* GetFirstResultView();

 private:
  // Schedules an Update call using |update_factory_|. Do nothing if there is a
  // pending call.
  void ScheduleUpdate();

  // Updates UI with model. Returns the number of visible results.
  virtual int DoUpdate() = 0;

  Delegate* delegate_ = nullptr;

  int num_results_ = 0;

  double container_score_;

  SearchModel::SearchResults* results_ = nullptr;  // Owned by SearchModel.

  // The factory that consolidates multiple Update calls into one.
  base::WeakPtrFactory<SearchResultContainerView> update_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchResultContainerView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_CONTAINER_VIEW_H_
