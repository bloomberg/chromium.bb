// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_slider_controller.h"

#include "ash/metrics/user_metrics_action.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

using chromeos::CrasAudioHandler;

namespace ash {

UnifiedVolumeSliderController::UnifiedVolumeSliderController(
    UnifiedVolumeSliderController::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

UnifiedVolumeSliderController::~UnifiedVolumeSliderController() = default;

views::View* UnifiedVolumeSliderController::CreateView() {
  DCHECK(!slider_);
  slider_ = new UnifiedVolumeView(this);
  return slider_;
}

void UnifiedVolumeSliderController::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  if (sender == slider_->button()) {
    bool mute_on = !CrasAudioHandler::Get()->IsOutputMuted();
    if (mute_on) {
      base::RecordAction(base::UserMetricsAction("StatusArea_Audio_Muted"));
    } else {
      base::RecordAction(base::UserMetricsAction("StatusArea_Audio_Unmuted"));
    }
    CrasAudioHandler::Get()->SetOutputMute(mute_on);
  } else if (sender == slider_->more_button()) {
    delegate_->OnAudioSettingsButtonClicked();
  }
}

void UnifiedVolumeSliderController::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  if (reason != views::SliderChangeReason::kByUser)
    return;

  const int level = value * 100;

  if (level != CrasAudioHandler::Get()->GetOutputVolumePercent()) {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_CHANGED_VOLUME_MENU);
  }

  CrasAudioHandler::Get()->SetOutputVolumePercent(level);

  // If the volume is above certain level and it's muted, it should be unmuted.
  if (CrasAudioHandler::Get()->IsOutputMuted() &&
      level > CrasAudioHandler::Get()->GetOutputDefaultVolumeMuteThreshold()) {
    CrasAudioHandler::Get()->SetOutputMute(false);
  }
}

}  // namespace ash
