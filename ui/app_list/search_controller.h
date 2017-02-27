// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SEARCH_CONTROLLER_H_
#define UI_APP_LIST_SEARCH_CONTROLLER_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search/mixer.h"
#include "ui/app_list/speech_ui_model_observer.h"

namespace app_list {

class History;
class SearchBoxModel;
class SearchProvider;
class SearchResult;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
class APP_LIST_EXPORT SearchController {
 public:
  SearchController(SearchBoxModel* search_box,
                   AppListModel::SearchResults* results,
                   History* history);
  virtual ~SearchController();

  void Start(bool is_voice_query);
  void Stop();

  void OpenResult(SearchResult* result, int event_flags);
  void InvokeResultAction(SearchResult* result,
                          int action_index,
                          int event_flags);

  // Adds a new mixer group. See Mixer::AddGroup.
  size_t AddGroup(size_t max_results, double multiplier);

  // Takes ownership of |provider| and associates it with given mixer group.
  void AddProvider(size_t group_id, std::unique_ptr<SearchProvider> provider);

 private:
  // Invoked when the search results are changed.
  void OnResultsChanged();

  SearchBoxModel* search_box_;

  bool dispatching_query_ = false;

  // If true, the search results are shown on the launcher start page.
  bool query_for_recommendation_ = false;

  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  std::unique_ptr<Mixer> mixer_;
  History* history_;  // KeyedService, not owned.

  bool is_voice_query_ = false;

  base::OneShotTimer stop_timer_;

  DISALLOW_COPY_AND_ASSIGN(SearchController);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SEARCH_CONTROLLER_H_
