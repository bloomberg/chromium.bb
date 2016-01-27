// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/default_screen_position_client.h"

#include "ui/aura/window_tree_host.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/screen.h"

namespace wm {

DefaultScreenPositionClient::DefaultScreenPositionClient() {
}

DefaultScreenPositionClient::~DefaultScreenPositionClient() {
}

gfx::Point DefaultScreenPositionClient::GetOriginInScreen(
    const aura::Window* root_window) {
  gfx::Point origin_in_pixels = root_window->GetHost()->GetBounds().origin();
  aura::Window* window = const_cast<aura::Window*>(root_window);
  float scale = gfx::Screen::GetScreen()
                    ->GetDisplayNearestWindow(window)
                    .device_scale_factor();
  return gfx::ScaleToFlooredPoint(origin_in_pixels, 1.0f / scale);
}

void DefaultScreenPositionClient::ConvertPointToScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::Window* root_window = window->GetRootWindow();
  aura::Window::ConvertPointToTarget(window, root_window, point);
  gfx::Point origin = GetOriginInScreen(root_window);
  point->Offset(origin.x(), origin.y());
}

void DefaultScreenPositionClient::ConvertPointFromScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::Window* root_window = window->GetRootWindow();
  gfx::Point origin = GetOriginInScreen(root_window);
  point->Offset(-origin.x(), -origin.y());
  aura::Window::ConvertPointToTarget(root_window, window, point);
}

void DefaultScreenPositionClient::ConvertHostPointToScreen(aura::Window* window,
                                                           gfx::Point* point) {
  aura::Window* root_window = window->GetRootWindow();
  ConvertPointToScreen(root_window, point);
}

void DefaultScreenPositionClient::SetBounds(aura::Window* window,
                                            const gfx::Rect& bounds,
                                            const gfx::Display& display) {
  window->SetBounds(bounds);
}

}  // namespace wm
