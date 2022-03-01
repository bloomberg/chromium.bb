// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/history_clusters/core/cluster_finalizer.h"
#include "components/history_clusters/core/cluster_processor.h"
#include "components/history_clusters/core/clusterer.h"
#include "components/history_clusters/core/clustering_backend.h"

class TemplateURLService;

namespace optimization_guide {
class BatchEntityMetadataTask;
struct EntityMetadata;
class EntityMetadataProvider;
}  // namespace optimization_guide

namespace site_engagement {
class SiteEngagementScoreProvider;
}  // namespace site_engagement

namespace history_clusters {

// A clustering backend that clusters visits on device.
class OnDeviceClusteringBackend : public ClusteringBackend {
 public:
  OnDeviceClusteringBackend(
      TemplateURLService* template_url_service,
      optimization_guide::EntityMetadataProvider* entity_metadata_provider,
      site_engagement::SiteEngagementScoreProvider* engagement_score_provider);
  ~OnDeviceClusteringBackend() override;

  // ClusteringBackend:
  void GetClusters(ClustersCallback callback,
                   const std::vector<history::AnnotatedVisit>& visits) override;

 private:
  // Callback invoked when batch entity metadata has been received from
  // |completed_task|. This will normalize |annotated_visits| and proceed to
  // cluster them after normalization.
  void OnBatchEntityMetadataRetrieved(
      optimization_guide::BatchEntityMetadataTask* completed_task,
      const std::vector<history::AnnotatedVisit>& annotated_visits,
      absl::optional<base::TimeTicks> entity_metadata_start,
      ClustersCallback callback,
      const base::flat_map<std::string, optimization_guide::EntityMetadata>&
          entity_metadata_map);

  // Clusters |visits| on background thread.
  std::vector<history::Cluster> ClusterVisitsOnBackgroundThread(
      const std::vector<history::ClusterVisit>& visits);

  // The object used to normalize SRP URLs. Not owned. Must outlive |this|.
  const TemplateURLService* template_url_service_;

  // The object to fetch entity metadata from. Not owned. Must outlive |this|.
  optimization_guide::EntityMetadataProvider* entity_metadata_provider_;

  // The object to get engagement scores from. Not owned. Must outlive |this|.
  site_engagement::SiteEngagementScoreProvider* engagement_score_provider_;

  // The set of batch entity metadata tasks currently in flight.
  base::flat_set<std::unique_ptr<optimization_guide::BatchEntityMetadataTask>,
                 base::UniquePtrComparator>
      in_flight_batch_entity_metadata_tasks_;

  // The task runner to run all clustering passes on.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceClusteringBackend> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_BACKEND_H_
