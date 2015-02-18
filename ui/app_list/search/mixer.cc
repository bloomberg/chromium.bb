// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search/mixer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"

namespace app_list {

namespace {

// Maximum number of results to show.
const size_t kMaxResults = 6;
const size_t kMaxMainGroupResults = 4;
const size_t kMaxWebstoreResults = 2;
const size_t kMaxPeopleResults = 2;
const size_t kMaxSuggestionsResults = 6;

// A value to indicate no max number of results limit.
const size_t kNoMaxResultsLimit = 0;

void UpdateResult(const SearchResult& source, SearchResult* target) {
  target->set_display_type(source.display_type());
  target->set_title(source.title());
  target->set_title_tags(source.title_tags());
  target->set_details(source.details());
  target->set_details_tags(source.details_tags());
}

}  // namespace

Mixer::SortData::SortData() : result(NULL), score(0.0) {
}

Mixer::SortData::SortData(SearchResult* result, double score)
    : result(result), score(score) {
}

bool Mixer::SortData::operator<(const SortData& other) const {
  // This data precedes (less than) |other| if it has higher score.
  return score > other.score;
}

// Used to group relevant providers together fox mixing their results.
class Mixer::Group {
 public:
  Group(size_t max_results, double boost)
      : max_results_(max_results), boost_(boost) {}
  ~Group() {}

  void AddProvider(SearchProvider* provider) { providers_.push_back(provider); }

  void FetchResults(bool is_voice_query, const KnownResults& known_results) {
    results_.clear();

    for (const SearchProvider* provider : providers_) {
      for (SearchResult* result : provider->results()) {
        DCHECK(!result->id().empty());

        // We cannot rely on providers to give relevance scores in the range
        // [0.0, 1.0] (e.g., PeopleProvider directly gives values from the
        // Google+ API). Clamp to that range.
        double relevance = std::min(std::max(result->relevance(), 0.0), 1.0);

        double boost = boost_;
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

        results_.push_back(SortData(result, relevance + boost));
      }
    }

    std::sort(results_.begin(), results_.end());
    if (max_results_ != kNoMaxResultsLimit && results_.size() > max_results_)
      results_.resize(max_results_);
  }

  const SortedResults& results() const { return results_; }

 private:
  typedef std::vector<SearchProvider*> Providers;
  const size_t max_results_;
  const double boost_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModel::SearchResults* ui_results)
    : ui_results_(ui_results) {
}
Mixer::~Mixer() {
}

void Mixer::Init() {
  groups_[MAIN_GROUP].reset(new Group(kMaxMainGroupResults, 3.0));
  groups_[OMNIBOX_GROUP].reset(new Group(kNoMaxResultsLimit, 2.0));
  groups_[WEBSTORE_GROUP].reset(new Group(kMaxWebstoreResults, 1.0));
  groups_[PEOPLE_GROUP].reset(new Group(kMaxPeopleResults, 0.0));
  groups_[SUGGESTIONS_GROUP].reset(new Group(kMaxSuggestionsResults, 3.0));
}

void Mixer::AddProviderToGroup(GroupId group, SearchProvider* provider) {
  groups_[group]->AddProvider(provider);
}

void Mixer::MixAndPublish(bool is_voice_query,
                          const KnownResults& known_results) {
  FetchResults(is_voice_query, known_results);

  SortedResults results;
  results.reserve(kMaxResults);

  const Group& main_group = *groups_[MAIN_GROUP];
  const Group& omnibox_group = *groups_[OMNIBOX_GROUP];
  const Group& webstore_group = *groups_[WEBSTORE_GROUP];
  const Group& people_group = *groups_[PEOPLE_GROUP];
  const Group& suggestions_group = *groups_[SUGGESTIONS_GROUP];

  // Adds main group and web store results first.
  results.insert(results.end(), main_group.results().begin(),
                 main_group.results().end());
  results.insert(results.end(), webstore_group.results().begin(),
                 webstore_group.results().end());
  results.insert(results.end(), people_group.results().begin(),
                 people_group.results().end());
  results.insert(results.end(), suggestions_group.results().begin(),
                 suggestions_group.results().end());

  // Collapse duplicate apps from local and web store.
  RemoveDuplicates(&results);

  // Fill the remaining slots with omnibox results. Always add at least one
  // omnibox result (even if there are no more slots; if we over-fill the
  // vector, the web store and people results will be removed in a later step).
  const size_t omnibox_results =
      std::min(omnibox_group.results().size(),
               results.size() < kMaxResults ? kMaxResults - results.size() : 1);
  results.insert(results.end(), omnibox_group.results().begin(),
                 omnibox_group.results().begin() + omnibox_results);

  std::sort(results.begin(), results.end());
  RemoveDuplicates(&results);
  if (results.size() > kMaxResults)
    results.resize(kMaxResults);

  Publish(results, ui_results_);
}

void Mixer::Publish(const SortedResults& new_results,
                    AppListModel::SearchResults* ui_results) {
  typedef std::map<std::string, SearchResult*> IdToResultMap;

  // The following algorithm is used:
  // 1. Transform the |ui_results| list into an unordered map from result ID
  // to item.
  // 2. Use the order of items in |new_results| to build an ordered list. If the
  // result IDs exist in the map, update and use the item in the map and delete
  // it from the map afterwards. Otherwise, clone new items from |new_results|.
  // 3. Delete the objects remaining in the map as they are unused.

  // A map of the items in |ui_results| that takes ownership of the items.
  IdToResultMap ui_results_map;
  for (SearchResult* ui_result : *ui_results)
    ui_results_map[ui_result->id()] = ui_result;
  // We have to erase all results at once so that observers are notified with
  // meaningful indexes.
  ui_results->RemoveAll();

  // Add items back to |ui_results| in the order of |new_results|.
  for (const SortData& sort_data : new_results) {
    const SearchResult& new_result = *sort_data.result;
    IdToResultMap::const_iterator ui_result_it =
        ui_results_map.find(new_result.id());
    if (ui_result_it != ui_results_map.end()) {
      // Update and use the old result if it exists.
      SearchResult* ui_result = ui_result_it->second;
      UpdateResult(new_result, ui_result);
      ui_result->set_relevance(sort_data.score);

      // |ui_results| takes back ownership from |ui_results_map| here.
      ui_results->Add(ui_result);

      // Remove the item from the map so that it ends up only with unused
      // results.
      ui_results_map.erase(ui_result->id());
    } else {
      scoped_ptr<SearchResult> result_copy = new_result.Duplicate();
      result_copy->set_relevance(sort_data.score);
      // Copy the result from |new_results| otherwise.
      ui_results->Add(result_copy.release());
    }
  }

  // Delete the results remaining in the map as they are not in the new results.
  for (const auto& ui_result : ui_results_map) {
    delete ui_result.second;
  }
}

void Mixer::RemoveDuplicates(SortedResults* results) {
  SortedResults final;
  final.reserve(results->size());

  std::set<std::string> id_set;
  for (const SortData& sort_data : *results) {
    const std::string& id = sort_data.result->id();
    if (id_set.find(id) != id_set.end())
      continue;

    id_set.insert(id);
    final.push_back(sort_data);
  }

  results->swap(final);
}

void Mixer::FetchResults(bool is_voice_query,
                         const KnownResults& known_results) {
  for (const auto& item : groups_)
    item.second->FetchResults(is_voice_query, known_results);
}

}  // namespace app_list
