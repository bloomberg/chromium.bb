// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include <string>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

void LogUserNightLightEvent(const bool enabled) {
  auto* logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogNightLightUkmEvent(enabled);
  }
}

}  // namespace

NightLightFeaturePodController::NightLightFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  DCHECK(tray_controller_);
  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
}

NightLightFeaturePodController::~NightLightFeaturePodController() {
  Shell::Get()->system_tray_model()->clock()->RemoveObserver(this);
}

FeaturePodButton* NightLightFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuNightLightIcon);
  button_->SetVisible(
      Shell::Get()->session_controller()->ShouldEnableSettings());
  button_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_BUTTON_LABEL));
  button_->SetLabelTooltip(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP));
  UpdateButton();
  return button_;
}

void NightLightFeaturePodController::OnIconPressed() {
  Shell::Get()->night_light_controller()->Toggle();
  LogUserNightLightEvent(Shell::Get()->night_light_controller()->GetEnabled());
  UpdateButton();

  if (Shell::Get()->night_light_controller()->GetEnabled()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_NightLight_Enabled"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_NightLight_Disabled"));
  }
}

void NightLightFeaturePodController::OnLabelPressed() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_NightLight_Settings"));
    tray_controller_->CloseBubble();  // Deletes |this|.
    Shell::Get()->system_tray_model()->client()->ShowDisplaySettings();
  }
}

SystemTrayItemUmaType NightLightFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_NIGHT_LIGHT;
}

void NightLightFeaturePodController::OnDateFormatChanged() {
  UpdateButton();
}

void NightLightFeaturePodController::OnSystemClockTimeUpdated() {
  UpdateButton();
}

void NightLightFeaturePodController::OnSystemClockCanSetTimeChanged(
    bool can_set_time) {
  UpdateButton();
}

void NightLightFeaturePodController::Refresh() {
  UpdateButton();
}

const std::u16string NightLightFeaturePodController::GetPodSubLabel() {
  auto* controller = Shell::Get()->night_light_controller();
  const bool is_enabled = controller->GetEnabled();
  const NightLightController::ScheduleType schedule_type =
      controller->GetScheduleType();
  std::u16string sublabel;
  switch (schedule_type) {
    case NightLightController::ScheduleType::kNone:
      return l10n_util::GetStringUTF16(
          is_enabled ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE
                     : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE);
    case NightLightController::ScheduleType::kSunsetToSunrise:
      return l10n_util::GetStringUTF16(
          is_enabled
              ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_SUNSET_TO_SUNRISE_SCHEDULED
              : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_SUNSET_TO_SUNRISE_SCHEDULED);
    case NightLightController::ScheduleType::kCustom:
      const TimeOfDay time = is_enabled ? controller->GetCustomEndTime()
                                        : controller->GetCustomStartTime();
      const std::u16string time_str =
          base::TimeFormatTimeOfDayWithHourClockType(
              time.ToTimeToday(),
              Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
              base::kKeepAmPm);
      return is_enabled
                 ? l10n_util::GetStringFUTF16(
                       IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_CUSTOM_SCHEDULED,
                       time_str)
                 : l10n_util::GetStringFUTF16(
                       IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_CUSTOM_SCHEDULED,
                       time_str);
  }
}

void NightLightFeaturePodController::UpdateButton() {
  auto* controller = Shell::Get()->night_light_controller();
  const bool is_enabled = controller->GetEnabled();
  button_->SetToggled(is_enabled);
  button_->SetSubLabel(GetPodSubLabel());

  std::u16string tooltip_state = l10n_util::GetStringUTF16(
      is_enabled ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ENABLED_STATE_TOOLTIP
                 : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_DISABLED_STATE_TOOLTIP);
  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_TOGGLE_TOOLTIP, tooltip_state));
}

}  // namespace ash
