// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ASSISTANT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_ASSISTANT_CONTAINER_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AssistantResponseContainerView;
class AssistantViewDelegate;
class AmbientAssistantDialogPlate;

class AmbientAssistantContainerView : public views::View,
                                      public AssistantUiModelObserver {
 public:
  AmbientAssistantContainerView();
  ~AmbientAssistantContainerView() override;

  // views::View:
  const char* GetClassName() const override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();

  // Owned by |AssistantController|, so it should always outlive |this|.
  AssistantViewDelegate* const delegate_;

  // Owned by view hierarchy.
  AmbientAssistantDialogPlate* ambient_assistant_dialog_plate_ = nullptr;
  AssistantResponseContainerView* assistant_response_container_view_ = nullptr;
  views::ImageView* avatar_view_ = nullptr;
  views::Label* greeting_label_ = nullptr;

  ScopedObserver<AssistantUiController,
                 AssistantUiModelObserver,
                 &AssistantUiController::AddModelObserver,
                 &AssistantUiController::RemoveModelObserver>
      assistant_ui_model_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientAssistantContainerView);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ASSISTANT_CONTAINER_VIEW_H_
