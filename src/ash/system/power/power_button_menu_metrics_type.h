// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_METRICS_TYPE_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_METRICS_TYPE_H_

namespace ash {

// Used for histograms. See tools/metrics/histograms/enums.xml
// PowerButtonMenuActionType.
enum class PowerButtonMenuActionType {
  kSignOut,
  kPowerOff,
  kDismissByEsc,
  kDismissByMouse,
  kDismissByTouch,
  kLockScreen,
  kFeedback,
  kMaxValue = kFeedback,
};

void RecordMenuActionHistogram(PowerButtonMenuActionType type);

// Used for histograms. See tools/metrics/histograms/enums.xml
// PowerButtonPressType.
enum class PowerButtonPressType {
  kTapWithoutMenu,
  kTapWithMenu,
  kLongPressToShowMenu,
  kLongPressWithMenuToShutdown,
  kLongPressWithoutMenuToShutdown,
  kMaxValue = kLongPressWithoutMenuToShutdown,
};

void RecordPressInLaptopModeHistogram(PowerButtonPressType type);
void RecordPressInTabletModeHistogram(PowerButtonPressType type);

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_METRICS_TYPE_H_
