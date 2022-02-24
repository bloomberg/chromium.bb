// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history_clusters {

// Computes a simplified GURL for deduping purposes only. The resulting GURL may
// not be valid or navigable, and is only intended for History Cluster deduping.
//
// Note, this is NOT meant to be applied to Search Result Page URLs. Those
// should be separately canonicalized by TemplateURLService and not sent here.
GURL ComputeURLForDeduping(const GURL& url);

// Stable sorts visits according to score, then reverse-chronologically.
void StableSortVisits(std::vector<history::ClusterVisit>* visits);

// Erases all clusters that don't match `query`. Also may re-score the visits
// within matching clusters.
//
// If `query` is an empty string, leaves `clusters` unmodified.
void ApplySearchQuery(const std::string& query,
                      std::vector<history::Cluster>* clusters);

// If `query` is empty, erases all non-prominent clusters.
//
// If `query` is non-empty, we assume that the user is searching for something,
// so we only cull duplicate occurrences of single-visit non-prominent clusters.
// The set of single-visit clusters we've already seen is tracked by
// `seen_single_visit_cluster_urls` and this function updates that set.
void CullNonProminentOrDuplicateClusters(
    std::string query,
    std::vector<history::Cluster>* clusters,
    std::set<GURL>* seen_single_visit_cluster_urls);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_UTIL_H_
