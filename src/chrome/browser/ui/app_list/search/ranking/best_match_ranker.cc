// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/best_match_ranker.h"

#include <cmath>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"

namespace app_list {
namespace {

// Returns true if the |type| provider's results should never be a best match.
bool ShouldIgnoreProvider(ProviderType type) {
  switch (type) {
      // Continue providers:
    case ProviderType::kZeroStateFile:
    case ProviderType::kZeroStateDrive:
      // Low-intent providers:
    case ProviderType::kPlayStoreReinstallApp:
    case ProviderType::kPlayStoreApp:
    case ProviderType::kAssistantText:
      // Deprecated providers:
    case ProviderType::kLauncher:
    case ProviderType::kAnswerCard:
      // Suggestion chip results:
    case ProviderType::kFileChip:
    case ProviderType::kDriveChip:
    case ProviderType::kAssistantChip:
      // Internal results:
    case ProviderType::kUnknown:
    case ProviderType::kInternalPrivacyInfo:
      return true;
    default:
      return false;
  }
}

bool ShouldIgnoreResult(const ChromeSearchResult* result) {
  // Ignore:
  // - answer results: ANSWER and CALCULATOR.
  // - 'magnifying glass' results: WEB_QUERY, SEARCH_SUGGEST,
  //   SEARCH_SUGGEST_PERSONALIZED.
  //
  // TODO(crbug.com/1258415): We should have a more robust way of determining
  // omnibox subtypes than using the metrics type.
  return result->metrics_type() == ash::OMNIBOX_ANSWER ||
         result->metrics_type() == ash::OMNIBOX_CALCULATOR ||
         result->metrics_type() == ash::OMNIBOX_WEB_QUERY ||
         result->metrics_type() == ash::OMNIBOX_SEARCH_SUGGEST ||
         result->metrics_type() == ash::OMNIBOX_SUGGEST_PERSONALIZED;
}

}  // namespace

BestMatchRanker::BestMatchRanker() {}

BestMatchRanker::~BestMatchRanker() {}

void BestMatchRanker::Start(const std::u16string& query,
                            ResultsMap& results,
                            CategoriesList& categories) {
  is_pre_burnin_ = true;
  best_matches_.clear();
}

void BestMatchRanker::OnBurnInPeriodElapsed() {
  is_pre_burnin_ = false;
}

void BestMatchRanker::UpdateResultRanks(ResultsMap& results,
                                        ProviderType provider) {
  // Skip results from providers that are never included in the top matches.
  if (ShouldIgnoreProvider(provider))
    return;

  // Remove invalidated weak pointers from best_matches_
  for (auto iter = best_matches_.begin(); iter != best_matches_.end();) {
    if (!(*iter)) {
      // C++11: the return value of erase(iter) is an iterator pointing to the
      // next element in the container.
      iter = best_matches_.erase(iter);
    } else {
      ++iter;
    }
  }

  const bool use_relevance_sort_only = is_pre_burnin_ || best_matches_.empty();

  // Insert into |best_matches_| any new results from this provider that meet
  // the best match threshold.
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  base::flat_set<std::string> seen_ids;
  for (const auto result : best_matches_) {
    seen_ids.insert(result->id());
  }

  for (const auto& result : it->second) {
    if (ShouldIgnoreResult(result.get()))
      continue;

    if (seen_ids.find(result->id()) != seen_ids.end()) {
      // Omnibox provider can return more than once. Don't add duplicate results
      // to best_matches_.
      continue;
    }
    Scoring& scoring = result->scoring();
    if (scoring.BestMatchScore() >= kBestMatchThreshold) {
      best_matches_.push_back(result->GetWeakPtr());
    }
  }

  // If we have no best matches, there is no ranking work to do. Return early.
  if (best_matches_.empty())
    return;

  // Sort best_matches_:
  if (use_relevance_sort_only) {
    // Pre-burn-in, or post-burn-in where we are receiving best matches for the
    // first time, simply sort by relevance.
    std::sort(best_matches_.begin(), best_matches_.end(),
              [](const auto& a, const auto& b) {
                return a->scoring().BestMatchScore() >
                       b->scoring().BestMatchScore();
              });
  } else {
    // Post-burn-in, where best matches exist from previous provider returns,
    // sort by result score but stabilize the top-ranked best match result. We
    // achieve this by first sorting by best match rank, and then sorting a
    // post-fix of the vector by relevance score. This is important because we
    // cannot rely on particular numbered ranks (e.g. 0) being present, as
    // results can technically be destroyed at any time.
    std::sort(best_matches_.begin(), best_matches_.end(),
              [](const auto& a, const auto& b) {
                const int a_rank = a->scoring().best_match_rank;
                const int b_rank = b->scoring().best_match_rank;
                // Sort order: 0, 1, 2, 3, ... then -1.
                // N.B. (a ^ b) < 0 checks for opposite sign.
                return (a_rank ^ b_rank) < 0 ? a_rank > b_rank
                                             : a_rank < b_rank;
              });
    std::sort(best_matches_.begin() + kNumBestMatchesToStabilize,
              best_matches_.end(), [](const auto& a, const auto& b) {
                return a->scoring().normalized_relevance >
                       b->scoring().normalized_relevance;
              });
  }

  // For the first kNumBestMatches of best_matches_, renumber their best match
  // rank.
  for (size_t i = 0; i < std::min(kNumBestMatches, best_matches_.size()); ++i) {
    best_matches_[i]->scoring().best_match_rank = i;
    best_matches_[i]->SetBestMatch(true);
  }

  // For the excess elements of best_matches_, revoke their best match status,
  // and remove them from the vector.
  if (best_matches_.size() > kNumBestMatches) {
    for (size_t i = kNumBestMatches; i < best_matches_.size(); ++i) {
      best_matches_[i]->scoring().best_match_rank = -1;
      best_matches_[i]->SetBestMatch(false);
    }
    best_matches_.resize(kNumBestMatches);
  }
}

}  // namespace app_list
