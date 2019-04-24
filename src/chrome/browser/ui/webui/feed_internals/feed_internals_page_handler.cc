// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feed_internals_page_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/android/feed/feed_lifecycle_bridge.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/content/feed_offline_host.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/user_classifier.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"
#include "components/prefs/pref_service.h"

namespace {

feed_internals::mojom::TimePtr ToMojoTime(base::Time time) {
  return time.is_null() ? nullptr
                        : feed_internals::mojom::Time::New(time.ToJsTime());
}

}  // namespace

FeedInternalsPageHandler::FeedInternalsPageHandler(
    feed_internals::mojom::PageHandlerRequest request,
    feed::FeedHostService* feed_host_service,
    PrefService* pref_service)
    : binding_(this, std::move(request)),
      feed_scheduler_host_(feed_host_service->GetSchedulerHost()),
      feed_offline_host_(feed_host_service->GetOfflineHost()),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {}

FeedInternalsPageHandler::~FeedInternalsPageHandler() = default;

void FeedInternalsPageHandler::GetGeneralProperties(
    GetGeneralPropertiesCallback callback) {
  auto properties = feed_internals::mojom::Properties::New();

  properties->is_feed_enabled =
      base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions);

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::GetUserClassifierProperties(
    GetUserClassifierPropertiesCallback callback) {
  auto properties = feed_internals::mojom::UserClassifier::New();

  feed::UserClassifier* user_classifier =
      feed_scheduler_host_->GetUserClassifierForDebugging();

  properties->user_class_description =
      user_classifier->GetUserClassDescriptionForDebugging();
  properties->avg_hours_between_views = user_classifier->GetEstimatedAvgTime(
      feed::UserClassifier::Event::kSuggestionsViewed);
  properties->avg_hours_between_uses = user_classifier->GetEstimatedAvgTime(
      feed::UserClassifier::Event::kSuggestionsUsed);

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::GetLastFetchProperties(
    GetLastFetchPropertiesCallback callback) {
  auto properties = feed_internals::mojom::LastFetchProperties::New();

  properties->last_fetch_status =
      feed_scheduler_host_->GetLastFetchStatusForDebugging();
  properties->last_fetch_time =
      ToMojoTime(pref_service_->GetTime(feed::prefs::kLastFetchAttemptTime));
  properties->refresh_suppress_time =
      ToMojoTime(feed_scheduler_host_->GetSuppressRefreshesUntilForDebugging());

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::ClearUserClassifierProperties() {
  feed_scheduler_host_->GetUserClassifierForDebugging()
      ->ClearClassificationForDebugging();
}

void FeedInternalsPageHandler::ClearCachedDataAndRefreshFeed() {
  feed::FeedLifecycleBridge::ClearCachedData();
}

void FeedInternalsPageHandler::GetCurrentContent(
    GetCurrentContentCallback callback) {
  feed_offline_host_->GetCurrentArticleSuggestions(base::BindOnce(
      &FeedInternalsPageHandler::OnGetCurrentArticleSuggestionsDone,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedInternalsPageHandler::OnGetCurrentArticleSuggestionsDone(
    GetCurrentContentCallback callback,
    std::vector<offline_pages::PrefetchSuggestion> results) {
  std::vector<feed_internals::mojom::SuggestionPtr> suggestions;

  for (offline_pages::PrefetchSuggestion result : results) {
    auto suggestion = feed_internals::mojom::Suggestion::New();
    suggestion->title = std::move(result.article_title);
    suggestion->url = result.article_url.spec();
    suggestion->publisher_name = std::move(result.article_attribution);
    suggestion->image_url = result.thumbnail_url.spec();
    suggestion->favicon_url = result.favicon_url.spec();

    suggestions.push_back(std::move(suggestion));
  }

  std::move(callback).Run(std::move(suggestions));
}
