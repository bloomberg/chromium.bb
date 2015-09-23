// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/search/history.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"

namespace {

// Maximum time (in milliseconds) to wait to the search providers to finish.
const int kStopTimeMS = 1500;
}

namespace app_list {

SearchController::SearchController(SearchBoxModel* search_box,
                                   AppListModel::SearchResults* results,
                                   History* history)
    : search_box_(search_box),
      dispatching_query_(false),
      mixer_(new Mixer(results)),
      history_(history),
      is_voice_query_(false) {
}

SearchController::~SearchController() {
}

void SearchController::Start(bool is_voice_query) {
  Stop();

  base::string16 query;
  base::TrimWhitespace(search_box_->text(), base::TRIM_ALL, &query);

  dispatching_query_ = true;
  for (Providers::iterator it = providers_.begin();
       it != providers_.end();
       ++it) {
    (*it)->Start(is_voice_query, query);
  }
  dispatching_query_ = false;

  is_voice_query_ = is_voice_query;

  OnResultsChanged();

  stop_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kStopTimeMS),
                    base::Bind(&SearchController::Stop,
                               base::Unretained(this)));
}

void SearchController::Stop() {
  stop_timer_.Stop();

  for (Providers::iterator it = providers_.begin();
       it != providers_.end();
       ++it) {
    (*it)->Stop();
  }
}

void SearchController::OpenResult(SearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  // Count AppList.Search here because it is composed of search + action.
  base::RecordAction(base::UserMetricsAction("AppList_Search"));

  UMA_HISTOGRAM_ENUMERATION(kSearchResultOpenDisplayTypeHistogram,
                            result->display_type(),
                            SearchResult::DISPLAY_TYPE_LAST);

  if (result->display_type() != SearchResult::DISPLAY_RECOMMENDATION) {
    UMA_HISTOGRAM_COUNTS_100(kSearchQueryLength, search_box_->text().size());

    if (result->distance_from_origin() >= 0) {
      UMA_HISTOGRAM_COUNTS_100(kSearchResultDistanceFromOrigin,
                               result->distance_from_origin());
    }
  }

  result->Open(event_flags);

  if (history_ && history_->IsReady()) {
    history_->AddLaunchEvent(base::UTF16ToUTF8(search_box_->text()),
                             result->id());
  }
}

void SearchController::InvokeResultAction(SearchResult* result,
                                          int action_index,
                                          int event_flags) {
  // TODO(xiyuan): Hook up with user learning.
  result->InvokeAction(action_index, event_flags);
}

size_t SearchController::AddGroup(size_t max_results,
                                  double boost,
                                  double multiplier) {
  return mixer_->AddGroup(max_results, boost, multiplier);
}

size_t SearchController::AddOmniboxGroup(size_t max_results,
                                         double boost,
                                         double multiplier) {
  return mixer_->AddOmniboxGroup(max_results, boost, multiplier);
}

void SearchController::AddProvider(size_t group_id,
                                   scoped_ptr<SearchProvider> provider) {
  provider->set_result_changed_callback(base::Bind(
      &SearchController::OnResultsChanged,
      base::Unretained(this)));
  mixer_->AddProviderToGroup(group_id, provider.get());
  providers_.push_back(provider.Pass());
}

void SearchController::OnResultsChanged() {
  if (dispatching_query_)
    return;

  KnownResults known_results;
  if (history_ && history_->IsReady()) {
    history_->GetKnownResults(base::UTF16ToUTF8(search_box_->text()))
        ->swap(known_results);
  }

  mixer_->MixAndPublish(is_voice_query_, known_results);
}

}  // namespace app_list
