// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_H_
#define UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

// TODO(b/77637813): Remove interface abstraction when removing Assistant from
// the launcher.
namespace app_list {

class AssistantInteractionModelObserver;
class AssistantUiElement;

// Models the state of the query. For a text query, only the high confidence
// text portion will be populated. At start of a voice query, both the high and
// low confidence text portions will be empty. As speech recognition continues,
// the low confidence portion will become non-empty. As speech recognition
// improves, both the high and low confidence portions of the query will be
// non-empty. When speech is fully recognized, only the high confidence portion
// will be populated.
struct Query {
  // High confidence portion of the query.
  std::string high_confidence_text;
  // Low confidence portion of the query.
  std::string low_confidence_text;
};

// Models the Assistant interaction. This includes query state, state of speech
// recognition, as well as renderable AssistantUiElements and suggestions.
class AssistantInteractionModel {
 public:
  // Adds/removes the specified interaction model |observer|.
  virtual void AddObserver(AssistantInteractionModelObserver* observer) = 0;
  virtual void RemoveObserver(AssistantInteractionModelObserver* observer) = 0;

  // Resets the interaction to its initial state.
  virtual void ClearInteraction() = 0;

  // Adds the specified |ui_element| that should be rendered for the
  // interaction.
  virtual void AddUiElement(std::unique_ptr<AssistantUiElement> ui_element) = 0;

  // Clears all UI elements for the interaction.
  virtual void ClearUiElements() = 0;

  // Updates the query state for the interaction.
  virtual void SetQuery(const Query& query) = 0;

  // Clears query state for the interaction.
  virtual void ClearQuery() = 0;

  // Adds the specified |suggestions| that should be rendered for the
  // interaction.
  virtual void AddSuggestions(const std::vector<std::string>& suggestions) = 0;

  // Clears all suggestions for the interaction.
  virtual void ClearSuggestions() = 0;

 protected:
  AssistantInteractionModel() = default;
  virtual ~AssistantInteractionModel() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_H_
