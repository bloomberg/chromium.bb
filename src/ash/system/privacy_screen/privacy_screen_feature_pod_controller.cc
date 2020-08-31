// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_feature_pod_controller.h"

#include <utility>

#include "ash/display/privacy_screen_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

PrivacyScreenFeaturePodController::PrivacyScreenFeaturePodController() {
  Shell::Get()->privacy_screen_controller()->AddObserver(this);
}

PrivacyScreenFeaturePodController::~PrivacyScreenFeaturePodController() {
  Shell::Get()->privacy_screen_controller()->RemoveObserver(this);
}

FeaturePodButton* PrivacyScreenFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  UpdateButton();
  return button_;
}

void PrivacyScreenFeaturePodController::OnIconPressed() {
  TogglePrivacyScreen();
}

void PrivacyScreenFeaturePodController::OnLabelPressed() {
  TogglePrivacyScreen();
}

SystemTrayItemUmaType PrivacyScreenFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_PRIVACY_SCREEN;
}

void PrivacyScreenFeaturePodController::TogglePrivacyScreen() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  DCHECK(privacy_screen_controller->IsSupported());
  privacy_screen_controller->SetEnabled(
      !privacy_screen_controller->GetEnabled());
}

void PrivacyScreenFeaturePodController::UpdateButton() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();

  bool is_supported = privacy_screen_controller->IsSupported();
  button_->SetVisible(is_supported);
  if (!is_supported)
    return;

  bool is_enabled = privacy_screen_controller->GetEnabled();
  bool is_managed = privacy_screen_controller->IsManaged();

  button_->SetVectorIcon(kPrivacyScreenIcon);
  button_->SetToggled(is_enabled);
  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_LABEL));

  base::string16 tooltip_state;
  if (is_enabled) {
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_SUBLABEL));
    tooltip_state =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE);
  } else {
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_SUBLABEL));
    tooltip_state =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_STATE);
  }

  if (is_managed) {
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_MANAGED_SUBLABEL));
  }

  button_->SetIconAndLabelTooltips(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOOLTIP, tooltip_state));
}

void PrivacyScreenFeaturePodController::OnPrivacyScreenSettingChanged(
    bool enabled) {
  UpdateButton();
}

}  // namespace ash
