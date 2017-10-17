// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search/mixer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"

namespace app_list {

namespace {

void UpdateResult(const SearchResult& source, SearchResult* target) {
  target->set_display_type(source.display_type());
  target->set_title(source.title());
  target->set_title_tags(source.title_tags());
  target->set_details(source.details());
  target->set_details_tags(source.details_tags());
}

const std::string& GetComparableId(const SearchResult& result) {
  return !result.comparable_id().empty() ? result.comparable_id() : result.id();
}

}  // namespace

Mixer::SortData::SortData() : result(nullptr), score(0.0) {}

Mixer::SortData::SortData(SearchResult* result, double score)
    : result(result), score(score) {
}

bool Mixer::SortData::operator<(const SortData& other) const {
  // This data precedes (less than) |other| if it has higher score.
  return score > other.score;
}

// Used to group relevant providers together for mixing their results.
class Mixer::Group {
 public:
  Group(size_t max_results, double multiplier, double boost)
      : max_results_(max_results),
        multiplier_(multiplier),
        boost_(boost),
        is_fullscreen_app_list_enabled_(
            features::IsFullscreenAppListEnabled()) {}
  ~Group() {}

  void AddProvider(SearchProvider* provider) {
    providers_.emplace_back(provider);
  }

  void FetchResults(bool is_voice_query, const KnownResults& known_results) {
    results_.clear();

    for (const SearchProvider* provider : providers_) {
      for (const auto& result : provider->results()) {
        DCHECK(!result->id().empty());

        // We cannot rely on providers to give relevance scores in the range
        // [0.0, 1.0]. Clamp to that range.
        const double relevance =
            std::min(std::max(result->relevance(), 0.0), 1.0);
        double boost = boost_;

        if (!is_fullscreen_app_list_enabled_) {
          // Recommendations should not be affected by query-to-launch
          // correlation from KnownResults as it causes recommendations to
          // become dominated by previously clicked results. This happens
          // because the recommendation query is the empty string and the
          // clicked results get forever boosted.
          if (result->display_type() != SearchResult::DISPLAY_RECOMMENDATION) {
            KnownResults::const_iterator known_it =
                known_results.find(result->id());
            if (known_it != known_results.end()) {
              switch (known_it->second) {
                case PERFECT_PRIMARY:
                  boost = 4.0;
                  break;
                case PREFIX_PRIMARY:
                  boost = 3.75;
                  break;
                case PERFECT_SECONDARY:
                  boost = 3.25;
                  break;
                case PREFIX_SECONDARY:
                  boost = 3.0;
                  break;
                case UNKNOWN_RESULT:
                  NOTREACHED() << "Unknown result in KnownResults?";
                  break;
              }
            }

            // If this is a voice query, voice results receive a massive boost.
            if (is_voice_query && result->voice_result())
              boost += 4.0;
          }
        }

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

  const bool is_fullscreen_app_list_enabled_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModel::SearchResults* ui_results)
    : ui_results_(ui_results) {
}
Mixer::~Mixer() {
}

size_t Mixer::AddGroup(size_t max_results, double multiplier, double boost) {
  groups_.push_back(std::make_unique<Group>(max_results, multiplier, boost));
  return groups_.size() - 1;
}

void Mixer::AddProviderToGroup(size_t group_id, SearchProvider* provider) {
  groups_[group_id]->AddProvider(provider);
}

void Mixer::MixAndPublish(bool is_voice_query,
                          const KnownResults& known_results,
                          size_t num_max_results) {
  FetchResults(is_voice_query, known_results);

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

  Publish(results, ui_results_);
}

void Mixer::Publish(const SortedResults& new_results,
                    AppListModel::SearchResults* ui_results) {
  // The following algorithm is used:
  // 1. Transform the |ui_results| list into an unordered map from result ID
  // to item.
  // 2. Use the order of items in |new_results| to build an ordered list. If the
  // result IDs exist in the map, update and use the item in the map and delete
  // it from the map afterwards. Otherwise, clone new items from |new_results|.
  // 3. Delete the objects remaining in the map as they are unused.

  // We have to erase all results at once so that observers are notified with
  // meaningful indexes.
  auto current_results = ui_results->RemoveAll();
  std::map<std::string, std::unique_ptr<SearchResult>> ui_results_map;
  for (auto& ui_result : current_results)
    ui_results_map[ui_result->id()] = std::move(ui_result);

  // Add items back to |ui_results| in the order of |new_results|.
  for (const SortData& sort_data : new_results) {
    const SearchResult& new_result = *sort_data.result;
    auto ui_result_it = ui_results_map.find(new_result.id());
    if (ui_result_it != ui_results_map.end() &&
        new_result.view() == ui_result_it->second->view()) {
      // Update and use the old result if it exists.
      std::unique_ptr<SearchResult> ui_result = std::move(ui_result_it->second);
      UpdateResult(new_result, ui_result.get());
      ui_result->set_relevance(sort_data.score);

      ui_results->Add(std::move(ui_result));

      // Remove the item from the map so that it ends up only with unused
      // results.
      ui_results_map.erase(ui_result_it);
    } else {
      std::unique_ptr<SearchResult> result_copy = new_result.Duplicate();
      result_copy->set_relevance(sort_data.score);
      // Copy the result from |new_results| otherwise.
      ui_results->Add(std::move(result_copy));
    }
  }

  // Any remaining results in |ui_results_map| will be automatically deleted.
}

void Mixer::RemoveDuplicates(SortedResults* results) {
  SortedResults final;
  final.reserve(results->size());

  std::set<std::string> id_set;
  for (const SortData& sort_data : *results) {
    if (!id_set.insert(GetComparableId(*sort_data.result)).second)
      continue;

    final.emplace_back(sort_data);
  }

  results->swap(final);
}

void Mixer::FetchResults(bool is_voice_query,
                         const KnownResults& known_results) {
  for (const auto& group : groups_)
    group->FetchResults(is_voice_query, known_results);
}

}  // namespace app_list
