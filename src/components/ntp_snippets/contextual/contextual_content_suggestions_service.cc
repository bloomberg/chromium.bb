// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service_proxy.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_result.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"

namespace contextual_suggestions {

using ntp_snippets::ContentSuggestion;
using ntp_snippets::RemoteSuggestionsDatabase;

namespace {
bool IsEligibleURL(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() && !url.HostIsIPAddress();
}

}  // namespace

ContextualContentSuggestionsService::ContextualContentSuggestionsService(
    std::unique_ptr<ContextualSuggestionsFetcher>
        contextual_suggestions_fetcher,
    std::unique_ptr<ContextualSuggestionsReporterProvider> reporter_provider)
    : fetch_cache_(kFetchCacheCapacity),
      contextual_suggestions_fetcher_(
          std::move(contextual_suggestions_fetcher)),
      reporter_provider_(std::move(reporter_provider)) {}

ContextualContentSuggestionsService::~ContextualContentSuggestionsService() =
    default;

void ContextualContentSuggestionsService::FetchContextualSuggestionClusters(
    const GURL& url,
    FetchClustersCallback callback,
    ReportFetchMetricsCallback metrics_callback) {
  // TODO(pnoland): Also check that the url is safe.
  ContextualSuggestionsResult result;
  if (IsEligibleURL(url) && !fetch_cache_.GetSuggestionsResult(url, &result)) {
    FetchClustersCallback internal_callback = base::BindOnce(
        &ContextualContentSuggestionsService::FetchDone, base::Unretained(this),
        url, std::move(callback), metrics_callback);
    contextual_suggestions_fetcher_->FetchContextualSuggestionsClusters(
        url, std::move(internal_callback), metrics_callback);
  } else if (result.peek_conditions.confidence < GetMinimumConfidence()) {
    BelowConfidenceThresholdFetchDone(std::move(callback), metrics_callback);
  } else {
    std::move(callback).Run(result);
  }
}

void ContextualContentSuggestionsService::FetchDone(
    const GURL& url,
    FetchClustersCallback callback,
    ReportFetchMetricsCallback metrics_callback,
    ContextualSuggestionsResult result) {
  // We still want to cache low confidence results so that we avoid doing
  // unnecessary fetches.
  if (result.clusters.size() > 0) {
    fetch_cache_.AddSuggestionsResult(url, result);
  }

  if (result.peek_conditions.confidence < GetMinimumConfidence()) {
    BelowConfidenceThresholdFetchDone(std::move(callback), metrics_callback);
    return;
  }

  std::move(callback).Run(result);
}

ContextualSuggestionsDebuggingReporter*
ContextualContentSuggestionsService::GetDebuggingReporter() {
  return reporter_provider_->GetDebuggingReporter();
}

base::flat_map<GURL, ContextualSuggestionsResult>
ContextualContentSuggestionsService::GetAllCachedResultsForDebugging() {
  return fetch_cache_.GetAllCachedResultsForDebugging();
}

void ContextualContentSuggestionsService::ClearCachedResultsForDebugging() {
  fetch_cache_.Clear();
}

std::unique_ptr<
    contextual_suggestions::ContextualContentSuggestionsServiceProxy>
ContextualContentSuggestionsService::CreateProxy() {
  return std::make_unique<
      contextual_suggestions::ContextualContentSuggestionsServiceProxy>(
      this, reporter_provider_->CreateReporter());
}

void ContextualContentSuggestionsService::BelowConfidenceThresholdFetchDone(
    FetchClustersCallback callback,
    ReportFetchMetricsCallback metrics_callback) {
  metrics_callback.Run(contextual_suggestions::FETCH_BELOW_THRESHOLD);
  std::move(callback).Run(ContextualSuggestionsResult("", {}, PeekConditions(),
                                                      ServerExperimentInfos()));
}

}  // namespace contextual_suggestions
