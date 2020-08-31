// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_DIALOG_PLATE_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_DIALOG_PLATE_H_

#include <memory>
#include <string>

#include "ash/app_list/app_list_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query_history.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/base/assistant_button_listener.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace views {
class ImageButton;
}  // namespace views

namespace ash {
class AssistantViewDelegate;
class LogoView;
class MicView;

// AssistantDialogPlate --------------------------------------------------------

// AssistantDialogPlate is the child of AssistantMainView concerned with
// providing the means by which a user converses with Assistant. To this end,
// AssistantDialogPlate provides a textfield for use with the keyboard input
// modality, and a MicView which serves to toggle voice interaction as
// appropriate for use with the voice input modality.
class APP_LIST_EXPORT AssistantDialogPlate
    : public views::View,
      public views::TextfieldController,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public AssistantButtonListener {
 public:
  explicit AssistantDialogPlate(AssistantViewDelegate* delegate);
  ~AssistantDialogPlate() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;

  // AssistantButtonListener:
  void OnButtonPressed(AssistantButtonId button_id) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // ash::AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // ash::AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;
  void OnCommittedQueryChanged(const AssistantQuery& committed_query) override;

  // ash::AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();
  void InitKeyboardLayoutContainer();
  void InitVoiceLayoutContainer();

  void UpdateModalityVisibility();
  void UpdateKeyboardVisibility();

  void OnAnimationStarted(const ui::CallbackLayerAnimationObserver& observer);
  bool OnAnimationEnded(const ui::CallbackLayerAnimationObserver& observer);

  InputModality input_modality() const;

  AssistantViewDelegate* const delegate_;

  // The following views are all owned by the view hierarchy
  LogoView* molecule_icon_ = nullptr;
  views::View* input_modality_layout_container_ = nullptr;
  views::View* keyboard_layout_container_ = nullptr;
  views::View* voice_layout_container_ = nullptr;
  views::ImageButton* keyboard_input_toggle_ = nullptr;
  views::ImageButton* voice_input_toggle_ = nullptr;
  MicView* animated_voice_input_toggle_ = nullptr;
  views::Textfield* textfield_ = nullptr;

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;
  std::unique_ptr<AssistantQueryHistory::Iterator> query_history_iterator_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  ScopedObserver<AssistantInteractionController,
                 AssistantInteractionModelObserver,
                 &AssistantInteractionController::AddModelObserver,
                 &AssistantInteractionController::RemoveModelObserver>
      assistant_interaction_model_observer_{this};

  ScopedObserver<AssistantUiController,
                 AssistantUiModelObserver,
                 &AssistantUiController::AddModelObserver,
                 &AssistantUiController::RemoveModelObserver>
      assistant_ui_model_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantDialogPlate);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_DIALOG_PLATE_H_
