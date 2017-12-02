// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_controller.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/search/history.h"
#include "ui/app_list/search_provider.h"

namespace app_list {

SearchController::SearchController(SearchBoxModel* search_box,
                                   SearchModel::SearchResults* results,
                                   History* history)
    : search_box_(search_box), mixer_(new Mixer(results)), history_(history) {}

SearchController::~SearchController() {
}

void SearchController::Start() {
  base::string16 query;
  base::TrimWhitespace(search_box_->text(), base::TRIM_ALL, &query);

  is_voice_query_ = search_box_->is_voice_query();

  dispatching_query_ = true;
  for (const auto& provider : providers_)
    provider->Start(is_voice_query_, query);

  dispatching_query_ = false;
  query_for_recommendation_ = query.empty();

  OnResultsChanged();
}

void SearchController::OpenResult(SearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  UMA_HISTOGRAM_ENUMERATION(kSearchResultOpenDisplayTypeHistogram,
                            result->display_type(),
                            SearchResult::DISPLAY_TYPE_LAST);

  // Record the search metric if the SearchResult is not a suggested app.
  if (result->display_type() != SearchResult::DISPLAY_RECOMMENDATION) {
    // Count AppList.Search here because it is composed of search + action.
    base::RecordAction(base::UserMetricsAction("AppList_Search"));

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
                                  double multiplier,
                                  double boost) {
  return mixer_->AddGroup(max_results, multiplier, boost);
}

void SearchController::AddProvider(size_t group_id,
                                   std::unique_ptr<SearchProvider> provider) {
  provider->set_result_changed_callback(
      base::Bind(&SearchController::OnResultsChanged, base::Unretained(this)));
  mixer_->AddProviderToGroup(group_id, provider.get());
  providers_.emplace_back(std::move(provider));
}

void SearchController::OnResultsChanged() {
  if (dispatching_query_)
    return;

  KnownResults known_results;
  if (history_ && history_->IsReady()) {
    history_->GetKnownResults(base::UTF16ToUTF8(search_box_->text()))
        ->swap(known_results);
  }

  size_t num_max_results =
      query_for_recommendation_ ? kNumStartPageTiles : kMaxSearchResults;
  mixer_->MixAndPublish(is_voice_query_, known_results, num_max_results);
}

}  // namespace app_list
