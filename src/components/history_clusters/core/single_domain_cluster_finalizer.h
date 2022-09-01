// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_SINGLE_DOMAIN_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_SINGLE_DOMAIN_CLUSTER_FINALIZER_H_

#include "components/history_clusters/core/cluster_finalizer.h"

namespace history_clusters {

// A cluster finalizer that finalizes single-domain clusters by not showing
// them.
class SingleDomainClusterFinalizer : public ClusterFinalizer {
 public:
  SingleDomainClusterFinalizer();
  ~SingleDomainClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_SINGLE_DOMAIN_CLUSTER_FINALIZER_H_
