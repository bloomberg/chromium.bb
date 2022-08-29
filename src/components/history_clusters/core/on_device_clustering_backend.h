// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/history_clusters/core/cluster_finalizer.h"
#include "components/history_clusters/core/cluster_processor.h"
#include "components/history_clusters/core/clusterer.h"
#include "components/history_clusters/core/clustering_backend.h"

namespace optimization_guide {
class BatchEntityMetadataTask;
struct EntityMetadata;
class EntityMetadataProvider;
class NewOptimizationGuideDecider;
}  // namespace optimization_guide

namespace site_engagement {
class SiteEngagementScoreProvider;
}  // namespace site_engagement

namespace history_clusters {

// A clustering backend that clusters visits on device.
class OnDeviceClusteringBackend : public ClusteringBackend {
 public:
  OnDeviceClusteringBackend(
      optimization_guide::EntityMetadataProvider* entity_metadata_provider,
      site_engagement::SiteEngagementScoreProvider* engagement_score_provider,
      optimization_guide::NewOptimizationGuideDecider*
          optimization_guide_decider);
  ~OnDeviceClusteringBackend() override;

  // ClusteringBackend:
  void GetClusters(ClusteringRequestSource clustering_request_source,
                   ClustersCallback callback,
                   std::vector<history::AnnotatedVisit> visits) override;

 private:
  // Callback invoked when batch entity metadata has been received from
  // |completed_task|. This will normalize |annotated_visits| and proceed to
  // cluster them after normalization.
  void OnBatchEntityMetadataRetrieved(
      ClusteringRequestSource clustering_request_source,
      optimization_guide::BatchEntityMetadataTask* completed_task,
      std::vector<history::AnnotatedVisit> annotated_visits,
      absl::optional<base::TimeTicks> entity_metadata_start,
      ClustersCallback callback,
      const base::flat_map<std::string, optimization_guide::EntityMetadata>&
          entity_metadata_map);

  // ProcessVisits adds additional metadata that might be used for clustering or
  // Journeys to each visit in |annotated_visits|, such as human-readable
  // entities and categories, site engagement, etc.
  void ProcessVisits(
      ClusteringRequestSource clustering_request_source,
      optimization_guide::BatchEntityMetadataTask* completed_task,
      std::vector<history::AnnotatedVisit> annotated_visits,
      absl::optional<base::TimeTicks> entity_metadata_start,
      ClustersCallback callback,
      const base::flat_map<std::string, optimization_guide::EntityMetadata>&
          entity_metadata_map);

  // Called when all visits have been processed.
  void OnAllVisitsFinishedProcessing(
      ClusteringRequestSource clustering_request_source,
      optimization_guide::BatchEntityMetadataTask* completed_task,
      std::vector<history::ClusterVisit> cluster_visits,
      base::flat_map<std::string, optimization_guide::EntityMetadata>
          human_readable_entity_name_to_entity_metadata_map,
      ClustersCallback callback);

  // Clusters |visits| on background thread.
  static std::vector<history::Cluster> ClusterVisitsOnBackgroundThread(
      bool engagement_score_provider_is_valid,
      std::vector<history::ClusterVisit> visits,
      base::flat_map<std::string, optimization_guide::EntityMetadata>
          human_readable_entity_name_to_entity_metadata_map);

  // The object to fetch entity metadata from. Not owned. Must outlive |this|.
  optimization_guide::EntityMetadataProvider* entity_metadata_provider_ =
      nullptr;

  // The object to get engagement scores from. Not owned. Must outlive |this|.
  site_engagement::SiteEngagementScoreProvider* engagement_score_provider_ =
      nullptr;

  // The object to fetch page load metadata from. Not owned. Must outlive
  // |this|.
  optimization_guide::NewOptimizationGuideDecider* optimization_guide_decider_ =
      nullptr;

  // The set of batch entity metadata tasks currently in flight.
  base::flat_set<std::unique_ptr<optimization_guide::BatchEntityMetadataTask>,
                 base::UniquePtrComparator>
      in_flight_batch_entity_metadata_tasks_;

  // The task runners to run clustering passes on.
  // |user_visible_priority_background_task_runner_| should be used iff
  // clustering is blocking content on a page that user is actively looking at.
  const base::TaskTraits user_visible_task_traits_;
  const base::TaskTraits continue_on_shutdown_user_visible_task_traits_;
  scoped_refptr<base::SequencedTaskRunner>
      user_visible_priority_background_task_runner_;
  const base::TaskTraits best_effort_task_traits_;
  const base::TaskTraits continue_on_shutdown_best_effort_task_traits_;
  scoped_refptr<base::SequencedTaskRunner>
      best_effort_priority_background_task_runner_;

  // Last time |engagement_score_cache_| was refreshed.
  base::TimeTicks engagement_score_cache_last_refresh_timestamp_;
  // URL host to score mapping.
  base::HashingLRUCache<std::string, float> engagement_score_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceClusteringBackend> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_
