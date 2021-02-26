// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// SuggestionContainerView is the child of AssistantMainView concerned with
// laying out SuggestionChipViews in response to Assistant interaction model
// suggestion events.
class COMPONENT_EXPORT(ASSISTANT_UI) SuggestionContainerView
    : public AnimatedContainerView,
      public AssistantSuggestionsModelObserver,
      public AssistantUiModelObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::AssistantSuggestion;

  METADATA_HEADER(SuggestionContainerView);

  explicit SuggestionContainerView(AssistantViewDelegate* delegate);
  SuggestionContainerView(const SuggestionContainerView&) = delete;
  SuggestionContainerView& operator=(const SuggestionContainerView&) = delete;
  ~SuggestionContainerView() override;

  // AnimatedContainerView:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;
  void OnAssistantControllerDestroying() override;
  void OnCommittedQueryChanged(const AssistantQuery& query) override;

  // AssistantSuggestionsModelObserver:
  void OnConversationStartersChanged(
      const std::vector<AssistantSuggestion>& conversation_starters) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // The suggestion chip that was pressed by the user. May be |nullptr|.
  const SuggestionChipView* selected_chip() const { return selected_chip_; }

 private:
  void InitLayout();

  // AnimatedContainerView:
  std::unique_ptr<ElementAnimator> HandleSuggestion(
      const AssistantSuggestion& suggestion) override;
  void OnAllViewsRemoved() override;

  std::unique_ptr<ElementAnimator> AddSuggestionChip(
      const AssistantSuggestion& suggestion);

  void OnButtonPressed(SuggestionChipView* chip_view);

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.

  // Whether or not we have committed a query during this Assistant session.
  bool has_committed_query_ = false;

  // The suggestion chip that was pressed by the user. May be |nullptr|.
  const SuggestionChipView* selected_chip_ = nullptr;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
