// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "components/arc/arc_util.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

namespace {

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  if (!window)
    return false;

  const aura::Window* parent = window->parent();
  return parent &&
         parent->id() == ash::ShellWindowId::kShellWindowId_AppListContainer;
}

void ThrottleInstance(const aura::Window* active) {
  SetArcCpuRestriction(!IsArcAppWindow(active));
}

}  // namespace

ArcInstanceThrottle::ArcInstanceThrottle() {
  if (!ash::Shell::HasInstance())  // for unit testing.
    return;
  ash::Shell::Get()->activation_client()->AddObserver(this);
  ThrottleInstance(ash::wm::GetActiveWindow());
}

ArcInstanceThrottle::~ArcInstanceThrottle() {
  if (!ash::Shell::HasInstance())
    return;
  ash::Shell::Get()->activation_client()->RemoveObserver(this);
}

void ArcInstanceThrottle::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  // On launching an app from the app list on Chrome OS the following events
  // happen -
  //
  // 1. When app list is brought up it's treated as a Chrome window and the
  // instance is throttled.
  //
  // 2. When an app icon is clicked the instance is unthrottled.
  //
  // 3. Between the time the app opens there is a narrow slice of time where
  // this callback is triggered with |lost_active| equal to the app list window
  // and the gained active possibly a native window. Without the check below the
  // instance will be throttled again, further delaying the app launch. If the
  // app was a native one then the instance was throttled anyway in step 1.
  if (IsAppListWindow(lost_active))
    return;
  ThrottleInstance(gained_active);
}

}  // namespace arc
