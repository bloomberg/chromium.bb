// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_ASSISTANT_BUBBLE_VIEW_H_
#define UI_APP_LIST_VIEWS_ASSISTANT_BUBBLE_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/app_list/assistant_interaction_model_observer.h"
#include "ui/app_list/views/suggestion_chip_view.h"
#include "ui/views/view.h"

namespace app_list {

class AssistantController;

namespace {
class CardContainer;
class InteractionContainer;
class SuggestionsContainer;
class TextContainer;
}  // namespace

class AssistantBubbleView : public views::View,
                            public AssistantInteractionModelObserver,
                            public SuggestionChipListener {
 public:
  explicit AssistantBubbleView(AssistantController* assistant_controller);
  ~AssistantBubbleView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnCardChanged(const std::string& html) override;
  void OnCardCleared() override;
  void OnQueryChanged(const Query& query) override;
  void OnQueryCleared() override;
  void OnSuggestionsAdded(const std::vector<std::string>& suggestions) override;
  void OnSuggestionsCleared() override;
  void OnTextAdded(const std::string& text) override;
  void OnTextCleared() override;

  // SuggestionChipListener:
  void OnSuggestionChipPressed(
      SuggestionChipView* suggestion_chip_view) override;

 private:
  void InitLayout();

  void OnCardReady(const base::UnguessableToken& embed_token);
  void OnReleaseCard();

  AssistantController* assistant_controller_;    // Owned by Shell.
  InteractionContainer* interaction_container_;  // Owned by view hierarchy.
  TextContainer* text_container_;                // Owned by view hierarchy.
  CardContainer* card_container_;                // Owned by view hierarchy.
  SuggestionsContainer* suggestions_container_;  // Owned by view hierarchy.

  // Uniquely identifies a card owned by AssistantCardRenderer.
  base::Optional<base::UnguessableToken> id_token_;

  base::WeakPtrFactory<AssistantBubbleView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantBubbleView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_ASSISTANT_BUBBLE_VIEW_H_
