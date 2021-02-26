// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/linear_map_search.h"

#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/components/local_search_service/search_utils.h"
#include "chromeos/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace local_search_service {

namespace {

using chromeos::string_matching::FuzzyTokenizedStringMatch;
using chromeos::string_matching::TokenizedString;

using Positions = std::vector<local_search_service::Position>;
using TokenizedStringWithId =
    std::pair<std::string, std::unique_ptr<TokenizedString>>;

void TokenizeSearchTags(const std::vector<Content>& contents,
                        std::vector<TokenizedStringWithId>* tokenized) {
  DCHECK(tokenized);

  for (const auto& content : contents) {
    tokenized->push_back(std::make_pair(
        content.id, std::make_unique<TokenizedString>(content.content)));
  }
}

// Returns whether a given item with |search_tags| is relevant to |query| using
// fuzzy string matching.
bool IsItemRelevant(const TokenizedString& query,
                    const std::vector<TokenizedStringWithId>& search_tags,
                    double relevance_threshold,
                    double* relevance_score,
                    Positions* positions) {
  DCHECK(relevance_score);
  DCHECK(positions);

  if (search_tags.empty())
    return false;

  for (const auto& tag : search_tags) {
    FuzzyTokenizedStringMatch match;
    if (match.IsRelevant(query, *(tag.second), relevance_threshold,
                         false /* use_prefix_only */,
                         true /* use_weighted_ratio */,
                         false /* use_edit_distance */,
                         0.9 /* partial_match_penalty_rate */, 0.1)) {
      *relevance_score = match.relevance();
      Position position;
      position.content_id = tag.first;
      positions->push_back(position);
      return true;
    }
  }
  return false;
}

// Updates data given |id| and |contents|. If |id| already exists, it will
// overwrite earlier data.
void UpdateData(const std::string& id,
                const std::vector<Content>& contents,
                KeyToTagVector* data) {
  DCHECK(data);
  (*data)[id] = std::vector<TokenizedStringWithId>();
  TokenizeSearchTags(contents, &((*data)[id]));
}

}  // namespace

LinearMapSearch::LinearMapSearch(IndexId index_id, PrefService* local_state)
    : IndexSync(index_id, Backend::kLinearMap, local_state),
      Index(index_id, Backend::kLinearMap) {}

LinearMapSearch::~LinearMapSearch() = default;

uint64_t LinearMapSearch::GetSizeSync() {
  return data_.size();
}

void LinearMapSearch::AddOrUpdateSync(
    const std::vector<local_search_service::Data>& data) {
  for (const auto& item : data) {
    const auto& id = item.id;
    DCHECK(!id.empty());

    UpdateData(id, item.contents, &data_);
  }

  MaybeLogIndexSize();
}

uint32_t LinearMapSearch::DeleteSync(const std::vector<std::string>& ids) {
  uint32_t num_deleted = 0u;
  for (const auto& id : ids) {
    DCHECK(!id.empty());
    num_deleted += data_.erase(id);
  }

  MaybeLogIndexSize();
  return num_deleted;
}

void LinearMapSearch::ClearIndexSync() {
  data_.clear();
}

ResponseStatus LinearMapSearch::FindSync(const base::string16& query,
                                         uint32_t max_results,
                                         std::vector<Result>* results) {
  const base::TimeTicks start = base::TimeTicks::Now();
  DCHECK(results);
  results->clear();

  if (query.empty()) {
    const ResponseStatus status = ResponseStatus::kEmptyQuery;
    MaybeLogSearchResultsStats(status, 0u, base::TimeDelta());
    return status;
  }

  if (data_.empty()) {
    const ResponseStatus status = ResponseStatus::kEmptyIndex;
    MaybeLogSearchResultsStats(status, 0u, base::TimeDelta());
    return status;
  }

  *results = GetSearchResults(query, max_results);

  const base::TimeTicks end = base::TimeTicks::Now();
  const ResponseStatus status = ResponseStatus::kSuccess;
  MaybeLogSearchResultsStats(status, results->size(), end - start);
  return status;
}

void LinearMapSearch::GetSize(GetSizeCallback callback) {
  std::move(callback).Run(data_.size());
}

void LinearMapSearch::AddOrUpdate(const std::vector<Data>& data,
                                  AddOrUpdateCallback callback) {
  // TODO(thanhdng): Add logging to this function once we have the metrics
  // reporter available.
  for (const auto& item : data) {
    const auto& id = item.id;
    DCHECK(!id.empty());
    UpdateData(id, item.contents, &data_);
  }
  std::move(callback).Run();
}

void LinearMapSearch::Delete(const std::vector<std::string>& ids,
                             DeleteCallback callback) {
  uint32_t num_deleted = 0u;
  for (const auto& id : ids) {
    DCHECK(!id.empty());
    num_deleted += data_.erase(id);
  }

  std::move(callback).Run(num_deleted);
}

void LinearMapSearch::UpdateDocuments(const std::vector<Data>& data,
                                      UpdateDocumentsCallback callback) {
  uint32_t num_deleted = 0u;
  for (const auto& item : data) {
    const auto& id = item.id;
    DCHECK(!id.empty());

    if (item.contents.empty()) {
      num_deleted += data_.erase(id);
    } else {
      UpdateData(id, item.contents, &data_);
    }
  }

  std::move(callback).Run(num_deleted);
}

void LinearMapSearch::Find(const base::string16& query,
                           uint32_t max_results,
                           FindCallback callback) {
  if (query.empty()) {
    std::move(callback).Run(ResponseStatus::kEmptyQuery, base::nullopt);
    return;
  }

  if (data_.empty()) {
    std::move(callback).Run(ResponseStatus::kEmptyIndex, base::nullopt);
    return;
  }

  std::vector<Result> results = GetSearchResults(query, max_results);
  std::move(callback).Run(ResponseStatus::kSuccess, std::move(results));
}

void LinearMapSearch::ClearIndex(ClearIndexCallback callback) {
  data_.clear();
  std::move(callback).Run();
}

std::vector<Result> LinearMapSearch::GetSearchResults(
    const base::string16& query,
    uint32_t max_results) const {
  std::vector<Result> results;
  const TokenizedString tokenized_query(query);

  for (const auto& item : data_) {
    double relevance_score = 0.0;
    Positions positions;
    if (IsItemRelevant(tokenized_query, item.second,
                       search_params_.relevance_threshold, &relevance_score,
                       &positions)) {
      Result result;
      result.id = item.first;
      result.score = relevance_score;
      result.positions = positions;
      results.push_back(result);
    }
  }

  std::sort(results.begin(), results.end(), CompareResults);
  if (results.size() > max_results && max_results > 0u) {
    results.resize(max_results);
  }
  return results;
}

}  // namespace local_search_service
}  // namespace chromeos
