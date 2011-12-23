// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/window_util.h"

#include "ash/wm/activation_controller.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/property_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace aura_shell {

bool IsWindowMaximized(aura::Window* window) {
  return window->GetIntProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

void ActivateWindow(aura::Window* window) {
  aura::client::GetActivationClient()->ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  aura::client::GetActivationClient()->DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return GetActiveWindow() == window;
}

aura::Window* GetActiveWindow() {
  return aura::client::GetActivationClient()->GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return internal::ActivationController::GetActivatableWindow(window);
}

void UpdateBoundsFromShowState(aura::Window* window) {
  switch (window->GetIntProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      window->SetProperty(aura::client::kRestoreBoundsKey, NULL);
      if (restore)
        window->SetBounds(*restore);
      delete restore;
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      SetRestoreBoundsIfNotSet(window);
      window->SetBounds(gfx::Screen::GetMonitorWorkAreaNearestWindow(window));
      break;

    case ui::SHOW_STATE_FULLSCREEN:
      SetRestoreBoundsIfNotSet(window);
      window->SetBounds(gfx::Screen::GetMonitorAreaNearestWindow(window));
      break;

    default:
      break;
  }
}

}  // namespace aura_shell
