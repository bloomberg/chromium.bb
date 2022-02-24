// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hps/hps_orientation_controller.h"

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_observation.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

HpsOrientationController::HpsOrientationController() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  DCHECK(tablet_mode_controller);
  physical_tablet_state_ =
      tablet_mode_controller->is_in_tablet_physical_state();
  tablet_mode_observation_.Observe(tablet_mode_controller);

  // Only care about rotation of the actual device.
  if (!display::Display::HasInternalDisplay())
    return;

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  DCHECK(display_manager);
  display_rotated_ =
      display_manager->GetDisplayForId(display::Display::InternalDisplayId())
          .rotation() != display::Display::ROTATE_0;
}

HpsOrientationController::~HpsOrientationController() = default;

void HpsOrientationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HpsOrientationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HpsOrientationController::IsOrientationSuitable() const {
  return !physical_tablet_state_ && !display_rotated_;
}

void HpsOrientationController::OnTabletPhysicalStateChanged() {
  const bool physical_tablet_state =
      Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
  UpdateOrientation(physical_tablet_state, display_rotated_);
}

void HpsOrientationController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // We only care when the rotation of the in-built display changes.
  if (!display.IsInternal() ||
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  const bool display_rotated = display.rotation() != display::Display::ROTATE_0;
  UpdateOrientation(physical_tablet_state_, display_rotated);
}

void HpsOrientationController::UpdateOrientation(bool physical_tablet_state,
                                                 bool display_rotated) {
  const bool was_suitable = IsOrientationSuitable();

  physical_tablet_state_ = physical_tablet_state;
  display_rotated_ = display_rotated;

  const bool is_suitable = IsOrientationSuitable();

  if (was_suitable == is_suitable)
    return;

  for (Observer& observer : observers_)
    observer.OnOrientationChanged(is_suitable);
}

}  // namespace ash
