// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/ranking_cluster_finalizer.h"

#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

namespace {

bool IsCanonicalVisit(const base::flat_set<history::VisitID>& duplicate_ids,
                      const history::ClusterVisit& visit) {
  return duplicate_ids.find(visit.annotated_visit.visit_row.visit_id) ==
         duplicate_ids.end();
}

// See https://en.wikipedia.org/wiki/Smoothstep.
float clamp(float x, float lowerlimit, float upperlimit) {
  if (x < lowerlimit)
    x = lowerlimit;
  if (x > upperlimit)
    x = upperlimit;
  return x;
}

// Maps |values| from the range specified by |low| and
// |high| into [0, 1].
// See https://en.wikipedia.org/wiki/Smoothstep.
float Smoothstep(float low, float high, float value) {
  DCHECK_NE(low, high);
  const float x = clamp((value - low) / (high - low), 0.0, 1.0);
  return x * x * (3 - 2 * x);
}

}  // namespace

RankingClusterFinalizer::RankingClusterFinalizer() = default;
RankingClusterFinalizer::~RankingClusterFinalizer() = default;

void RankingClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<history::VisitID, VisitScores> url_visit_scores;
  const base::flat_set<history::VisitID>& duplicate_visit_ids =
      CalculateAllDuplicateVisitsForCluster(cluster);

  CalculateVisitDurationScores(cluster, url_visit_scores, duplicate_visit_ids);
  CalculateVisitAttributeScoring(cluster, url_visit_scores,
                                 duplicate_visit_ids);
  ComputeFinalVisitScores(cluster, url_visit_scores, duplicate_visit_ids);
}

void RankingClusterFinalizer::CalculateVisitAttributeScoring(
    history::Cluster& cluster,
    base::flat_map<history::VisitID, VisitScores>& url_visit_scores,
    const base::flat_set<history::VisitID>& duplicate_visit_ids) {
  for (auto visit_it = cluster.visits.rbegin();
       visit_it != cluster.visits.rend(); ++visit_it) {
    auto& visit = *visit_it;
    if (!IsCanonicalVisit(duplicate_visit_ids, visit)) {
      continue;
    }
    auto it = url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);
    if (it == url_visit_scores.end()) {
      auto visit_score = VisitScores();
      url_visit_scores.insert(
          {visit.annotated_visit.visit_row.visit_id, visit_score});
    }
    it = url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);

    // Check if the visit is bookmarked.
    if (visit.annotated_visit.context_annotations.is_existing_bookmark ||
        visit.annotated_visit.context_annotations.is_new_bookmark) {
      it->second.set_bookmarked();
    }

    // Check if the visit contained a search query.
    if (visit.is_search_visit) {
      it->second.set_is_srp();
    }

    // Additional/future attribute checks go here.
  }
}

void RankingClusterFinalizer::CalculateVisitDurationScores(
    history::Cluster& cluster,
    base::flat_map<history::VisitID, VisitScores>& url_visit_scores,
    const base::flat_set<history::VisitID>& duplicate_visit_ids) {
  // |max_visit_duration| and |max_foreground_duration| must be > 0 for
  // reshaping between 0 and 1.
  base::TimeDelta max_visit_duration = base::Seconds(1);
  base::TimeDelta max_foreground_duration = base::Seconds(1);
  for (const auto& visit : cluster.visits) {
    // We don't care about checking for duplicate visits since the duration
    // should have been rolled up already.
    if (visit.annotated_visit.visit_row.visit_duration > max_visit_duration) {
      max_visit_duration = visit.annotated_visit.visit_row.visit_duration;
    }
    if (visit.annotated_visit.context_annotations.total_foreground_duration >
        max_foreground_duration) {
      max_foreground_duration =
          visit.annotated_visit.context_annotations.total_foreground_duration;
    }
  }
  for (auto visit_it = cluster.visits.rbegin();
       visit_it != cluster.visits.rend(); ++visit_it) {
    auto& visit = *visit_it;
    if (!IsCanonicalVisit(duplicate_visit_ids, visit)) {
      continue;
    }
    float visit_duration_score =
        Smoothstep(0.0f, max_visit_duration.InSecondsF(),
                   visit.annotated_visit.visit_row.visit_duration.InSecondsF());
    float foreground_duration_score =
        Smoothstep(0.0f, max_foreground_duration.InSecondsF(),
                   std::max(0.0, visit.annotated_visit.context_annotations
                                     .total_foreground_duration.InSecondsF()));
    auto visit_scores_it =
        url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);
    if (visit_scores_it == url_visit_scores.end()) {
      VisitScores visit_scores;
      visit_scores.set_visit_duration_score(visit_duration_score);
      visit_scores.set_foreground_duration_score(foreground_duration_score);
      url_visit_scores.insert(
          {visit.annotated_visit.visit_row.visit_id, std::move(visit_scores)});
    } else {
      visit_scores_it->second.set_visit_duration_score(visit_duration_score);
      visit_scores_it->second.set_foreground_duration_score(
          foreground_duration_score);
    }
  }
}

void RankingClusterFinalizer::ComputeFinalVisitScores(
    history::Cluster& cluster,
    base::flat_map<history::VisitID, VisitScores>& url_visit_scores,
    const base::flat_set<history::VisitID>& duplicate_visit_ids) {
  float max_score = -1.0;
  for (auto visit_it = cluster.visits.rbegin();
       visit_it != cluster.visits.rend(); ++visit_it) {
    auto& visit = *visit_it;

    // Only canonical visits should have scores > 0.0.
    if (!IsCanonicalVisit(duplicate_visit_ids, visit)) {
      // Check that no individual scores have been given a visit that is not
      // canonical a score.
      DCHECK(url_visit_scores.find(visit.annotated_visit.visit_row.visit_id) ==
             url_visit_scores.end());
      visit.score = 0.0;
      continue;
    }

    // Determine the max score to use for normalizing all the scores.
    auto visit_url = visit.normalized_url;
    auto visit_scores_it =
        url_visit_scores.find(visit.annotated_visit.visit_row.visit_id);
    if (visit_scores_it != url_visit_scores.end()) {
      visit.score = visit_scores_it->second.GetTotalScore();
      if (visit.score > max_score) {
        max_score = visit.score;
      }
    }
  }
  if (max_score <= 0.0)
    return;

  // Now normalize the score by `max_score` so they values are all between 0
  // and 1.
  for (auto visit_it = cluster.visits.rbegin();
       visit_it != cluster.visits.rend(); ++visit_it) {
    auto& visit = *visit_it;
    visit.score = visit.score / max_score;
  }
}

}  // namespace history_clusters
