// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_backend.h"
#include "components/history_clusters/core/history_clusters_types.h"

namespace history_clusters {

class HistoryClustersService;

class HistoryClustersServiceTaskGetMostRecentClusters {
 public:
  HistoryClustersServiceTaskGetMostRecentClusters(
      base::WeakPtr<HistoryClustersService> weak_history_clusters_service,
      const IncompleteVisitMap incomplete_visit_context_annotations,
      ClusteringBackend* const backend,
      history::HistoryService* const history_service,
      ClusteringRequestSource clustering_request_source,
      base::Time begin_time,
      QueryClustersContinuationParams continuation_params,
      QueryClustersCallback callback);
  ~HistoryClustersServiceTaskGetMostRecentClusters();

  bool Done() { return done_; }

 private:
  // Invoked during construction. Will asyncly request annotated visits from
  // `GetAnnotatedVisitsToCluster`.
  void Start();

  // Invoked after `Start()` asyncly fetches annotated visits. Will asyncly
  // request clusters from `ClusteringBackend`.
  void OnGotAnnotatedVisitsToCluster(
      std::vector<history::AnnotatedVisit> annotated_visits,
      QueryClustersContinuationParams continuation_params);

  // Invoked after `OnGotAnnotatedVisitsToCluster()` asyncly obtains clusters.
  // Will synchronously invoke `callback_`.
  void OnGotModelClusters(QueryClustersContinuationParams continuation_params,
                          std::vector<history::Cluster> clusters);

  // Never nullptr.
  base::WeakPtr<HistoryClustersService> weak_history_clusters_service_;
  const IncompleteVisitMap incomplete_visit_context_annotations_;
  // Can be nullptr.
  ClusteringBackend* const backend_;
  // Non-owning pointer, but never nullptr.
  history::HistoryService* const history_service_;

  // Used to make requests to `ClusteringBackend`.
  ClusteringRequestSource clustering_request_source_;

  // Used to make requests to `GetAnnotatedVisitsToCluster`.
  base::Time begin_time_;
  QueryClustersContinuationParams continuation_params_;
  base::CancelableTaskTracker task_tracker_;

  // Invoked after `OnGotModelClusters().
  QueryClustersCallback callback_;

  // When `Start()` kicked off the request to fetch visits to cluster.
  base::TimeTicks history_service_get_annotated_visits_to_cluster_start_time_;
  // When `OnGotAnnotatedVisitsToCluster()` kicked off the request to cluster
  // the visits.
  base::TimeTicks backend_get_clusters_start_time_;

  // Set to true when `callback_` is invoked, either with clusters or no
  // clusters.
  bool done_ = false;

  // Used for async callbacks.
  base::WeakPtrFactory<HistoryClustersServiceTaskGetMostRecentClusters>
      weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_GET_MOST_RECENT_CLUSTERS_H_
