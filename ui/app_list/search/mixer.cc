// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search/mixer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"

namespace app_list {

namespace {

// Maximum number of results to show. Ignored if the AppListMixer field trial is
// "Blended".
const size_t kMaxResults = 6;

// The minimum number of results to show, if the AppListMixer field trial is
// "Blended". If this quota is not reached, the per-group limitations are
// removed and we try again. (We may still not reach the minumum, but at least
// we tried.) Ignored if the field trial is off.
const size_t kMinBlendedResults = 6;

const char kAppListMixerFieldTrialName[] = "AppListMixer";
const char kAppListMixerFieldTrialEnabled[] = "Blended";

void UpdateResult(const SearchResult& source, SearchResult* target) {
  target->set_display_type(source.display_type());
  target->set_title(source.title());
  target->set_title_tags(source.title_tags());
  target->set_details(source.details());
  target->set_details_tags(source.details_tags());
}

// Returns true if the "AppListMixer" trial is set to "Blended". This is an
// experiment on the new Mixer logic that allows results from different groups
// to be blended together, rather than stratified.
bool IsBlendedMixerTrialEnabled() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kAppListMixerFieldTrialName);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewAppListMixer)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNewAppListMixer)) {
    return true;
  }

  return group_name == kAppListMixerFieldTrialEnabled;
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

// Used to group relevant providers together for mixing their results.
class Mixer::Group {
 public:
  Group(size_t max_results, double boost, double multiplier)
      : max_results_(max_results), boost_(boost), multiplier_(multiplier) {}
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

        double multiplier = multiplier_;
        double boost = boost_;

        // Recommendations should not be affected by query-to-launch correlation
        // from KnownResults as it causes recommendations to become dominated by
        // previously clicked results. This happens because the recommendation
        // query is the empty string and the clicked results get forever
        // boosted.
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

        results_.push_back(SortData(result, relevance * multiplier + boost));
      }
    }

    std::sort(results_.begin(), results_.end());
  }

  const SortedResults& results() const { return results_; }

  size_t max_results() const { return max_results_; }

 private:
  typedef std::vector<SearchProvider*> Providers;
  const size_t max_results_;
  const double boost_;
  const double multiplier_;

  Providers providers_;  // Not owned.
  SortedResults results_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

Mixer::Mixer(AppListModel::SearchResults* ui_results)
    : ui_results_(ui_results) {
}
Mixer::~Mixer() {
}

size_t Mixer::AddGroup(size_t max_results, double boost, double multiplier) {
  // Only consider |boost| if the AppListMixer field trial is default.
  // Only consider |multiplier| if the AppListMixer field trial is "Blended".
  if (IsBlendedMixerTrialEnabled())
    boost = 0.0;
  else
    multiplier = 1.0;
  groups_.push_back(new Group(max_results, boost, multiplier));
  return groups_.size() - 1;
}

size_t Mixer::AddOmniboxGroup(size_t max_results,
                              double boost,
                              double multiplier) {
  // There should not already be an omnibox group.
  DCHECK(!has_omnibox_group_);
  size_t id = AddGroup(max_results, boost, multiplier);
  omnibox_group_ = id;
  has_omnibox_group_ = true;
  return id;
}

void Mixer::AddProviderToGroup(size_t group_id, SearchProvider* provider) {
  groups_[group_id]->AddProvider(provider);
}

void Mixer::MixAndPublish(bool is_voice_query,
                          const KnownResults& known_results) {
  FetchResults(is_voice_query, known_results);

  SortedResults results;

  if (IsBlendedMixerTrialEnabled()) {
    results.reserve(kMinBlendedResults);

    // Add results from each group. Limit to the maximum number of results in
    // each group.
    for (const Group* group : groups_) {
      size_t num_results =
          std::min(group->results().size(), group->max_results());
      results.insert(results.end(), group->results().begin(),
                     group->results().begin() + num_results);
    }
    // Remove results with duplicate IDs before sorting. If two providers give a
    // result with the same ID, the result from the provider with the *lower
    // group number* will be kept (e.g., an app result takes priority over a web
    // store result with the same ID).
    RemoveDuplicates(&results);
    std::sort(results.begin(), results.end());

    if (results.size() < kMinBlendedResults) {
      size_t original_size = results.size();
      // We didn't get enough results. Insert all the results again, and this
      // time, do not limit the maximum number of results from each group. (This
      // will result in duplicates, which will be removed by RemoveDuplicates.)
      for (const Group* group : groups_) {
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
  } else {
    results.reserve(kMaxResults);

    // Add results from non-omnibox groups first. Limit to the maximum number of
    // results in each group.
    for (size_t i = 0; i < groups_.size(); ++i) {
      if (!has_omnibox_group_ || i != omnibox_group_) {
        const Group& group = *groups_[i];
        size_t num_results =
            std::min(group.results().size(), group.max_results());
        results.insert(results.end(), group.results().begin(),
                       group.results().begin() + num_results);
      }
    }

    // Collapse duplicate apps from local and web store.
    RemoveDuplicates(&results);

    // Fill the remaining slots with omnibox results. Always add at least one
    // omnibox result (even if there are no more slots; if we over-fill the
    // vector, the web store and people results will be removed in a later
    // step). Note: max_results() is ignored for the omnibox group.
    if (has_omnibox_group_) {
      CHECK_LT(omnibox_group_, groups_.size());
      const Group& omnibox_group = *groups_[omnibox_group_];
      const size_t omnibox_results = std::min(
          omnibox_group.results().size(),
          results.size() < kMaxResults ? kMaxResults - results.size() : 1);
      results.insert(results.end(), omnibox_group.results().begin(),
                     omnibox_group.results().begin() + omnibox_results);
    }

    std::sort(results.begin(), results.end());
    RemoveDuplicates(&results);
    if (results.size() > kMaxResults)
      results.resize(kMaxResults);
  }

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
  for (auto* group : groups_)
    group->FetchResults(is_voice_query, known_results);
}

}  // namespace app_list
