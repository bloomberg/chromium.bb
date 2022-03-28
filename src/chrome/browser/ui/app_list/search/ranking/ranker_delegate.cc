// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/ui/app_list/search/ranking/answer_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/best_match_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/continue_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/filtering_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/ftrl_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/mrfu_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/query_highlighter.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results.pb.h"
#include "chrome/browser/ui/app_list/search/ranking/removed_results_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/score_normalizing_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/util/score_normalizer.h"
#include "chrome/browser/ui/app_list/search/util/score_normalizer.pb.h"

namespace app_list {
namespace {

// A standard write delay used for protos without time-sensitive writes. This is
// intended to be slightly longer than the longest conceivable latency for a
// search.
constexpr base::TimeDelta kStandardWriteDelay = base::Seconds(3);

// No write delay for protos with time-sensitive writes.
constexpr base::TimeDelta kNoWriteDelay = base::Seconds(0);

}  // namespace

RankerDelegate::RankerDelegate(Profile* profile, SearchController* controller) {
  // Score normalization parameters:
  ScoreNormalizer::Params score_normalizer_params;
  // Change this version number when changing the number of bins below.
  score_normalizer_params.version = 1;
  // The maximum number of buckets the score normalizer discretizes result
  // scores into.
  score_normalizer_params.max_bins = 5;

  // Result ranking parameters.
  // TODO(crbug.com/1199206): These need tweaking.
  FtrlOptimizer::Params ftrl_result_params;
  ftrl_result_params.alpha = 0.1;
  ftrl_result_params.gamma = 0.1;
  ftrl_result_params.num_experts = 2u;

  MrfuCache::Params mrfu_result_params;
  mrfu_result_params.half_life = 20.0f;
  mrfu_result_params.boost_factor = 2.9f;
  mrfu_result_params.max_items = 200u;

  // Category ranking parameters.
  // TODO(crbug.com/1199206): These need tweaking.
  FtrlOptimizer::Params ftrl_category_params;
  ftrl_category_params.alpha = 0.1;
  ftrl_category_params.gamma = 0.1;
  ftrl_category_params.num_experts = 2u;

  MrfuCache::Params mrfu_category_params;
  mrfu_category_params.half_life = 20.0f;
  mrfu_category_params.boost_factor = 7.0f;
  mrfu_category_params.max_items = 20u;

  const auto state_dir = RankerStateDirectory(profile);

  // 1. Result pre-processing. These filter or modify search results but don't
  // change their scores.
  AddRanker(std::make_unique<QueryHighlighter>());
  AddRanker(std::make_unique<ContinueRanker>());
  AddRanker(std::make_unique<FilteringRanker>());
  AddRanker(std::make_unique<RemovedResultsRanker>(
      PersistentProto<RemovedResultsProto>(
          state_dir.AppendASCII("removed_results.pb"), kNoWriteDelay)));

  // 2. Score normalization, a precursor to other ranking.
  AddRanker(std::make_unique<ScoreNormalizingRanker>(
      score_normalizer_params,
      PersistentProto<ScoreNormalizerProto>(
          state_dir.AppendASCII("score_norm.pb"), kStandardWriteDelay)));

  // 3. Ranking for results.
  // 3a. Most-frequently-recently-used (MRFU) ranking.
  auto mrfu_ranker = std::make_unique<MrfuResultRanker>(
      mrfu_result_params,
      PersistentProto<MrfuCacheProto>(state_dir.AppendASCII("mrfu_results.pb"),
                                      kStandardWriteDelay));
  AddRanker(std::move(mrfu_ranker));

  // 3b. Ensembling between MRFU and normalized score ranking.
  auto ftrl_ranker = std::make_unique<FtrlRanker>(
      FtrlRanker::RankingKind::kResults, ftrl_result_params,
      PersistentProto<FtrlOptimizerProto>(
          state_dir.AppendASCII("ftrl_results.pb"), kStandardWriteDelay));
  ftrl_ranker->AddExpert(std::make_unique<ResultScoringShim>(
      ResultScoringShim::ScoringMember::kNormalizedRelevance));
  ftrl_ranker->AddExpert(std::make_unique<ResultScoringShim>(
      ResultScoringShim::ScoringMember::kMrfuResultScore));
  AddRanker(std::move(ftrl_ranker));

  // 4. Ranking for categories.
  std::string category_ranking = base::GetFieldTrialParamValueByFeature(
      ash::features::kProductivityLauncher, "category_ranking");
  if (category_ranking == "ftrl") {
    auto category_ranker = std::make_unique<FtrlRanker>(
        FtrlRanker::RankingKind::kCategories, ftrl_category_params,
        PersistentProto<FtrlOptimizerProto>(
            state_dir.AppendASCII("ftrl_categories.pb"), kStandardWriteDelay));
    category_ranker->AddExpert(std::make_unique<MrfuCategoryRanker>(
        mrfu_category_params,
        PersistentProto<MrfuCacheProto>(
            state_dir.AppendASCII("mrfu_categories.pb"), kStandardWriteDelay)));
    category_ranker->AddExpert(std::make_unique<BestResultCategoryRanker>());
    AddRanker(std::move(category_ranker));
  } else if (category_ranking == "score") {
    AddRanker(std::make_unique<BestResultCategoryRanker>());
  } else {
    // == "usage" and any other mis-set value.
    AddRanker(std::make_unique<MrfuCategoryRanker>(
        mrfu_category_params,
        PersistentProto<MrfuCacheProto>(
            state_dir.AppendASCII("mrfu_categories.pb"), kStandardWriteDelay)));
  }

  // 5. Result post-processing.
  // Nb. the best match ranker relies on score normalization, and the answer
  // ranker relies on the best match ranker.
  AddRanker(std::make_unique<BestMatchRanker>());
  AddRanker(std::make_unique<AnswerRanker>());
}

RankerDelegate::~RankerDelegate() {}

void RankerDelegate::Start(const std::u16string& query,
                           ResultsMap& results,
                           CategoriesList& categories) {
  for (auto& ranker : rankers_)
    ranker->Start(query, results, categories);
}

void RankerDelegate::UpdateResultRanks(ResultsMap& results,
                                       ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->UpdateResultRanks(results, provider);
}

void RankerDelegate::UpdateCategoryRanks(const ResultsMap& results,
                                         CategoriesList& categories,
                                         ProviderType provider) {
  for (auto& ranker : rankers_)
    ranker->UpdateCategoryRanks(results, categories, provider);
}

void RankerDelegate::Train(const LaunchData& launch) {
  for (auto& ranker : rankers_)
    ranker->Train(launch);
}

void RankerDelegate::Remove(ChromeSearchResult* result) {
  for (auto& ranker : rankers_)
    ranker->Remove(result);
}

void RankerDelegate::AddRanker(std::unique_ptr<Ranker> ranker) {
  rankers_.emplace_back(std::move(ranker));
}

void RankerDelegate::OnBurnInPeriodElapsed() {
  for (auto& ranker : rankers_)
    ranker->OnBurnInPeriodElapsed();
}

}  // namespace app_list
