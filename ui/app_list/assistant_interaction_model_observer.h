// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
#define UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_

#include <string>
#include <vector>

#include "base/macros.h"

// TODO(b/77637813): Move to /ash/assistant/model when pulling Assistant out
// of the launcher.
namespace app_list {

class AssistantUiElement;
struct Query;

// An observer which receives notification of changes to an Assistant
// interaction.
class AssistantInteractionModelObserver {
 public:
  // Invoked when a UI element associated with the interaction is added.
  virtual void OnUiElementAdded(const AssistantUiElement* ui_element) {}

  // Invoked when all UI elements associated with the interaction are cleared.
  virtual void OnUiElementsCleared() {}

  // Invoked when the query associated with the interaction is changed.
  virtual void OnQueryChanged(const Query& query) {}

  // Invoked when the query associated with the interaction is cleared.
  virtual void OnQueryCleared() {}

  // Invoked when the specified |suggestions| are added to the associated
  // interaction.
  virtual void OnSuggestionsAdded(const std::vector<std::string>& suggestions) {
  }

  // Invoked when all suggestions associated with the interaction are cleared.
  virtual void OnSuggestionsCleared() {}

 protected:
  AssistantInteractionModelObserver() = default;
  virtual ~AssistantInteractionModelObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModelObserver);
};

}  // namespace app_list

#endif  // UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
