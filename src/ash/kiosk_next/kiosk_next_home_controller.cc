// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_home_controller.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/wm_event.h"
#include "base/logging.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ash {

KioskNextHomeController::KioskNextHomeController() {
  display::Screen::GetScreen()->AddObserver(this);
}

KioskNextHomeController::~KioskNextHomeController() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void KioskNextHomeController::ShowHomeScreenView() {
  // Nothing to do because the contents of the app window are always shown
  // on the primary display.
  // HomeScreenController will show/hide the root home screen window.
}

aura::Window* KioskNextHomeController::GetHomeScreenWindow() {
  aura::Window::Windows containers = wm::GetContainersFromAllRootWindows(
      kShellWindowId_HomeScreenContainer, nullptr);
  for (aura::Window* container : containers) {
    if (container->children().empty())
      continue;

    // Expect only one window (there should be no app list window).
    DCHECK_EQ(1u, container->children().size());
    aura::Window* window = container->children()[0];
    DCHECK_EQ(window->type(), aura::client::WindowType::WINDOW_TYPE_NORMAL);
    return window;
  }

  return nullptr;
}

void KioskNextHomeController::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    UpdateAnimationSettingsCallback callback) {
  aura::Window* window = GetHomeScreenWindow();
  if (!window)
    return;

  const gfx::Transform translation(1.f, 0.f, 0.f, 1.f, 0.f,
                                   static_cast<float>(y_position_in_screen));
  ui::Layer* layer = window->layer();
  layer->GetAnimator()->StopAnimating();
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        layer->GetAnimator());
    callback.Run(settings.get());
  }
  layer->SetOpacity(opacity);
  layer->SetTransform(translation);
}

void KioskNextHomeController::UpdateAfterHomeLauncherShown() {}

base::Optional<base::TimeDelta>
KioskNextHomeController::GetOptionalAnimationDuration() {
  return base::nullopt;
}

bool KioskNextHomeController::ShouldShowShelfOnHomeScreen() const {
  return false;
}

bool KioskNextHomeController::ShouldShowStatusAreaOnHomeScreen() const {
  return true;
}

void KioskNextHomeController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::Window* window = GetHomeScreenWindow();
  if (!window)
    return;

  // Update the window to reflect the display bounds.
  const gfx::Rect bounds =
      screen_util::SnapBoundsToDisplayEdge(window->bounds(), window);
  window->SetBounds(bounds);
  const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
  wm::GetWindowState(window)->OnWMEvent(&event);
}

}  // namespace ash
