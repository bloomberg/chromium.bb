// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/answer_ranker.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"

namespace app_list {
namespace {

// Returns the provider type's priority. A higher value indicates higher
// priority. Providers that are never used for answers will have 0 priority.
bool GetPriority(ProviderType type) {
  switch (type) {
    case ProviderType::kOmnibox:
      return 2;
    case ProviderType::kKeyboardShortcut:
      return 1;
    default:
      return 0;
  }
}

// If there are any Omnibox answers, returns the highest scoring one. If not,
// returns nullptr.
ChromeSearchResult* GetOmniboxCandidate(Results& results) {
  ChromeSearchResult* top_answer = nullptr;
  double top_score = 0.0;
  for (const auto& result : results) {
    if (result->display_type() != DisplayType::kAnswerCard)
      continue;

    const double score = result->relevance();
    if (!top_answer || score > top_score) {
      top_answer = result.get();
      top_score = score;
    }
  }
  return top_answer;
}

// Returns the Shortcut best match as long as there is only one. Otherwise,
// returns nullptr.
ChromeSearchResult* GetShortcutCandidate(Results& results) {
  ChromeSearchResult* best_shortcut = nullptr;
  for (auto& result : results) {
    if (!result->best_match())
      continue;

    if (best_shortcut) {
      // A best match shortcut has already been found, so there are at least
      // two and neither should be promoted to Answer Card.
      return nullptr;
    }
    best_shortcut = result.get();
  }
  return best_shortcut;
}

}  // namespace

AnswerRanker::AnswerRanker() = default;

AnswerRanker::~AnswerRanker() = default;

void AnswerRanker::Start(const std::u16string& query,
                         ResultsMap& results,
                         CategoriesList& categories) {
  burn_in_elapsed_ = false;
  chosen_answer_ = nullptr;
}

void AnswerRanker::UpdateResultRanks(ResultsMap& results,
                                     ProviderType provider) {
  if (GetPriority(provider) == 0)
    return;

  const auto it = results.find(provider);
  DCHECK(it != results.end());
  auto& new_results = it->second;

  // Omnibox answers should not be displayed unless they are selected, so filter
  // them out by default. If an Omnibox answer is selected later, it will be
  // un-filtered then.
  if (provider == ProviderType::kOmnibox) {
    for (auto& result : new_results) {
      if (result->display_type() == DisplayType::kAnswerCard) {
        result->scoring().filter = true;
      }
    }
  }

  // Don't change a selected answer after the burn-in period has elapsed. This
  // includes ensuring that the answer is re-selected.
  if (burn_in_elapsed_ && chosen_answer_) {
    if (chosen_answer_->result_type() == provider) {
      PromoteChosenAnswer();
    }
    return;
  }

  // Don't make any changes if the chosen answer has higher priority than the
  // current provider.
  if (chosen_answer_ &&
      GetPriority(provider) < GetPriority(chosen_answer_->result_type())) {
    return;
  }

  // Finally, choose a new candidate from the current provider if one exists.
  ChromeSearchResult* new_answer = nullptr;
  switch (provider) {
    case ProviderType::kOmnibox:
      new_answer = GetOmniboxCandidate(new_results);
      break;
    case ProviderType::kKeyboardShortcut:
      new_answer = GetShortcutCandidate(new_results);
      break;
    default:
      return;
  }
  if (new_answer)
    chosen_answer_ = new_answer->GetWeakPtr();

  if (burn_in_elapsed_)
    PromoteChosenAnswer();
}

void AnswerRanker::OnBurnInPeriodElapsed() {
  burn_in_elapsed_ = true;
  PromoteChosenAnswer();
}

void AnswerRanker::PromoteChosenAnswer() {
  if (!chosen_answer_)
    return;

  chosen_answer_->SetDisplayType(DisplayType::kAnswerCard);
  chosen_answer_->SetIconDimension(GetAnswerCardIconDimension());
  chosen_answer_->scoring().filter = false;
}

}  // namespace app_list
