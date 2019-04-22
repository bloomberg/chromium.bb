// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"

namespace app_list {

SearchController::SearchController(AppListModelUpdater* model_updater,
                                   AppListControllerDelegate* list_controller,
                                   Profile* profile)
    : mixer_(std::make_unique<Mixer>(model_updater)),
      ranker_(std::make_unique<AppSearchResultRanker>(
          profile->GetPath(),
          chromeos::ProfileHelper::IsEphemeralUserProfile(profile))),
      list_controller_(list_controller) {}

SearchController::~SearchController() {}

void SearchController::Start(const base::string16& query) {
  dispatching_query_ = true;
  for (const auto& provider : providers_)
    provider->Start(query);

  dispatching_query_ = false;
  query_for_recommendation_ = query.empty();

  OnResultsChanged();
}

void SearchController::ViewClosing() {
  for (const auto& provider : providers_)
    provider->ViewClosing();
}

void SearchController::OpenResult(ChromeSearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  result->Open(event_flags);

  // Launching apps can take some time. It looks nicer to dismiss the app list.
  // Do not close app list for home launcher.
  if (!TabletModeClient::Get() ||
      !TabletModeClient::Get()->tablet_mode_enabled()) {
    list_controller_->DismissView();
  }
}

void SearchController::InvokeResultAction(ChromeSearchResult* result,
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

  size_t num_max_results =
      query_for_recommendation_
          ? AppListConfig::instance().num_start_page_tiles()
          : AppListConfig::instance().max_search_results();
  mixer_->MixAndPublish(num_max_results);
}

ChromeSearchResult* SearchController::FindSearchResult(
    const std::string& result_id) {
  for (const auto& provider : providers_) {
    for (const auto& result : provider->results()) {
      if (result->id() == result_id)
        return result.get();
    }
  }
  return nullptr;
}

ChromeSearchResult* SearchController::GetResultByTitleForTest(
    const std::string& title) {
  base::string16 target_title = base::ASCIIToUTF16(title);
  for (const auto& provider : providers_) {
    for (const auto& result : provider->results()) {
      if (result->title() == target_title &&
          result->result_type() == ash::SearchResultType::kInstalledApp &&
          result->display_type() !=
              ash::SearchResultDisplayType::kRecommendation) {
        return result.get();
      }
    }
  }
  return nullptr;
}

void SearchController::SetRecurrenceRanker(
    std::unique_ptr<RecurrenceRanker> ranker) {
  mixer_->SetRecurrenceRanker(std::move(ranker));
}

void SearchController::Train(const std::string& id, RankingItemType type) {
  for (const auto& provider : providers_)
    provider->Train(id, type);
  mixer_->Train(id, type);
}

AppSearchResultRanker* SearchController::GetSearchResultRanker() {
  return ranker_.get();
}

}  // namespace app_list
