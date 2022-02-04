// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_TYPES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_TYPES_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/history/core/browser/history_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history_clusters {

// The result data returned by `QueryClusters()`.
struct QueryClustersResult {
  QueryClustersResult();
  ~QueryClustersResult();
  QueryClustersResult(const QueryClustersResult&);

  std::vector<history::Cluster> clusters;

  // A nullopt `continuation_end_time` means we have exhausted History.
  // Note that this differs from History itself, which uses base::Time() as the
  // value to indicate we've exhausted history. I've found that to be not
  // explicit enough in practice. This value will never be base::Time().
  absl::optional<base::Time> continuation_end_time;
};
using QueryClustersCallback = base::OnceCallback<void(QueryClustersResult)>;

// Tracks which fields have been or are pending recording. This helps 1) avoid
// re-recording fields and 2) determine whether a visit is compete (i.e. has all
// expected fields recorded).
struct RecordingStatus {
  // Whether `url_row` and `visit_row` have been set.
  bool history_rows = false;
  // Whether a navigation has ended; i.e. another navigation has began in the
  // same tab or the navigation's tab has been closed.
  bool navigation_ended = false;
  // Whether the `context_annotations` associated with navigation end have been
  // set. Should only be true if both `history_rows` and `navigation_ended` are
  // true.
  bool navigation_end_signals = false;
  // Whether the UKM `page_end_reason` `context_annotations` is expected to be
  // set.
  bool expect_ukm_page_end_signals = false;
  // Whether the UKM `page_end_reason` `context_annotations` has been set.
  // Should only be true if `expect_ukm_page_end_signals` is true.
  bool ukm_page_end_signals = false;
};

// A partially built VisitContextAnnotations with its state of completeness and
// associated `URLRow` and `VisitRow` which are necessary to build it.
struct IncompleteVisitContextAnnotations {
  RecordingStatus status;
  history::URLRow url_row;
  history::VisitRow visit_row;
  history::VisitContextAnnotations context_annotations;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_TYPES_H_
