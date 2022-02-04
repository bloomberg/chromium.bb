// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_impl_new.h"

#include <algorithm>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/metrics/metrics_hashes.h"
#include "base/sequence_token.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/common/string_util.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/search_features.h"
#include "chrome/browser/ui/app_list/search/search_metrics_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "components/metrics/structured/structured_mojo_events.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {
namespace {

// TODO(crbug.com/1288636): This is brittle as it relies on knowing exactly
// which providers are sending zero-state results. We should replace it with an
// approach where providers indicate whether a result type is for query search
// or zero-state.
void ClearAllResultsExceptContinue(ResultsMap& results) {
  for (auto it = results.begin(); it != results.end();) {
    if (it->first != ResultType::kZeroStateFile &&
        it->first != ResultType::kZeroStateDrive) {
      it = results.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace

SearchControllerImplNew::SearchControllerImplNew(
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    Profile* profile)
    : profile_(profile),
      ranker_(std::make_unique<RankerDelegate>(profile, this)),
      burnin_period_(::search_features::QuerySearchBurnInPeriodDuration()),
      metrics_observer_(std::make_unique<SearchMetricsObserver>(notifier)),
      model_updater_(model_updater),
      list_controller_(list_controller) {
  DCHECK(app_list_features::IsCategoricalSearchEnabled());
}

SearchControllerImplNew::~SearchControllerImplNew() {}

void SearchControllerImplNew::StartSearch(const std::u16string& query) {
  // For query searches, begin the burn-in timer.
  if (!query.empty()) {
    burn_in_timer_.Start(
        FROM_HERE, burnin_period_,
        base::BindOnce(&SearchControllerImplNew::OnBurnInPeriodElapsed,
                       base::Unretained(this)));
  }

  // Cancel a pending zero-state publish if it exists.
  zero_state_timeout_.Stop();

  // TODO(crbug.com/1199206): We should move this histogram logic somewhere
  // else.
  ash::RecordLauncherIssuedSearchQueryLength(query.length());

  // Clear all search results but preserve zero-state results. On a call to
  // StartSearch, we were previously either in zero-state, or another query
  // search. Handle these two cases differently:
  //
  // a) were in zero-state: publish these changes, so that results from a
  //    previous search aren't shown.
  //
  // b) were in search query: do not publish these changes, so that the
  //    old results stay on screen until the new ones are ready.
  ClearAllResultsExceptContinue(results_);
  if (last_query_.empty())
    Publish();
  for (Observer& observer : observer_list_)
    observer.OnResultsCleared();

  categories_ = CreateAllCategories();
  ranker_->Start(query, results_, categories_);

  burnin_iteration_counter_ = 0;
  ids_to_burnin_iteration_.clear();
  session_start_ = base::Time::Now();
  last_query_ = query;

  // Search all providers.
  //
  // TODO(crbug.com/1288712): The query can be empty if the user has entered and
  // then deleted a query. We should consider whether this should trigger a
  // StartSearch call or not.
  for (const auto& provider : providers_) {
    if (query.empty()) {
      provider->StartZeroState();
    } else {
      provider->Start(query);
    }
  }
}

void SearchControllerImplNew::StartZeroState(base::OnceClosure on_done,
                                             base::TimeDelta timeout) {
  // Cancel a pending search publish if it exists.
  burn_in_timer_.Stop();

  results_.clear();
  // Categories currently are not used by zero-state, but may be required for
  // sorting in SetResults.
  categories_ = CreateAllCategories();
  for (Observer& observer : observer_list_)
    observer.OnResultsCleared();

  last_query_.clear();

  ranker_->Start(std::u16string(), results_, categories_);

  on_zero_state_done_ = std::move(on_done);
  returned_zero_state_blockers_ = 0;
  for (const auto& provider : providers_)
    provider->StartZeroState();

  zero_state_timeout_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&SearchControllerImplNew::OnZeroStateTimedOut,
                     base::Unretained(this)));
}

void SearchControllerImplNew::OnZeroStateTimedOut() {
  // This will be nullopt if all zero-state blocking providers have returned. If
  // it isn't, publish whatever results have been returned.
  if (on_zero_state_done_.has_value()) {
    Publish();
    std::move(on_zero_state_done_.value()).Run();
    on_zero_state_done_.reset();
  }
}

void SearchControllerImplNew::OnBurnInPeriodElapsed() {
  ranker_->OnBurnInPeriodElapsed();
  Publish();
}

void SearchControllerImplNew::OpenResult(ChromeSearchResult* result,
                                         int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  // Log the length of the last query that led to the clicked result.
  // TODO(crbug.com/1199206): This histogram logic should be moved somewhere
  // else.
  ash::RecordLauncherClickedSearchQueryLength(last_query_.length());

  const bool dismiss_view_on_open = result->dismiss_view_on_open();

  // Open() may cause |result| to be deleted.
  result->Open(event_flags);

  // Launching apps can take some time. It looks nicer to eagerly dismiss the
  // app list if |result| permits it. Do not close app list for home launcher.
  if (dismiss_view_on_open &&
      (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode())) {
    list_controller_->DismissView();
  }
}

void SearchControllerImplNew::InvokeResultAction(
    ChromeSearchResult* result,
    ash::SearchResultActionType action) {
  if (!result)
    return;

  // In the general case, actions are forwarded to the RemovedResultsRanker.
  // Currently only "remove" actions are supported (and not e.g. "append"
  // actions).
  //
  // In the special case, actions are delegated to the result itself. This is
  // when, for example, supported actions can be handled by a provider backend,
  // as is the case with some actions for some Omnibox results. At the moment we
  // are temporarily handling Omnibox result removal requests in the general
  // case, using RemovedResultsRanker, because the omnibox autocomplete
  // controller supports removal of zero-state results but not of non-zero state
  // results.
  //
  // TODO(crbug.com/1272361): Call result->InvokeAction(action) for all Omnibox
  // action requests, once the autocomplete controller supports removal of
  // non-zero state results.
  if (action == ash::SearchResultActionType::kRemove) {
    ranker_->Remove(result);
  } else if (result->result_type() == ash::AppListSearchResultType::kOmnibox) {
    result->InvokeAction(action);
  }
}

size_t SearchControllerImplNew::AddGroup(size_t max_results) {
  // Unused.
  return 0ul;
}

void SearchControllerImplNew::AddProvider(
    size_t group_id,
    std::unique_ptr<SearchProvider> provider) {
  if (provider->ShouldBlockZeroState())
    ++total_zero_state_blockers_;
  provider->set_controller(this);
  providers_.emplace_back(std::move(provider));
}

void SearchControllerImplNew::SetResults(const SearchProvider* provider,
                                         Results results) {
  // Re-post onto the UI sequence if not called from there.
  auto ui_thread = content::GetUIThreadTaskRunner({});
  if (!ui_thread->RunsTasksInCurrentSequence()) {
    ui_thread->PostTask(
        FROM_HERE,
        base::BindOnce(&SearchControllerImplNew::SetResults,
                       base::Unretained(this), provider, std::move(results)));
    return;
  }

  results_[provider->ResultType()] = std::move(results);
  if (last_query_.empty()) {
    SetZeroStateResults(provider);
  } else {
    SetSearchResults(provider);
  }
}

void SearchControllerImplNew::SetSearchResults(const SearchProvider* provider) {
  Rank(provider->ResultType());

  // From here below is logic concerning the burn-in period.
  const bool is_post_burnin =
      base::Time::Now() - session_start_ > burnin_period_;
  if (is_post_burnin)
    ++burnin_iteration_counter_;

  // Record the burn-in iteration number for categories we are seeing for the
  // first time in this search.
  base::flat_set<Category> updated_categories;
  for (const auto& result : results_[provider->ResultType()])
    updated_categories.insert(result->category());
  for (auto& category : categories_) {
    const auto it = updated_categories.find(category.category);
    if (it != updated_categories.end() && category.burnin_iteration == -1)
      category.burnin_iteration = burnin_iteration_counter_;
  }

  // Record-keeping for the burn-in iteration number of individual results.
  const auto it = results_.find(provider->ResultType());
  DCHECK(it != results_.end());

  for (const auto& result : it->second) {
    const std::string result_id = result->id();
    if (ids_to_burnin_iteration_.find(result_id) !=
        ids_to_burnin_iteration_.end()) {
      // Result has been seen before. Set burnin_iteration, since the result
      // object has changed since last seen.
      result->scoring().burnin_iteration = ids_to_burnin_iteration_[result_id];
    } else {
      result->scoring().burnin_iteration = burnin_iteration_counter_;
      ids_to_burnin_iteration_[result_id] = burnin_iteration_counter_;
    }
  }

  // If the burn-in period has not yet elapsed, don't call Publish here. This
  // case is covered by a call scheduled from within Start().
  if (is_post_burnin)
    Publish();
}

void SearchControllerImplNew::SetZeroStateResults(
    const SearchProvider* provider) {
  Rank(provider->ResultType());

  if (provider->ShouldBlockZeroState())
    ++returned_zero_state_blockers_;

  if (!on_zero_state_done_) {
    // Zero-state has been unblocked, publish immediately.
    Publish();
  } else if (returned_zero_state_blockers_ == total_zero_state_blockers_) {
    // All zero-state blockers have returned. Publish everything received so
    // far, and trigger the on-done callback.
    Publish();
    std::move(on_zero_state_done_.value()).Run();
    on_zero_state_done_.reset();
  }
}

void SearchControllerImplNew::Rank(ash::AppListSearchResultType provider_type) {
  DCHECK(ranker_);
  if (results_.empty()) {
    // Happens if the burn-in period has elapsed without any results having been
    // received from providers. Return early.
    return;
  }

  if (disable_ranking_for_test_)
    return;

  // Update ranking of all results and categories for this provider. This
  // ordering is important, as result scores may affect category scores.
  ranker_->UpdateResultRanks(results_, provider_type);
  ranker_->UpdateCategoryRanks(results_, categories_, provider_type);
}

void SearchControllerImplNew::Publish() {
  // Sort categories first by burn-in iteration number, then by score.
  std::sort(categories_.begin(), categories_.end(),
            [](const auto& a, const auto& b) {
              const int a_burnin = a.burnin_iteration;
              const int b_burnin = b.burnin_iteration;
              if (a_burnin != b_burnin) {
                // Sort order: 0, 1, 2, 3, ... then -1.
                // The effect of this is to sort by arrival order, with unseen
                // categories ranked last.
                // N.B. (a ^ b) < 0 checks for opposite sign.
                return (a_burnin ^ b_burnin) < 0 ? a_burnin > b_burnin
                                                 : a_burnin < b_burnin;
              } else {
                return a.score > b.score;
              }
            });
  // Create a vector of category enums in display order.
  std::vector<Category> category_enums;
  for (const auto& category : categories_)
    category_enums.push_back(category.category);

  // Compile a single list of results and sort first by their category with best
  // match first, then by burn-in iteration number, and finally by relevance.
  std::vector<ChromeSearchResult*> all_results;
  for (const auto& type_results : results_) {
    for (const auto& result : type_results.second) {
      // TODO(crbug.com/1199206): Category-based search combines apps into the
      // results list, so redirect any kTile results to kList before updating
      // the UI. Once SearchControllerImplNew is the only search controller,
      // this can be removed and all results can be created as kList.
      if (result->display_type() == ash::SearchResultDisplayType::kTile) {
        result->SetDisplayType(ash::SearchResultDisplayType::kList);
      }

      double score = result->scoring().FinalScore();

      // Filter out results with negative relevance, which is the rankers'
      // signal that a result should not be displayed at all.
      if (score < 0.0)
        continue;

      // The display score is the result's final score before display. It is
      // used for sorting below, and may be used directly in ash.
      result->SetDisplayScore(score);
      all_results.push_back(result.get());
    }
  }

  std::sort(
      all_results.begin(), all_results.end(),
      [&](const ChromeSearchResult* a, const ChromeSearchResult* b) {
        const int a_best_match_rank = a->scoring().best_match_rank;
        const int b_best_match_rank = b->scoring().best_match_rank;
        if (a_best_match_rank != b_best_match_rank) {
          // First, sort by best match. All best matches are brought to
          // the front of the list and ordered by best_match_rank.
          //
          // The following gives sort order:
          //    0, 1, 2, ... (best matches) then -1 (non-best matches).
          // N.B. (a ^ b) < 0 checks for opposite sign.
          return (a_best_match_rank ^ b_best_match_rank) < 0
                     ? a_best_match_rank > b_best_match_rank
                     : a_best_match_rank < b_best_match_rank;
        } else if (a_best_match_rank == -1 && a->category() != b->category()) {
          // Next, sort by categories, except for within best match.
          // |categories_| has been sorted above so the first category in
          // |categories_| should be ranked more highly.
          for (const auto& category : categories_) {
            if (category.category == a->category()) {
              return true;
            } else if (category.category == b->category()) {
              return false;
            }
          }
          // Any category associated with a result should also be present
          // in |categories_|.
          NOTREACHED();
          return false;
        } else if (a->scoring().burnin_iteration !=
                   b->scoring().burnin_iteration) {
          // Next, sort by burn-in iteration number. This has no effect on
          // results which arrive pre-burn-in. For post-burn-in results
          // for a given category, later-arriving results are placed below
          // earlier-arriving results.
          // This happens before sorting on display_score, as a trade-off
          // between ranking accuracy and UX pop-in mitigation.
          return a->scoring().burnin_iteration < b->scoring().burnin_iteration;
        } else {
          // Lastly, sort by display score.
          return a->display_score() > b->display_score();
        }
      });

  if (!observer_list_.empty()) {
    std::vector<const ChromeSearchResult*> observer_results;
    for (auto* result : all_results)
      observer_results.push_back(const_cast<const ChromeSearchResult*>(result));
    for (Observer& observer : observer_list_)
      observer.OnResultsAdded(last_query_, observer_results);
  }

  model_updater_->PublishSearchResults(all_results, category_enums);
}

ChromeSearchResult* SearchControllerImplNew::FindSearchResult(
    const std::string& result_id) {
  for (const auto& provider_results : results_) {
    for (const auto& result : provider_results.second) {
      if (result->id() == result_id)
        return result.get();
    }
  }
  return nullptr;
}

void SearchControllerImplNew::OnSearchResultsImpressionMade(
    const std::u16string& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int launched_index) {
  if (trimmed_query.empty()) {
    // Extract result types for logging.
    std::vector<RankingItemType> result_types;
    for (const auto& result : results) {
      result_types.push_back(
          RankingItemTypeFromSearchResult(*FindSearchResult(result.id)));
    }
  }
}

ChromeSearchResult* SearchControllerImplNew::GetResultByTitleForTest(
    const std::string& title) {
  std::u16string target_title = base::ASCIIToUTF16(title);
  for (const auto& provider_results : results_) {
    for (const auto& result : provider_results.second) {
      if (result->title() == target_title &&
          result->result_type() ==
              ash::AppListSearchResultType::kInstalledApp &&
          !result->is_recommendation()) {
        return result.get();
      }
    }
  }
  return nullptr;
}

int SearchControllerImplNew::GetLastQueryLength() const {
  return last_query_.size();
}

void SearchControllerImplNew::Train(LaunchData&& launch_data) {
  launch_data.query = base::UTF16ToUTF8(last_query_);

  // TODO(crbug.com/1199206): This logging code should move elsewhere.
  if (app_list_features::IsAppListLaunchRecordingEnabled()) {
    // Record a structured metrics event.
    const base::Time now = base::Time::Now();
    base::Time::Exploded now_exploded;
    now.LocalExplode(&now_exploded);

    metrics::structured::events::v2::launcher_usage::LauncherUsage()
        .SetTarget(NormalizeId(launch_data.id))
        .SetApp(last_launched_app_id_)
        .SetSearchQuery(base::UTF16ToUTF8(last_query_))
        .SetSearchQueryLength(last_query_.size())
        .SetProviderType(static_cast<int>(launch_data.ranking_item_type))
        .SetHour(now_exploded.hour)
        .SetScore(launch_data.score)
        .Record();

    // Only record the last launched app if the hashed logging feature flag is
    // enabled, because it is only used by hashed logging.
    if (launch_data.ranking_item_type == RankingItemType::kApp) {
      last_launched_app_id_ = NormalizeId(launch_data.id);
    } else if (launch_data.ranking_item_type ==
               RankingItemType::kArcAppShortcut) {
      last_launched_app_id_ =
          RemoveAppShortcutLabel(NormalizeId(launch_data.id));
    }
  }

  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kLauncherResultEverLaunched,
                                   true);

  // CrOS action recorder.
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat({"SearchResultLaunched-", NormalizeId(launch_data.id)})},
      {{"ResultType", static_cast<int>(launch_data.ranking_item_type)},
       {"Query", static_cast<int>(
                     base::HashMetricName(base::UTF16ToUTF8(last_query_)))}});

  // Train all search result ranking models.
  ranker_->Train(launch_data);
}

void SearchControllerImplNew::AppListShown() {
  for (const auto& provider : providers_)
    provider->AppListShown();
}

void SearchControllerImplNew::ViewClosing() {
  for (const auto& provider : providers_)
    provider->ViewClosing();
}

void SearchControllerImplNew::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchControllerImplNew::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::u16string SearchControllerImplNew::get_query() {
  return last_query_;
}

base::Time SearchControllerImplNew::session_start() {
  return session_start_;
}

void SearchControllerImplNew::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {
  // Unused.
}

void SearchControllerImplNew::disable_ranking_for_test() {
  disable_ranking_for_test_ = true;
}

}  // namespace app_list
