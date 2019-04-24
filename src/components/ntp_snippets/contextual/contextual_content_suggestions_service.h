// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_REPORTING_CONTEXTUAL_CONTENT_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_REPORTING_CONTEXTUAL_CONTENT_SUGGESTIONS_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_cache.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_fetcher.h"
#include "components/ntp_snippets/contextual/reporting/contextual_suggestions_debugging_reporter.h"
#include "components/ntp_snippets/contextual/reporting/contextual_suggestions_reporter.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace contextual_suggestions {

static constexpr int kFetchCacheCapacity = 10;

class ContextualContentSuggestionsServiceProxy;

// Retrieves contextual suggestions for a given URL using caching.
class ContextualContentSuggestionsService : public KeyedService {
 public:
  ContextualContentSuggestionsService(
      std::unique_ptr<ContextualSuggestionsFetcher>
          contextual_suggestions_fetcher,
      std::unique_ptr<
          contextual_suggestions::ContextualSuggestionsReporterProvider>
          reporter_provider);
  ~ContextualContentSuggestionsService() override;

  // Asynchronously fetches contextual suggestions for the given URL.
  virtual void FetchContextualSuggestionClusters(
      const GURL& url,
      FetchClustersCallback callback,
      ReportFetchMetricsCallback metrics_callback);

  void FetchDone(const GURL& url,
                 FetchClustersCallback callback,
                 ReportFetchMetricsCallback metrics_callback,
                 ContextualSuggestionsResult result);

  // Used to surface metrics events via chrome://eoc-internals.
  ContextualSuggestionsDebuggingReporter* GetDebuggingReporter();

  // Expose cached results for debugging.
  base::flat_map<GURL, ContextualSuggestionsResult>
  GetAllCachedResultsForDebugging();

  // Clear the cached results for debugging.
  void ClearCachedResultsForDebugging();

  std::unique_ptr<ContextualContentSuggestionsServiceProxy> CreateProxy();

 private:
  void BelowConfidenceThresholdFetchDone(
      FetchClustersCallback callback,
      ReportFetchMetricsCallback metrics_callback);

  // Cache of contextual suggestions fetch results, keyed by the context url.
  ContextualSuggestionsCache fetch_cache_;

  // Performs actual network request to fetch contextual suggestions.
  std::unique_ptr<ContextualSuggestionsFetcher> contextual_suggestions_fetcher_;

  std::unique_ptr<contextual_suggestions::ContextualSuggestionsReporterProvider>
      reporter_provider_;

  DISALLOW_COPY_AND_ASSIGN(ContextualContentSuggestionsService);
};

}  // namespace contextual_suggestions

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_REPORTING_CONTEXTUAL_CONTENT_SUGGESTIONS_SERVICE_H_
