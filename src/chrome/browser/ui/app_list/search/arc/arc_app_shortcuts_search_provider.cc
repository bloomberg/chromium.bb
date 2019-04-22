// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcut_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"

namespace {

// When ranking with the |QueryBasedAppsRanker| is enabled, this boost is
// added to all shortcuts that the ranker knows about.
constexpr float kDefaultRankerScoreBoost = 0.0f;

// When ranking with the |QueryBasedAppsRanker| is enabled, its scores are
// multiplied by this amount.
constexpr float kDefaultRankerScoreCoefficient = 0.1f;

// Linearly maps |score| to the range [min, max].
// |score| is assumed to be within [0.0, 1.0]; if it's greater than 1.0
// then max is returned; if it's less than 0.0, then min is returned.
// |min| and |max| are assumed to be within [0.0, 1.0].
float ReRange(const float score, const float min, const float max) {
  DCHECK_LT(min, max);
  if (score >= 1.0f)
    return max;
  if (score <= 0.0f)
    return min;

  return min + score * (max - min);
}

}  // namespace

namespace app_list {

ArcAppShortcutsSearchProvider::ArcAppShortcutsSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller,
    AppSearchResultRanker* ranker)
    : max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller),
      ranker_(ranker),
      weak_ptr_factory_(this) {}

ArcAppShortcutsSearchProvider::~ArcAppShortcutsSearchProvider() = default;

void ArcAppShortcutsSearchProvider::Start(const base::string16& query) {
  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppShortcutGlobalQueryItems)
          : nullptr;

  if (!app_instance) {
    ClearResults();
    return;
  }

  // Invalidate the weak ptr to prevent previous callback run.
  weak_ptr_factory_.InvalidateWeakPtrs();
  int max_items = max_results_;
  if (query.empty()) {
    if (ShouldRerankZeroState()) {
      max_items = base::GetFieldTrialParamByFeatureAsInt(
          app_list_features::kEnableZeroStateAppsRanker, "max_items_to_get",
          10);
      app_instance->GetAppShortcutGlobalQueryItems(
          base::UTF16ToUTF8(query), max_items,
          base::BindOnce(
              &ArcAppShortcutsSearchProvider::UpdateRecomendedResults,
              weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    if (ShouldReRankQueryBased()) {
      max_items = base::GetFieldTrialParamByFeatureAsInt(
          app_list_features::kEnableQueryBasedAppsRanker, "max_items_to_get",
          10);
    }
    app_instance->GetAppShortcutGlobalQueryItems(
        base::UTF16ToUTF8(query), max_items,
        base::BindOnce(
            &ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcAppShortcutsSearchProvider::Train(const std::string& id,
                                          RankingItemType type) {
  if (type == RankingItemType::kArcAppShortcut && ranker_ != nullptr)
    ranker_->Train(id);
}

bool ArcAppShortcutsSearchProvider::ShouldRerankZeroState() const {
  return app_list_features::IsZeroStateAppsRankerEnabled() &&
         base::GetFieldTrialParamByFeatureAsBool(
             app_list_features::kEnableZeroStateAppsRanker,
             "rank_arc_app_shortcuts", false) &&
         ranker_ != nullptr;
}

bool ArcAppShortcutsSearchProvider::ShouldReRankQueryBased() const {
  return app_list_features::IsQueryBasedAppsRankerEnabled() &&
         base::GetFieldTrialParamByFeatureAsBool(
             app_list_features::kEnableQueryBasedAppsRanker,
             "rank_arc_app_shortcuts", false) &&
         ranker_ != nullptr;
}

void ArcAppShortcutsSearchProvider::UpdateRecomendedResults(
    std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);

  // Maps app IDs to their score according to |ranker_|
  base::flat_map<std::string, float> ranker_scores;
  SearchProvider::Results search_results;
  for (auto& item : shortcut_items) {
    const std::string app_id =
        arc_prefs->GetAppIdByPackageName(item->package_name.value());
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    // Ignore shortcuts for apps that are not present in the launcher.
    if (!app_info || !app_info->show_in_launcher)
      continue;
    auto result = std::make_unique<ArcAppShortcutSearchResult>(
        std::move(item), profile_, list_controller_,
        true /*is_recommendation*/);
    DCHECK(ShouldRerankZeroState());
    ranker_scores = ranker_->Rank();
    const auto find_in_ranker = ranker_scores.find(result->id());
    // TODO(crbug.com/931149): This logic mimics the logic of
    // AppSearchProvider::UpdateRecommendedResults. Need update to a
    // logic that fits ArcAppShortcut.
    if (find_in_ranker != ranker_scores.end()) {
      // Case 1: If it is recommended by |ranker_|, set the relevance as
      // a score in a range [0.67, 1.0].
      result->set_relevance(ReRange(find_in_ranker->second, 0.67, 1.0));
    } else if (!app_info->install_time.is_null() ||
               !app_info->last_launch_time.is_null()) {
      // Case 2: It it has |install_time| or |last_launch_time|, set the
      // relevance to 0.5.
      result->set_relevance(0.5);
    } else {
      // Case 3: otherwise set relevance to 0.0.
      result->set_relevance(0);
    }
    search_results.emplace_back(std::move(result));
  }
  SwapResults(&search_results);
}

void ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems(
    std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);
  // Maps app IDs to their score according to |ranker_|.
  base::flat_map<std::string, float> ranker_scores;
  float ranker_score_coefficient = kDefaultRankerScoreCoefficient;
  float ranker_score_boost = kDefaultRankerScoreBoost;
  const bool should_rerank = ShouldReRankQueryBased();
  if (should_rerank) {
    ranker_scores = ranker_->Rank();
    ranker_score_coefficient = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableQueryBasedAppsRanker,
        "arc_app_shortcuts_query_coefficient", ranker_score_coefficient);
    ranker_score_boost = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableQueryBasedAppsRanker,
        "arc_app_shortcuts_query_boost", ranker_score_boost);
  }

  SearchProvider::Results search_results;
  for (auto& item : shortcut_items) {
    const std::string app_id =
        arc_prefs->GetAppIdByPackageName(item->package_name.value());
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    // Ignore shortcuts for apps that are not present in the launcher.
    if (!app_info || !app_info->show_in_launcher)
      continue;
    auto result = std::make_unique<ArcAppShortcutSearchResult>(
        std::move(item), profile_, list_controller_,
        false /*is_recommendation*/);
    if (should_rerank) {
      const auto find_in_ranker = ranker_scores.find(result->id());
      if (find_in_ranker != ranker_scores.end()) {
        // TODO(crbug.com/931149): currently, relevance scores for app shortcuts
        // are always 0, but are included here in case this changes. If this
        // changes, review their range and possibly update this formula.
        result->set_relevance(result->relevance() +
                              ranker_score_coefficient *
                                  find_in_ranker->second +
                              ranker_score_boost);
      }
    }
    search_results.emplace_back(std::move(result));
  }
  SwapResults(&search_results);
}

}  // namespace app_list
