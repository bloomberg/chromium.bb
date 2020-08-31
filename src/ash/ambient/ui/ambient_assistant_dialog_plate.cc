// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_assistant_dialog_plate.h"

#include <memory>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/dialog_plate/mic_view.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

AmbientAssistantDialogPlate::AmbientAssistantDialogPlate(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  assistant_interaction_model_observer_.Add(
      AssistantInteractionController::Get());
}

AmbientAssistantDialogPlate::~AmbientAssistantDialogPlate() = default;

const char* AmbientAssistantDialogPlate::GetClassName() const {
  return "AmbientAssistantDialogPlate";
}

void AmbientAssistantDialogPlate::OnButtonPressed(AssistantButtonId button_id) {
  delegate_->OnDialogPlateButtonPressed(button_id);
}

void AmbientAssistantDialogPlate::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  voice_query_view_->SetQuery(query);
}

void AmbientAssistantDialogPlate::OnPendingQueryChanged(
    const AssistantQuery& query) {
  voice_query_view_->SetQuery(query);
}

void AmbientAssistantDialogPlate::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  // Animated voice input toggle button.
  animated_voice_input_toggle_ = AddChildView(
      std::make_unique<MicView>(this, AssistantButtonId::kVoiceInputToggle));
  animated_voice_input_toggle_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME));

  // Voice input query view.
  voice_query_view_ = AddChildView(std::make_unique<AssistantQueryView>());
}

}  // namespace ash
