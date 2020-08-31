// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_OBSERVER_H_

#include <vector>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"

namespace ash {

class AssistantUiElement;

// A checked observer which receives Assistant response events.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantResponseObserver
    : public base::CheckedObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  // Invoked when the specified |ui_element| is added to the response.
  virtual void OnUiElementAdded(const AssistantUiElement* ui_element) {}

  // Invoked when the specified |suggestions| are added to the response.
  virtual void OnSuggestionsAdded(
      const std::vector<const AssistantSuggestion*>& suggestions) {}

 protected:
  AssistantResponseObserver() = default;
  ~AssistantResponseObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_OBSERVER_H_
