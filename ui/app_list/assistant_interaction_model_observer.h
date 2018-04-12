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

struct RecognizedSpeech;

// An observer which receives notification of changes to an Assistant
// interaction.
class AssistantInteractionModelObserver {
 public:
  // Invoked when the card associated with the interaction is changed.
  virtual void OnCardChanged(const std::string& html) {}

  // Invoked when the card associated with the interaction is cleared.
  virtual void OnCardCleared() {}

  // Invoked when recognized speech associated with the interaction is changed.
  virtual void OnRecognizedSpeechChanged(
      const RecognizedSpeech& recognized_speech) {}

  // Invoked when recognized speech associated with the interaction is cleared.
  virtual void OnRecognizedSpeechCleared() {}

  // Invoked when the specified |suggestions| are added to the associated
  // interaction.
  virtual void OnSuggestionsAdded(const std::vector<std::string>& suggestions) {
  }

  // Invoked when all suggestions associated with the interaction are cleared.
  virtual void OnSuggestionsCleared() {}

  // Invoked the specified |text| is added to the associated interaction.
  virtual void OnTextAdded(const std::string& text) {}

  // Invoked all text associated with the interaction is cleared.
  virtual void OnTextCleared() {}

 protected:
  AssistantInteractionModelObserver() = default;
  virtual ~AssistantInteractionModelObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModelObserver);
};

}  // namespace app_list

#endif  // UI_APP_LIST_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
