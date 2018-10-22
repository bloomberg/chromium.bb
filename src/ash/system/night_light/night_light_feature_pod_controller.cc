// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_feature_pod_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

NightLightFeaturePodController::NightLightFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  DCHECK(tray_controller_);
}

NightLightFeaturePodController::~NightLightFeaturePodController() = default;

FeaturePodButton* NightLightFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuNightLightIcon);
  button_->SetVisible(
      features::IsNightLightEnabled() &&
      Shell::Get()->session_controller()->ShouldEnableSettings());
  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_BUTTON_LABEL));
  button_->SetIconTooltip(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_TOGGLE_TOOLTIP));
  button_->SetLabelTooltip(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP));
  UpdateButton();
  return button_;
}

void NightLightFeaturePodController::OnIconPressed() {
  DCHECK(features::IsNightLightEnabled());
  Shell::Get()->night_light_controller()->Toggle();
  UpdateButton();
}

void NightLightFeaturePodController::OnLabelPressed() {
  DCHECK(features::IsNightLightEnabled());
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client_ptr()->ShowDisplaySettings();
    tray_controller_->CloseBubble();
  }
}

SystemTrayItemUmaType NightLightFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_NIGHT_LIGHT;
}

void NightLightFeaturePodController::UpdateButton() {
  if (!features::IsNightLightEnabled())
    return;

  bool is_enabled = Shell::Get()->night_light_controller()->GetEnabled();
  button_->SetToggled(is_enabled);
  button_->SetSubLabel(l10n_util::GetStringUTF16(
      is_enabled ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE
                 : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE));
}

}  // namespace ash
