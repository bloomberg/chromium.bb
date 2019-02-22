// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_shadow_controller_delegate.h"

#include "ash/shell.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

WmShadowControllerDelegate::WmShadowControllerDelegate() = default;

WmShadowControllerDelegate::~WmShadowControllerDelegate() = default;

bool WmShadowControllerDelegate::ShouldShowShadowForWindow(
    const aura::Window* window) {
  // Hide the shadow if it is one of the splitscreen snapped windows.
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (split_view_controller &&
      (window == split_view_controller->left_window() ||
       window == split_view_controller->right_window())) {
    return false;
  }

  // Hide the shadow while we are in overview mode.
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  if (window_selector_controller && window_selector_controller->IsSelecting()) {
    WindowSelector* window_selector =
        window_selector_controller->window_selector();
    if (!window_selector->IsShuttingDown() &&
        window_selector->IsWindowInOverview(window)) {
      return false;
    }
  }

  // Show the shadow if it's currently being dragged no matter of the window's
  // show state.
  if (wm::GetWindowState(window)->is_dragged())
    return ::wm::GetShadowElevationConvertDefault(window) > 0;

  // Hide the shadow if it's not being dragged and it's a maximized/fullscreen
  // window.
  ui::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  if (show_state == ui::SHOW_STATE_FULLSCREEN ||
      show_state == ui::SHOW_STATE_MAXIMIZED) {
    return false;
  }

  return ::wm::GetShadowElevationConvertDefault(window) > 0;
}

}  // namespace ash
