// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/mixer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {

Mixer::SortData::SortData() : result(nullptr), score(0.0) {}

Mixer::SortData::SortData(ChromeSearchResult* result, double score)
    : result(result), score(score) {}

bool Mixer::SortData::operator<(const SortData& other) const {
  // This data precedes (less than) |other| if it has higher score.
  return score > other.score;
}

// Used to group relevant providers together for mixing their results.
class Mixer::Group {
 public:
  Group(size_t max_results, double multiplier, double boost)
      : max_results_(max_results), multiplier_(multiplier), boost_(boost) {}
  ~Group() {}

  void AddProvider(SearchProvider* provider) {
    providers_.emplace_back(provider);
  }

  void FetchResults() {
    results_.clear();

    for (const SearchProvider* provider : providers_) {
      for (const auto& result : provider->results()) {
        DCHECK(!result->id().empty());

        // We cannot rely on providers to give relevance scores in the range
        // [0.0, 1.0]. Clamp to that range.
        const double relevance =
            std::min(std::max(result->relevance(), 0.0), 1.0);
        double boost = boost_;
        results_.emplace_back(result.get(), relevance * multiplier_ + boost);
      }
    }

    std::sort(results_.begin(), results_.end());
  }

  const SortedResults& results() const { return results_; }

  size_t max_results() const { return max_results_; }

 private:
  typedef std::vector<SearchProvider*> Providers;
  const size_t max_results_;
  const double multiplier_;
  const double boost_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModelUpdater* model_updater)
    : model_updater_(model_updater),
      boost_coefficient_(base::GetFieldTrialParamByFeatureAsDouble(
          app_list_features::kEnableAdaptiveResultRanker,
          "boost_coefficient",
          0.1)) {}
Mixer::~Mixer() = default;

size_t Mixer::AddGroup(size_t max_results, double multiplier, double boost) {
  groups_.push_back(std::make_unique<Group>(max_results, multiplier, boost));
  return groups_.size() - 1;
}

void Mixer::AddProviderToGroup(size_t group_id, SearchProvider* provider) {
  groups_[group_id]->AddProvider(provider);
}

void Mixer::MixAndPublish(size_t num_max_results) {
  FetchResults();

  SortedResults results;
  results.reserve(num_max_results);

  // Add results from each group. Limit to the maximum number of results in each
  // group.
  for (const auto& group : groups_) {
    const size_t num_results =
        std::min(group->results().size(), group->max_results());
    results.insert(results.end(), group->results().begin(),
                   group->results().begin() + num_results);
  }
  // Remove results with duplicate IDs before sorting. If two providers give a
  // result with the same ID, the result from the provider with the *lower group
  // number* will be kept (e.g., an app result takes priority over a web store
  // result with the same ID).
  RemoveDuplicates(&results);

  // Tweak the rankings using the ranker if it exists.
  if (app_list_features::IsAdaptiveResultRankerEnabled() && ranker_) {
    base::flat_map<std::string, float> ranks = ranker_->Rank();

    for (auto& result : results) {
      RankingItemType type = RankingItemTypeFromSearchResult(*result.result);
      const auto& rank_it = ranks.find(std::to_string(static_cast<int>(type)));
      // The ranker only contains entries trained with types relating to files
      // or the omnibox. This means scores for apps, app shortcuts, and answer
      // cards will be unchanged.
      if (rank_it != ranks.end())
        // Ranker scores are guaranteed to be in [0,1]. But, enforce that the
        // result of tweaking does not put the score above 3.0, as that may
        // interfere with apps or answer cards.
        result.score += std::min(rank_it->second * boost_coefficient_, 3.0f);
    }
  }

  std::sort(results.begin(), results.end());

  const size_t original_size = results.size();
  if (original_size < num_max_results) {
    // We didn't get enough results. Insert all the results again, and this
    // time, do not limit the maximum number of results from each group. (This
    // will result in duplicates, which will be removed by RemoveDuplicates.)
    for (const auto& group : groups_) {
      results.insert(results.end(), group->results().begin(),
                     group->results().end());
    }
    RemoveDuplicates(&results);
    // Sort just the newly added results. This ensures that, for example, if
    // there are 6 Omnibox results (score = 0.8) and 1 People result (score =
    // 0.4) that the People result will be 5th, not 7th, because the Omnibox
    // group has a soft maximum of 4 results. (Otherwise, the People result
    // would not be seen at all once the result list is truncated.)
    std::sort(results.begin() + original_size, results.end());
  }

  std::vector<ChromeSearchResult*> new_results;
  for (const SortData& sort_data : results) {
    sort_data.result->SetDisplayScore(sort_data.score);
    new_results.push_back(sort_data.result);
  }
  model_updater_->PublishSearchResults(new_results);
}

void Mixer::RemoveDuplicates(SortedResults* results) {
  SortedResults final;
  final.reserve(results->size());

  std::set<std::string> id_set;
  for (const SortData& sort_data : *results) {
    if (!id_set.insert(sort_data.result->id()).second)
      continue;

    final.emplace_back(sort_data);
  }

  results->swap(final);
}

void Mixer::FetchResults() {
  for (const auto& group : groups_)
    group->FetchResults();
}

void Mixer::SetRecurrenceRanker(std::unique_ptr<RecurrenceRanker> ranker) {
  ranker_ = std::move(ranker);
}

void Mixer::Train(const std::string& id, RankingItemType type) {
  if (!ranker_)
    return;

  if (type == RankingItemType::kFile ||
      type == RankingItemType::kOmniboxGeneric ||
      type == RankingItemType::kOmniboxBookmark ||
      type == RankingItemType::kOmniboxDocument ||
      type == RankingItemType::kOmniboxHistory ||
      type == RankingItemType::kOmniboxSearch) {
    ranker_->Record(std::to_string(static_cast<int>(type)));
  }
}

}  // namespace app_list
