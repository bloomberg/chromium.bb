// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/window_util.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/activation_controller.h"
#include "ui/base/ui_base_types.h"

namespace aura_shell {

bool IsWindowMaximized(aura::Window* window) {
  return window->GetIntProperty(aura::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

void ActivateWindow(aura::Window* window) {
  aura::ActivationClient::GetActivationClient()->ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  aura::ActivationClient::GetActivationClient()->DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return GetActiveWindow() == window;
}

aura::Window* GetActiveWindow() {
  return aura::ActivationClient::GetActivationClient()->GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return internal::ActivationController::GetActivatableWindow(window);
}

}  // namespace aura_shell
