// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_KIOSK_NEXT_HOME_CONTROLLER_H_
#define ASH_KIOSK_NEXT_KIOSK_NEXT_HOME_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace ash {

// KioskNextHomeController manages the Home window for the Kiosk Next shell.
// TODO(michaelpg): Manage gestures on the Home window, such as dragging down
// from the top for Overview mode.
class ASH_EXPORT KioskNextHomeController : public HomeScreenDelegate,
                                           public display::DisplayObserver,
                                           public aura::WindowObserver {
 public:
  KioskNextHomeController();
  ~KioskNextHomeController() override;

  // HomeScreenDelegate:
  void ShowHomeScreenView() override;
  aura::Window* GetHomeScreenWindow() override;
  void UpdateYPositionAndOpacityForHomeLauncher(
      int y_position_in_screen,
      float opacity,
      UpdateAnimationSettingsCallback callback) override;
  void UpdateAfterHomeLauncherShown() override;
  base::Optional<base::TimeDelta> GetOptionalAnimationDuration() override;
  bool ShouldShowShelfOnHomeScreen() const override;
  bool ShouldShowStatusAreaOnHomeScreen() const override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override;
  void OnWillRemoveWindow(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  aura::Window* home_screen_container_ = nullptr;
  aura::Window* home_screen_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(KioskNextHomeController);
};

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_KIOSK_NEXT_HOME_CONTROLLER_H_
