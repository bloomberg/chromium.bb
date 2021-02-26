// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_

#include <vector>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/component_export.h"
#include "base/scoped_observer.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOnboardingView
    : public views::View,
      public AssistantControllerObserver,
      public AssistantSuggestionsModelObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantOnboardingView(AssistantViewDelegate* delegate);
  AssistantOnboardingView(const AssistantOnboardingView&) = delete;
  AssistantOnboardingView& operator=(const AssistantOnboardingView&) = delete;
  ~AssistantOnboardingView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantController:
  void OnAssistantControllerDestroying() override;

  // AssistantSuggestionsModelObserver:
  void OnOnboardingSuggestionsChanged(
      const std::vector<AssistantSuggestion>& onboarding_suggestions) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void UpdateGreeting();
  void UpdateSuggestions();

  AssistantViewDelegate* const delegate_;  // Owned by AssistantController.
  views::Label* greeting_ = nullptr;       // Owned by view hierarchy.
  views::View* grid_ = nullptr;            // Owned by view hierarchy.

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
