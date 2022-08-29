// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/keyword_cluster_finalizer.h"

#include <queue>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/optimization_guide/core/entity_metadata.h"

namespace history_clusters {

namespace {

static constexpr float kSearchTermsScore = 100.0;
static constexpr float kScoreEpsilon = 1e-8;

bool IsKeywordSimilarToVisitHost(
    const std::vector<std::u16string>& lowercase_host_parts,
    const std::u16string& keyword) {
  std::u16string lowercase_keyword = base::ToLowerASCII(keyword);
  if (base::Contains(lowercase_host_parts, keyword))
    return true;

  // Now check if the whitespace-stripped keyword is part of the host.
  std::u16string stripped_lowercase_keyword;
  base::RemoveChars(lowercase_keyword, base::kWhitespaceASCIIAs16,
                    &stripped_lowercase_keyword);
  return base::Contains(lowercase_host_parts, stripped_lowercase_keyword);
}

// Computes an keyword score per cluster visit by mulitplying its visit-wise
// weight and cluster visit score, and applying a weight factor.
float ComputeKeywordScorePerClusterVisit(int visit_keyword_weight,
                                         float cluster_visit_score,
                                         float weight = 1.0) {
  return static_cast<float>(visit_keyword_weight) * cluster_visit_score *
         weight;
}

void KeepTopKeywords(
    base::flat_map<std::u16string, history::ClusterKeywordData>&
        keyword_to_data_map,
    size_t max_num_keywords_per_cluster) {
  if (keyword_to_data_map.size() <= max_num_keywords_per_cluster) {
    return;
  }

  // Compare keywords first by their scores and then by types.
  auto cmp_keywords =
      [](const std::pair<std::u16string, const history::ClusterKeywordData*>&
             left,
         const std::pair<std::u16string, const history::ClusterKeywordData*>&
             right) {
        return std::fabs(left.second->score - right.second->type) >
                       kScoreEpsilon
                   ? left.second->score > right.second->score
                   : left.second->type > right.second->type;
      };

  // Minimum priority queue of top keywords.
  std::priority_queue<
      std::pair<std::u16string, const history::ClusterKeywordData*>,
      std::vector<
          std::pair<std::u16string, const history::ClusterKeywordData*>>,
      decltype(cmp_keywords)>
      pq(cmp_keywords);
  for (const auto& keyword_data_p : keyword_to_data_map) {
    bool should_insert = false;
    if (pq.size() < max_num_keywords_per_cluster) {
      should_insert = true;
    } else {
      if (pq.top().second->score < keyword_data_p.second.score) {
        pq.pop();
        should_insert = true;
      }
    }
    if (should_insert) {
      pq.push(std::make_pair(keyword_data_p.first, &keyword_data_p.second));
    }
  }

  base::flat_set<std::u16string> keywords_set;
  while (!pq.empty()) {
    keywords_set.insert(pq.top().first);
    pq.pop();
  }

  auto it = keyword_to_data_map.begin();
  for (; it != keyword_to_data_map.end();) {
    if (!keywords_set.contains(it->first)) {
      it = keyword_to_data_map.erase(it);
    } else {
      it++;
    }
  }
  DCHECK_EQ(keyword_to_data_map.size(), max_num_keywords_per_cluster);
}

}  // namespace

KeywordClusterFinalizer::KeywordClusterFinalizer(
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_metadata_map)
    : entity_metadata_map_(entity_metadata_map) {}
KeywordClusterFinalizer::~KeywordClusterFinalizer() = default;

void KeywordClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<std::u16string, history::ClusterKeywordData>
      keyword_to_data_map;
  for (const auto& visit : cluster.visits) {
    if (!GetConfig().keyword_filter_on_noisy_visits && IsNoisyVisit(visit)) {
      // Do not put keywords if user visits the page a lot and it's not a
      // search-like visit.
      continue;
    }

    std::vector<std::u16string> lowercase_host_parts = base::SplitString(
        base::ToLowerASCII(
            base::UTF8ToUTF16(visit.annotated_visit.url_row.url().host())),
        u".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& entity :
         visit.annotated_visit.content_annotations.model_annotations.entities) {
      base::flat_set<std::u16string> entity_keywords;
      const std::u16string keyword_u16str = base::UTF8ToUTF16(entity.id);
      entity_keywords.insert(keyword_u16str);

      // Add an entity to keyword data.
      const float entity_score =
          ComputeKeywordScorePerClusterVisit(entity.weight, visit.score);
      auto keyword_it = keyword_to_data_map.find(keyword_u16str);
      if (keyword_it != keyword_to_data_map.end()) {
        // Accumulate entity scores from multiple visits.
        keyword_it->second.score += entity_score;
        keyword_it->second.MaybeUpdateKeywordType(
            history::ClusterKeywordData::kEntity);
      } else {
        keyword_to_data_map[keyword_u16str] = history::ClusterKeywordData(
            history::ClusterKeywordData::kEntity, entity_score,
            /*entity_collections=*/{});
      }

      // Add the top one entity collection to keyword data.
      const auto it = entity_metadata_map_.find(entity.id);
      if (it != entity_metadata_map_.end() && !it->second.collections.empty()) {
        keyword_to_data_map[keyword_u16str].entity_collections = {
            it->second.collections[0]};
      }

      if (GetConfig().keyword_filter_on_entity_aliases) {
        if (it != entity_metadata_map_.end()) {
          for (size_t i = 0; i < it->second.human_readable_aliases.size() &&
                             i < GetConfig().max_entity_aliases_in_keywords;
               i++) {
            const auto alias =
                base::UTF8ToUTF16(it->second.human_readable_aliases[i]);
            entity_keywords.insert(alias);
            // Use the same score and collections of an entity for its aliases
            // as well.
            auto alias_it = keyword_to_data_map.find(alias);
            if (alias_it == keyword_to_data_map.end()) {
              keyword_to_data_map[alias] = history::ClusterKeywordData(
                  history::ClusterKeywordData::kEntityAlias, entity_score, {});
            } else {
              alias_it->second.score += entity_score;
              alias_it->second.MaybeUpdateKeywordType(
                  history::ClusterKeywordData::kEntityAlias);
            }
            keyword_to_data_map[alias].entity_collections =
                keyword_to_data_map[keyword_u16str].entity_collections;
          }
        }
      }

      if (!GetConfig().keyword_filter_on_visit_hosts) {
        // If we do not want any keywords associated with the visit host, make
        // sure that none of the keywords associated with the entity look like
        // they are for the visit host.
        bool clear_entity_keywords = false;
        for (const auto& entity_keyword : entity_keywords) {
          if (IsKeywordSimilarToVisitHost(lowercase_host_parts,
                                          entity_keyword)) {
            // One of the keywords is likely for the visit host, so clear out
            // the keywords for the whole entity.
            clear_entity_keywords = true;
            break;
          }
        }
        if (clear_entity_keywords) {
          for (const auto& keyword : entity_keywords) {
            keyword_to_data_map.erase(keyword);
          }
        }
      }
    }
    if (GetConfig().keyword_filter_on_categories) {
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        std::u16string category_u16string = base::UTF8ToUTF16(category.id);
        if (!GetConfig().keyword_filter_on_visit_hosts &&
            IsKeywordSimilarToVisitHost(lowercase_host_parts,
                                        category_u16string)) {
          continue;
        }
        // Add a discounted keyword score for categories.
        const float category_score = ComputeKeywordScorePerClusterVisit(
            category.weight, visit.score,
            GetConfig().category_keyword_score_weight);
        auto category_it = keyword_to_data_map.find(category_u16string);
        if (category_it == keyword_to_data_map.end()) {
          keyword_to_data_map[category_u16string] = history::ClusterKeywordData(
              history::ClusterKeywordData::kEntityCategory, category_score,
              /*entity_collections=*/{});
        } else {
          // Accumulate category scores if there are multiple.
          category_it->second.score += category_score;
          category_it->second.MaybeUpdateKeywordType(
              history::ClusterKeywordData::kEntityCategory);
        }
      }
    }

    if (GetConfig().keyword_filter_on_search_terms &&
        !visit.annotated_visit.content_annotations.search_terms.empty()) {
      const auto& search_terms =
          visit.annotated_visit.content_annotations.search_terms;
      auto search_it = keyword_to_data_map.find(search_terms);
      if (search_it == keyword_to_data_map.end()) {
        keyword_to_data_map[search_terms] = history::ClusterKeywordData(
            history::ClusterKeywordData::kSearchTerms,
            /*score=*/kSearchTermsScore, /*entity_collections=*/{});
      } else {
        search_it->second.score += kSearchTermsScore;
        search_it->second.MaybeUpdateKeywordType(
            history::ClusterKeywordData::kSearchTerms);
      }
    }
  }

  KeepTopKeywords(keyword_to_data_map,
                  GetConfig().max_num_keywords_per_cluster);

  cluster.keyword_to_data_map = std::move(keyword_to_data_map);
}

}  // namespace history_clusters
