// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_backend.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/content_annotations_cluster_processor.h"
#include "components/history_clusters/core/content_visibility_cluster_finalizer.h"
#include "components/history_clusters/core/keyword_cluster_finalizer.h"
#include "components/history_clusters/core/noisy_cluster_finalizer.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/history_clusters/core/ranking_cluster_finalizer.h"
#include "components/history_clusters/core/similar_visit_deduper_cluster_finalizer.h"
#include "components/history_clusters/core/single_visit_cluster_finalizer.h"
#include "components/history_clusters/core/url_deduper_cluster_finalizer.h"
#include "components/optimization_guide/core/batch_entity_metadata_task.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/search_engines/template_url_service.h"
#include "components/site_engagement/core/site_engagement_score_provider.h"

namespace history_clusters {

namespace {

// Returns the normalized URL for the search provider if the URL for the visit
// is a Search URL. Otherwise, returns nullopt.
absl::optional<GURL> GetNormalizedURLForSearchVisit(
    const history::AnnotatedVisit& visit,
    const TemplateURLService* template_url_service) {
  if (!template_url_service)
    return absl::nullopt;

  const TemplateURL* template_url =
      template_url_service->GetTemplateURLForHost(visit.url_row.url().host());
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  std::u16string search_terms;
  bool is_valid_search_url =
      template_url &&
      template_url->ExtractSearchTermsFromURL(
          visit.url_row.url(), search_terms_data, &search_terms) &&
      !search_terms.empty();
  if (!is_valid_search_url)
    return absl::nullopt;

  const std::u16string& normalized_search_query =
      base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
  TemplateURLRef::SearchTermsArgs search_terms_args(normalized_search_query);
  const TemplateURLRef& search_url_ref = template_url->url_ref();
  if (!search_url_ref.SupportsReplacement(search_terms_data))
    return absl::nullopt;

  return GURL(
      search_url_ref.ReplaceSearchTerms(search_terms_args, search_terms_data));
}

}  // namespace

OnDeviceClusteringBackend::OnDeviceClusteringBackend(
    TemplateURLService* template_url_service,
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    site_engagement::SiteEngagementScoreProvider* engagement_score_provider)
    : template_url_service_(template_url_service),
      entity_metadata_provider_(entity_metadata_provider),
      engagement_score_provider_(engagement_score_provider),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

OnDeviceClusteringBackend::~OnDeviceClusteringBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OnDeviceClusteringBackend::GetClusters(
    ClustersCallback callback,
    const std::vector<history::AnnotatedVisit>& visits) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visits.empty()) {
    std::move(callback).Run({});
    return;
  }

  // Just start clustering without getting entity metadata if we don't have a
  // provider to translate the entities.
  if (!entity_metadata_provider_) {
    OnBatchEntityMetadataRetrieved(/*completed_task=*/nullptr, visits,
                                   /*entity_metadata_start=*/absl::nullopt,
                                   std::move(callback),
                                   /*entity_metadata_map=*/{});
    return;
  }

  // Figure out what entity IDs we need to fetch metadata for.
  base::flat_set<std::string> entity_ids;
  for (const auto& visit : visits) {
    for (const auto& entity :
         visit.content_annotations.model_annotations.entities) {
      entity_ids.insert(entity.id);
    }
  }

  // Don't bother with getting entity metadata if there's nothing to get
  // metadata for.
  if (entity_ids.empty()) {
    OnBatchEntityMetadataRetrieved(/*completed_task=*/nullptr, visits,
                                   /*entity_metadata_start=*/absl::nullopt,
                                   std::move(callback),
                                   /*entity_metadata_map=*/{});
    return;
  }

  base::UmaHistogramCounts1000("History.Clusters.Backend.BatchEntityLookupSize",
                               entity_ids.size());

  // Fetch the metadata for the entity ID present in |visits|.
  auto batch_entity_metadata_task =
      std::make_unique<optimization_guide::BatchEntityMetadataTask>(
          entity_metadata_provider_, entity_ids);
  auto* batch_entity_metadata_task_ptr = batch_entity_metadata_task.get();
  in_flight_batch_entity_metadata_tasks_.insert(
      std::move(batch_entity_metadata_task));
  batch_entity_metadata_task_ptr->Execute(base::BindOnce(
      &OnDeviceClusteringBackend::OnBatchEntityMetadataRetrieved,
      weak_ptr_factory_.GetWeakPtr(), batch_entity_metadata_task_ptr, visits,
      base::TimeTicks::Now(), std::move(callback)));
}

void OnDeviceClusteringBackend::OnBatchEntityMetadataRetrieved(
    optimization_guide::BatchEntityMetadataTask* completed_task,
    const std::vector<history::AnnotatedVisit>& annotated_visits,
    absl::optional<base::TimeTicks> entity_metadata_start,
    ClustersCallback callback,
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (entity_metadata_start) {
    base::UmaHistogramTimes("History.Clusters.Backend.BatchEntityLookupLatency",
                            *entity_metadata_start - base::TimeTicks::Now());
  }
  // Rewrite the visits based on the mapping and normalize URLs here.
  std::vector<history::ClusterVisit> cluster_visits;
  cluster_visits.reserve(annotated_visits.size());
  for (const auto& visit : annotated_visits) {
    history::ClusterVisit cluster_visit;
    cluster_visit.annotated_visit = visit;
    absl::optional<GURL> maybe_normalized_url =
        GetNormalizedURLForSearchVisit(visit, template_url_service_);
    cluster_visit.is_search_visit = maybe_normalized_url.has_value();
    cluster_visit.normalized_url =
        maybe_normalized_url.value_or(visit.url_row.url());

    if (engagement_score_provider_) {
      cluster_visit.engagement_score =
          engagement_score_provider_->GetScore(visit.url_row.url());
    }

    // Rewrite the entities for the visit, but only if it is possible that we
    // had additional metadata for it.
    if (entity_metadata_provider_) {
      cluster_visit.annotated_visit.content_annotations.model_annotations
          .entities.clear();
      cluster_visit.annotated_visit.content_annotations.model_annotations
          .categories.clear();
      base::flat_set<std::string> inserted_categories;
      for (const auto& entity :
           visit.content_annotations.model_annotations.entities) {
        auto entity_metadata_it = entity_metadata_map.find(entity.id);
        if (entity_metadata_it == entity_metadata_map.end()) {
          continue;
        }

        history::VisitContentModelAnnotations::Category rewritten_entity =
            entity;
        rewritten_entity.id = entity_metadata_it->second.human_readable_name;
        cluster_visit.annotated_visit.content_annotations.model_annotations
            .entities.push_back(rewritten_entity);

        for (const auto& category :
             entity_metadata_it->second.human_readable_categories) {
          if (inserted_categories.find(category.first) ==
              inserted_categories.end()) {
            cluster_visit.annotated_visit.content_annotations.model_annotations
                .categories.push_back(
                    {category.first, static_cast<int>(100 * category.second)});
            inserted_categories.insert(category.first);
          }
        }
      }
      if (visit.content_annotations.model_annotations
              .page_topics_model_version <
          features::GetMinPageTopicsModelVersionToUseContentVisibilityFrom()) {
        // Override the visibility score to be as if the model was not evaluated
        // if the version of the model being used does not exceed the min
        // version that was not a random model.
        cluster_visit.annotated_visit.content_annotations.model_annotations
            .visibility_score = -1.0;
      }
    }

    cluster_visits.push_back(cluster_visit);
  }

  // Mark the task as completed, which will destruct |entity_metadata_map|.
  if (completed_task) {
    auto it = in_flight_batch_entity_metadata_tasks_.find(completed_task);
    if (it != in_flight_batch_entity_metadata_tasks_.end())
      in_flight_batch_entity_metadata_tasks_.erase(it);
  }

  // Post the actual clustering work onto the thread pool, then reply on the
  // calling sequence. This is to prevent UI jank.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread,
          base::Unretained(this), cluster_visits),
      std::move(callback));
}

std::vector<history::Cluster>
OnDeviceClusteringBackend::ClusterVisitsOnBackgroundThread(
    const std::vector<history::ClusterVisit>& visits) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug.com/1260145): All of these objects are "stateless" between
  // requests for clusters. If there needs to be shared state, the entire
  // backend needs to be refactored to separate these objects from the UI and
  // background thread.
  std::unique_ptr<Clusterer> clusterer = std::make_unique<Clusterer>();

  // The cluster processors to be run.
  std::vector<std::unique_ptr<ClusterProcessor>> cluster_processors;

  // The cluster finalizers to be run.
  std::vector<std::unique_ptr<ClusterFinalizer>> cluster_finalizers;

  if (features::ContentClusteringEnabled()) {
    cluster_processors.push_back(
        std::make_unique<ContentAnnotationsClusterProcessor>());
  }

  cluster_finalizers.push_back(
      std::make_unique<ContentVisibilityClusterFinalizer>());
  if (features::ShouldDedupeSimilarVisits()) {
    cluster_finalizers.push_back(
        std::make_unique<SimilarVisitDeduperClusterFinalizer>());
  } else {
    // Otherwise, only dedupe based on normalized URL.
    cluster_finalizers.push_back(
        std::make_unique<UrlDeduperClusterFinalizer>());
  }
  cluster_finalizers.push_back(std::make_unique<RankingClusterFinalizer>());
  if (features::ShouldHideSingleVisitClustersOnProminentUISurfaces()) {
    cluster_finalizers.push_back(
        std::make_unique<SingleVisitClusterFinalizer>());
  }
  // Add feature to turn on/off site engagement score filter.
  if (engagement_score_provider_ && features::ShouldFilterNoisyClusters()) {
    cluster_finalizers.push_back(std::make_unique<NoisyClusterFinalizer>());
  }
  cluster_finalizers.push_back(std::make_unique<KeywordClusterFinalizer>());

  // Group visits into clusters.
  std::vector<history::Cluster> clusters =
      clusterer->CreateInitialClustersFromVisits(visits);

  // Process clusters.
  for (const auto& processor : cluster_processors) {
    clusters = processor->ProcessClusters(clusters);
  }

  // Run finalizers that dedupe and score visits within a cluster and
  // log several metrics about the result.
  std::vector<int> keyword_sizes;
  std::vector<int> visits_in_clusters;
  for (auto& cluster : clusters) {
    for (const auto& finalizer : cluster_finalizers) {
      finalizer->FinalizeCluster(cluster);
    }
    visits_in_clusters.emplace_back(cluster.visits.size());
    keyword_sizes.emplace_back(cluster.keywords.size());
  }

  // It's a bit strange that this is essentially a `ClusterProcessor` but has
  // to operate after the finalizers.
  SortClusters(&clusters);

  if (!visits_in_clusters.empty()) {
    // We check for empty to ensure the below code doesn't crash, but
    // realistically this vector should never be empty.
    base::UmaHistogramCounts1000("History.Clusters.Backend.ClusterSize.Min",
                                 *std::min_element(visits_in_clusters.begin(),
                                                   visits_in_clusters.end()));
    base::UmaHistogramCounts1000("History.Clusters.Backend.ClusterSize.Max",
                                 *std::max_element(visits_in_clusters.begin(),
                                                   visits_in_clusters.end()));
  }
  if (!keyword_sizes.empty()) {
    // We check for empty to ensure the below code doesn't crash, but
    // realistically this vector should never be empty.
    base::UmaHistogramCounts100(
        "History.Clusters.Backend.NumKeywordsPerCluster.Min",
        *std::min_element(keyword_sizes.begin(), keyword_sizes.end()));
    base::UmaHistogramCounts100(
        "History.Clusters.Backend.NumKeywordsPerCluster.Max",
        *std::max_element(keyword_sizes.begin(), keyword_sizes.end()));
  }

  return clusters;
}

}  // namespace history_clusters
