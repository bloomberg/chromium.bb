// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_buildflags.h"
#include "components/history_clusters/core/history_clusters_db_tasks.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/history_clusters/core/remote_clustering_backend.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/time_format.h"

#if BUILDFLAG(BUILD_WITH_ON_DEVICE_CLUSTERING_BACKEND)
#include "components/history_clusters/internal/on_device_clustering_backend.h"
#endif

namespace history_clusters {

namespace {

// Returns true if `find_nodes` matches `cluster`.
// This is deliberately meant to closely mirror the History implementation..
// TODO(tommycli): Merge with `URLDatabase::GetTextMatchesWithAlgorithm()`.
bool DoesQueryMatchCluster(const query_parser::QueryNodeVector& find_nodes,
                           const history::Cluster& cluster) {
  query_parser::QueryWordVector find_in_words;

  // All of the cluster's `keyword`s go into `find_in_words`.
  // Each `keyword` may have multiple terms, so loop over them.
  for (auto& keyword : cluster.keywords) {
    query_parser::QueryParser::ExtractQueryWords(base::i18n::ToLower(keyword),
                                                 &find_in_words);
  }

  // Also extract all of the visits' URLs and titles into `find_in_words`.
  for (const auto& visit : cluster.visits) {
    GURL gurl = visit.annotated_visit.url_row.url();

    std::u16string url_lower =
        base::i18n::ToLower(base::UTF8ToUTF16(gurl.possibly_invalid_spec()));
    query_parser::QueryParser::ExtractQueryWords(url_lower, &find_in_words);

    if (gurl.is_valid()) {
      // Decode punycode to match IDN.
      std::u16string ascii = base::ASCIIToUTF16(gurl.host());
      std::u16string utf = url_formatter::IDNToUnicode(gurl.host());
      if (ascii != utf)
        query_parser::QueryParser::ExtractQueryWords(utf, &find_in_words);
    }

    std::u16string title_lower =
        base::i18n::ToLower(visit.annotated_visit.url_row.title());
    query_parser::QueryParser::ExtractQueryWords(title_lower, &find_in_words);
  }

  return query_parser::QueryParser::DoesQueryMatch(find_in_words, find_nodes);
}

// Filter `clusters` matching `query`. There are additional filters (e.g.
// `max_time`) used when requesting `QueryClusters()`, but this function is only
// responsible for matching `query`.
std::vector<history::Cluster> FilterClustersMatchingQuery(
    std::string query,
    const std::vector<history::Cluster>& clusters) {
  if (query.empty()) {
    // For the empty-query state, only show clusters with
    // `should_show_on_prominent_ui_surfaces` set to true. This restriction is
    // NOT applied when the user is searching for a specific keyword.
    std::vector<history::Cluster> shown_clusters;
    base::ranges::copy_if(clusters, std::back_inserter(shown_clusters),
                          [](const history::Cluster& cluster) {
                            return cluster.should_show_on_prominent_ui_surfaces;
                          });
    return shown_clusters;
  }

  // Extract query nodes from the query string.
  query_parser::QueryNodeVector find_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query),
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH, &find_nodes);

  std::vector<history::Cluster> matching_clusters;
  base::ranges::copy_if(clusters, std::back_inserter(matching_clusters),
                        [&find_nodes](const history::Cluster& cluster) {
                          return DoesQueryMatchCluster(find_nodes, cluster);
                        });

  return matching_clusters;
}

// Enforces the reverse-chronological invariant of clusters, as well the
// by-score sorting of visits within clusters.
std::vector<Cluster> SortClusters(std::vector<Cluster> clusters) {
  // Within each cluster, sort visits from best to worst using score.
  // TODO(tommycli): Once cluster persistence is done, maybe we can eliminate
  //  this sort step, if they are stored in-order.
  for (auto& cluster : clusters) {
    base::ranges::sort(cluster.visits, [](auto& v1, auto& v2) {
      if (v1.score != v2.score) {
        // Use v1 > v2 to get higher scored visits BEFORE lower scored visits.
        return v1.score > v2.score;
      }

      // Use v1 > v2 to get more recent visits BEFORE older visits.
      return v1.annotated_visit.visit_row.visit_time >
             v2.annotated_visit.visit_row.visit_time;
    });
  }

  // After that, sort clusters reverse-chronologically based on their highest
  // scored visit.
  base::ranges::sort(clusters, [&](auto& c1, auto& c2) {
    // TODO(tommycli): If we can establish an invariant that no backend will
    //  ever return an empty cluster, we can simplify the below code.
    base::Time c1_time;
    if (!c1.visits.empty()) {
      c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;
    }
    base::Time c2_time;
    if (!c1.visits.empty()) {
      c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;
    }

    // Use c1 > c2 to get more recent clusters BEFORE older clusters.
    return c1_time > c2_time;
  });

  return clusters;
}

// Gets a loggable JSON representation of `visits`.
std::string GetDebugJSONForVisits(
    const std::vector<history::AnnotatedVisit>& visits) {
  base::ListValue debug_visits_list;
  for (auto& visit : visits) {
    base::DictionaryValue debug_visit;
    debug_visit.SetIntKey("visitId", visit.visit_row.visit_id);
    debug_visit.SetStringKey("url", visit.url_row.url().spec());
    debug_visit.SetStringKey("title", visit.url_row.title());
    debug_visit.SetIntKey("foreground_time_secs",
                          visit.visit_row.visit_duration.InSeconds());
    debug_visit.SetIntKey(
        "navigationTimeMs",
        visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
    debug_visit.SetIntKey("pageEndReason",
                          visit.context_annotations.page_end_reason);
    debug_visit.SetIntKey("pageTransition",
                          static_cast<int>(visit.visit_row.transition));
    debug_visit.SetIntKey("referringVisitId",
                          visit.referring_visit_of_redirect_chain_start);
    debug_visit.SetIntKey("openerVisitId",
                          visit.opener_visit_of_redirect_chain_start);
    debug_visits_list.Append(std::move(debug_visit));
  }

  base::DictionaryValue debug_value;
  debug_value.SetKey("visits", std::move(debug_visits_list));
  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &debug_string)) {
    debug_string = "Error: Could not write visits to JSON.";
  }
  return debug_string;
}

// Gets a loggable JSON representation of `clusters`.
std::string GetDebugJSONForClusters(
    const std::vector<history::Cluster>& clusters) {
  // TODO(manukh): `ListValue` is deprecated; replace with `std::vector`.
  base::ListValue debug_clusters_list;
  for (const auto& cluster : clusters) {
    base::DictionaryValue debug_cluster;

    base::ListValue debug_keywords;
    for (const auto& keyword : cluster.keywords) {
      debug_keywords.Append(keyword);
    }
    debug_cluster.SetKey("keywords", std::move(debug_keywords));
    debug_cluster.SetBoolKey("should_show_on_prominent_ui_surfaces",
                             cluster.should_show_on_prominent_ui_surfaces);

    base::ListValue debug_visits;
    for (const auto& visit : cluster.visits) {
      base::DictionaryValue debug_visit;
      debug_visit.SetIntKey("visit_id",
                            visit.annotated_visit.visit_row.visit_id);
      debug_visit.SetDoubleKey("score", visit.score);
      base::ListValue debug_categories;
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        base::DictionaryValue debug_category;
        debug_category.SetStringKey("name", category.id);
        debug_category.SetIntKey("value", category.weight);
        debug_categories.Append(std::move(debug_category));
      }
      debug_visit.SetKey("categories", std::move(debug_categories));
      base::ListValue debug_entities;
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
        base::DictionaryValue debug_entity;
        debug_entity.SetStringKey("name", entity.id);
        debug_entity.SetIntKey("value", entity.weight);
        debug_entities.Append(std::move(debug_entity));
      }
      debug_visit.SetKey("entities", std::move(debug_entities));

      base::ListValue debug_duplicate_visits;
      for (const auto duplicate_visit : visit.duplicate_visit_ids) {
        debug_duplicate_visits.Append(int(duplicate_visit));
      }
      debug_visit.SetKey("duplicate_visits", std::move(debug_duplicate_visits));

      debug_visits.Append(std::move(debug_visit));
    }
    debug_cluster.SetKey("visits", std::move(debug_visits));

    debug_clusters_list.Append(std::move(debug_cluster));
  }

  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_clusters_list, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write clusters to JSON.";
  }
  return debug_string;
}

// TODO(tommycli): Explicitly link this number to what's in WebUI.
constexpr int kMaxCountForKeywordCacheBatch = 10;

}  // namespace

VisitDeletionObserver::VisitDeletionObserver(
    HistoryClustersService* history_clusters_service)
    : history_clusters_service_(history_clusters_service) {}

VisitDeletionObserver::~VisitDeletionObserver() = default;

void VisitDeletionObserver::AttachToHistoryService(
    history::HistoryService* history_service) {
  DCHECK(history_service);
  history_service_observation_.Observe(history_service);
}

void VisitDeletionObserver::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  history_clusters_service_->ClearKeywordCache();
}

HistoryClustersService::HistoryClustersService(
    history::HistoryService* history_service,
    TemplateURLService* template_url_service,
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : history_service_(history_service), visit_deletion_observer_(this) {
  DCHECK(history_service_);

  visit_deletion_observer_.AttachToHistoryService(history_service);

#if BUILDFLAG(BUILD_WITH_ON_DEVICE_CLUSTERING_BACKEND)
  if (kUseOnDeviceClusteringBackend.Get()) {
    backend_ = std::make_unique<OnDeviceClusteringBackend>(
        template_url_service, entity_metadata_provider);
  }
#endif

  if (!backend_ && RemoteModelEndpoint().is_valid() && url_loader_factory) {
    backend_ = std::make_unique<RemoteClusteringBackend>(
        url_loader_factory,
        base::BindRepeating(&HistoryClustersService::NotifyDebugMessage,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

HistoryClustersService::~HistoryClustersService() = default;

void HistoryClustersService::Shutdown() {}

void HistoryClustersService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void HistoryClustersService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void HistoryClustersService::NotifyDebugMessage(
    const std::string& message) const {
  for (Observer& obs : observers_) {
    obs.OnDebugMessage(message);
  }
}

IncompleteVisitContextAnnotations&
HistoryClustersService::GetIncompleteVisitContextAnnotations(int64_t nav_id) {
  DCHECK(HasIncompleteVisitContextAnnotations(nav_id));
  return GetOrCreateIncompleteVisitContextAnnotations(nav_id);
}

IncompleteVisitContextAnnotations&
HistoryClustersService::GetOrCreateIncompleteVisitContextAnnotations(
    int64_t nav_id) {
  return incomplete_visit_context_annotations_[nav_id];
}

bool HistoryClustersService::HasIncompleteVisitContextAnnotations(
    int64_t nav_id) {
  return incomplete_visit_context_annotations_.count(nav_id);
}

void HistoryClustersService::CompleteVisitContextAnnotationsIfReady(
    int64_t nav_id) {
  auto& visit_context_annotations =
      GetIncompleteVisitContextAnnotations(nav_id);
  DCHECK((visit_context_annotations.status.history_rows &&
          visit_context_annotations.status.navigation_ended) ||
         !visit_context_annotations.status.navigation_end_signals);
  DCHECK(visit_context_annotations.status.expect_ukm_page_end_signals ||
         !visit_context_annotations.status.ukm_page_end_signals);
  if (visit_context_annotations.status.history_rows &&
      visit_context_annotations.status.navigation_end_signals &&
      (visit_context_annotations.status.ukm_page_end_signals ||
       !visit_context_annotations.status.expect_ukm_page_end_signals)) {
    // If the main kMemories feature is enabled, we want to persist visits.
    // And if the persist-only switch is enabled, we also want to persist them.
    if (base::FeatureList::IsEnabled(kJourneys) ||
        base::FeatureList::IsEnabled(kPersistContextAnnotationsInHistoryDb)) {
      history_service_->AddContextAnnotationsForVisit(
          visit_context_annotations.visit_row.visit_id,
          visit_context_annotations.context_annotations);
    }
    incomplete_visit_context_annotations_.erase(nav_id);
  }
}

void HistoryClustersService::QueryClusters(
    const std::string& query,
    base::Time begin_time,
    base::Time end_time,
    const size_t max_count,
    QueryClustersCallback callback,
    base::CancelableTaskTracker* task_tracker) {
  NotifyDebugMessage("HistoryClustersService::QueryClusters()");
  NotifyDebugMessage("  end_time = " + (end_time.is_null()
                                            ? "null"
                                            : base::TimeToISO8601(end_time)));
  NotifyDebugMessage("  max_count = " + base::NumberToString(max_count));

  if (!backend_) {
    NotifyDebugMessage(
        "HistoryClustersService::QueryClusters Error: ClusteringBackend is "
        "nullptr. Returning empty cluster vector.");
    std::move(callback).Run({});
    return;
  }

  DCHECK(history_service_);

  size_t max_visit_count = kMaxVisitsToCluster.Get();
  if (max_count > 0) {
    // As a primitive heuristic, fetch 3x the amount of visits as requested
    // clusters. We don't know in advance how big the clusters will be.
    max_visit_count = max_count * 3;
  }

  NotifyDebugMessage("Starting History Query:");
  NotifyDebugMessage("  end_time = " + (end_time.is_null()
                                            ? "null"
                                            : base::TimeToISO8601(end_time)));
  NotifyDebugMessage(base::StringPrintf("  max_count = %zu", max_count));

  // TODO(crbug/1243049) : Add timing metrics for the history service DB query.
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<GetAnnotatedVisitsToCluster>(
          incomplete_visit_context_annotations_, begin_time, end_time,
          max_visit_count,
          base::BindOnce(&HistoryClustersService::OnGotHistoryVisits,
                         weak_ptr_factory_.GetWeakPtr(), query,
                         std::move(callback))),
      task_tracker);
}

void HistoryClustersService::RemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list,
    base::OnceClosure closure,
    base::CancelableTaskTracker* task_tracker) {
  // We expect HistoryService to internally delete any associated annotations
  // and cluster rows. In the future we may remove this indirection entirely.
  history_service_->ExpireHistory(expire_list, std::move(closure),
                                  task_tracker);
}

bool HistoryClustersService::DoesQueryMatchAnyCluster(
    const std::string& query) {
  if (!base::FeatureList::IsEnabled(kJourneys))
    return false;

  // 2 hour threshold chosen arbitrarily for cache refresh time.
  if ((base::Time::Now() - all_keywords_cache_timestamp_) > base::Hours(2) &&
      !cache_query_task_tracker_.HasTrackedTasks()) {
    // Update the timestamp right away, to prevent this from running again.
    // (The cache_query_task_tracker_ should also do this.)
    all_keywords_cache_timestamp_ = base::Time::Now();

    // Query for 30 days of clusters since older visits won't have keywords.
    const auto begin_time = base::Time::Now() - base::Days(30);
    // TODO(tommycli): This `QueryClusters()` correctly returns only clusters
    //  with `should_show_on_prominent_ui_surfaces` set to true because the
    //  `query` parameter is set to empty. However, it would be nice if this
    //  was more explicit, rather than just a happy coincidence. Likely the real
    //  solution will be to explicitly ask the backend for this bag of keywords.
    QueryClusters(
        /*query=*/"", begin_time, /*end_time=*/
        base::Time(), kMaxCountForKeywordCacheBatch,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), begin_time,
                       std::make_unique<std::set<std::u16string>>()),
        &cache_query_task_tracker_);
  }

  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query), query_parser::MatchingAlgorithm::DEFAULT,
      &query_nodes);

  return query_parser::QueryParser::DoesQueryMatch(all_keywords_cache_,
                                                   query_nodes,
                                                   /*exact=*/true);
}

std::vector<Cluster> HistoryClustersService::CollapseDuplicateVisits(
    const std::vector<history::Cluster>& raw_clusters) const {
  std::vector<Cluster> result_clusters;
  for (const auto& raw_cluster : raw_clusters) {
    Cluster cluster;
    cluster.cluster_id = raw_cluster.cluster_id;
    cluster.keywords = raw_cluster.keywords;

    // First stash all visits within the cluster in a id-keyed map.
    base::flat_map<int64_t, Visit> visits_map;
    visits_map.reserve(raw_cluster.visits.size());
    for (const auto& raw_visit : raw_cluster.visits) {
      Visit visit;
      visit.annotated_visit = raw_visit.annotated_visit;
      visit.normalized_url = raw_visit.normalized_url;
      visit.score = raw_visit.score;

      visits_map[visit.annotated_visit.visit_row.visit_id] = std::move(visit);
    }

    // Now do the actual un-flattening in a second loop.
    for (const auto& raw_visit : raw_cluster.visits) {
      int64_t visit_id = raw_visit.annotated_visit.visit_row.visit_id;

      // For every duplicate marked in the original raw visit, find the visit
      // in the id-keyed map, move it to the canonical visit's vector, and
      // erase it from the map.
      for (auto& duplicate_id : raw_visit.duplicate_visit_ids) {
        auto duplicate_visit = visits_map.find(duplicate_id);
        if (duplicate_visit == visits_map.end()) {
          NotifyDebugMessage(
              base::StringPrintf("Visit id=%d has missing duplicate_id=%d",
                                 int(visit_id), int(duplicate_id)));
          continue;
        }

        // Move the duplicate visit into the vector of the canonical visit.
        if (!duplicate_visit->second.duplicate_visits.empty()) {
          NotifyDebugMessage(
              "Duplicates shouldn't themselves have duplicates. "
              "If they do, the output is undefined.");
        }
        auto& canonical_visit = visits_map[visit_id];
        canonical_visit.duplicate_visits.push_back(
            std::move(duplicate_visit->second));

        // Remove the duplicate from the map.
        visits_map.erase(duplicate_visit);
      }
    }

    // Now move all our surviving visits, which should all be canonical visits,
    // to the final cluster.
    for (auto& visit_pair : visits_map) {
      cluster.visits.push_back(std::move(visit_pair.second));
    }

    result_clusters.push_back(std::move(cluster));
  }

  DCHECK_EQ(result_clusters.size(), raw_clusters.size());
  return result_clusters;
}

void HistoryClustersService::ClearKeywordCache() {
  all_keywords_cache_timestamp_ = base::Time();
  all_keywords_cache_.clear();
  cache_query_task_tracker_.TryCancelAll();
}

void HistoryClustersService::PopulateClusterKeywordCache(
    base::Time begin_time,
    std::unique_ptr<std::set<std::u16string>> keyword_accumulator,
    QueryClustersResult result) {
  // Copy keywords from every cluster into a the accumulator set.
  for (auto& cluster : result.clusters) {
    if (cluster.visits.size() < 2) {
      // Only accept keywords from clusters with at least two visits. This is a
      // simple first-pass technique to avoid overtriggering the omnibox action.
      continue;
    }
    keyword_accumulator->insert(cluster.keywords.begin(),
                                cluster.keywords.end());
  }

  // If there's still more to get, ask for another batch of keywords.
  if (result.continuation_end_time) {
    QueryClusters(
        /*query=*/"", begin_time, *result.continuation_end_time,
        kMaxCountForKeywordCacheBatch,
        base::BindOnce(&HistoryClustersService::PopulateClusterKeywordCache,
                       weak_ptr_factory_.GetWeakPtr(), begin_time,
                       // Pass on the accumulator set to the next callback.
                       std::move(keyword_accumulator)),
        &cache_query_task_tracker_);
    return;
  }

  // TODO(manukh) Even though `keyword_accumulator` is a `std::set`, we still
  //  often end up with duplicate keywords since different strings in the
  //  accumulator may contain the same words. We should add a metric to measure
  //  the keyword cache size and consider preventing duplicate keywords being
  //  inserted.

  // We've got all the keywords now, time to populate the cache.
  all_keywords_cache_.clear();
  for (auto& keyword : *keyword_accumulator) {
    // Each `keyword` may itself have multiple terms that we need to extract.
    query_parser::QueryParser::ExtractQueryWords(base::i18n::ToLower(keyword),
                                                 &all_keywords_cache_);
  }
}

void HistoryClustersService::OnGotHistoryVisits(
    const std::string& query,
    QueryClustersCallback callback,
    std::vector<history::AnnotatedVisit> annotated_visits,
    base::Time continuation_end_time) const {
  NotifyDebugMessage("HistoryClustersService::OnGotHistoryVisits()");
  NotifyDebugMessage(base::StringPrintf("  annotated_visits.size() = %zu",
                                        annotated_visits.size()));
  NotifyDebugMessage("  continuation_end_time = " +
                     (continuation_end_time.is_null()
                          ? "null (i.e. exhausted history)"
                          : base::TimeToISO8601(continuation_end_time)));

  if (annotated_visits.empty()) {
    // Early exit without calling backend if there's no annotated visits.
    QueryClustersResult result;
    if (!continuation_end_time.is_null()) {
      result.continuation_end_time = continuation_end_time;
    }
    std::move(callback).Run(std::move(result));
    return;
  }

  NotifyDebugMessage("  Visits JSON follows:");
  NotifyDebugMessage(GetDebugJSONForVisits(annotated_visits));

  NotifyDebugMessage("Calling backend_->GetClusters()");
  base::UmaHistogramCounts1000("History.Clusters.Backend.NumVisitsToCluster",
                               static_cast<int>(annotated_visits.size()));

  backend_->GetClusters(
      base::BindOnce(&HistoryClustersService::OnGotClusters,
                     weak_ptr_factory_.GetWeakPtr(), query,
                     continuation_end_time, base::TimeTicks::Now(),
                     std::move(callback)),
      annotated_visits);
}

void HistoryClustersService::OnGotClusters(
    const std::string& query,
    base::Time continuation_end_time,
    base::TimeTicks cluster_start_time,
    QueryClustersCallback callback,
    const std::vector<history::Cluster>& clusters) const {
  NotifyDebugMessage("HistoryClustersService::OnGotClusters()");
  QueryClustersResult result;
  if (!continuation_end_time.is_null()) {
    result.continuation_end_time = continuation_end_time;
  }

  base::UmaHistogramTimes("History.Clusters.Backend.GetClustersLatency",
                          base::TimeTicks::Now() - cluster_start_time);

  auto filtered_raw_clusters = FilterClustersMatchingQuery(query, clusters);
  result.clusters = CollapseDuplicateVisits(filtered_raw_clusters);
  result.clusters = SortClusters(result.clusters);

  base::UmaHistogramCounts1000("History.Clusters.Backend.NumClustersReturned",
                               static_cast<int>(clusters.size()));

  if (!clusters.empty()) {
    // Log the percentage of clusters that get filtered (e.g., 100 - % of
    // clusters that remain).
    base::UmaHistogramCounts100(
        "History.Clusters.PercentClustersFilteredByQuery",
        static_cast<int>(
            100 - (result.clusters.size() / (1.0 * clusters.size()) * 100)));
  }

  NotifyDebugMessage("  Clusters JSON follows:");
  NotifyDebugMessage(GetDebugJSONForClusters(clusters));

  NotifyDebugMessage("  Passing results back to original caller now.");
  std::move(callback).Run(std::move(result));
}

}  // namespace history_clusters
