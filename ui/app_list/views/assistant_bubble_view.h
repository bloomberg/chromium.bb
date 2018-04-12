// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_ASSISTANT_BUBBLE_VIEW_H_
#define UI_APP_LIST_VIEWS_ASSISTANT_BUBBLE_VIEW_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/app_list/assistant_interaction_model_observer.h"
#include "ui/views/view.h"

namespace app_list {

class AssistantInteractionModel;

namespace {
class InteractionContainer;
class SuggestionsContainer;
class TextContainer;
}  // namespace

class AssistantBubbleView : public views::View,
                            public AssistantInteractionModelObserver {
 public:
  explicit AssistantBubbleView(
      AssistantInteractionModel* assistant_interaction_model);
  ~AssistantBubbleView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnCardChanged(const std::string& html) override;
  void OnCardCleared() override;
  void OnRecognizedSpeechChanged(
      const RecognizedSpeech& recognized_speech) override;
  void OnRecognizedSpeechCleared() override;
  void OnSuggestionsAdded(const std::vector<std::string>& suggestions) override;
  void OnSuggestionsCleared() override;
  void OnTextAdded(const std::string& text) override;
  void OnTextCleared() override;

 private:
  void InitLayout();

  // Owned by AshAssistantController.
  AssistantInteractionModel* assistant_interaction_model_;
  InteractionContainer* interaction_container_;  // Owned by view hierarchy.
  TextContainer* text_container_;                // Owned by view hierarchy.
  views::View* card_container_;                  // Owned by view hierarchy.
  SuggestionsContainer* suggestions_container_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_ASSISTANT_BUBBLE_VIEW_H_
