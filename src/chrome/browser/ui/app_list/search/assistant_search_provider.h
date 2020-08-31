// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ASSISTANT_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ASSISTANT_SEARCH_PROVIDER_H_

#include <vector>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

namespace app_list {

// A search provider implementation serving results from Assistant.
// NOTE: This is currently only used to provide a single search result when
// launcher chip integration is enabled from Assistant's internal cache of
// conversation starters.
class AssistantSearchProvider : public SearchProvider,
                                public ash::AssistantStateObserver,
                                public ash::AssistantSuggestionsModelObserver {
 public:
  AssistantSearchProvider();
  AssistantSearchProvider(const AssistantSearchProvider&) = delete;
  AssistantSearchProvider& operator=(const AssistantSearchProvider&) = delete;
  ~AssistantSearchProvider() override;

 private:
  // SearchProvider:
  void Start(const base::string16& query) override {}

  // ash::AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      chromeos::assistant::AssistantAllowedState allowed_state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;

  // ash::AssistantSuggestionsModelObserver:
  void OnConversationStartersChanged(
      const std::vector<const AssistantSuggestion*>& conversation_starters)
      override;

  // Invoke to update results based on current state.
  void UpdateResults();

  // We observe Assistant state.
  ScopedObserver<ash::AssistantStateBase,
                 ash::AssistantStateObserver,
                 &ash::AssistantState::AddObserver,
                 &ash::AssistantState::RemoveObserver>
      state_observer_{this};

  // We observe Assistant suggestions.
  ScopedObserver<ash::AssistantSuggestionsController,
                 ash::AssistantSuggestionsModelObserver,
                 &ash::AssistantSuggestionsController::AddModelObserver,
                 &ash::AssistantSuggestionsController::RemoveModelObserver>
      suggestions_observer_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ASSISTANT_SEARCH_PROVIDER_H_
