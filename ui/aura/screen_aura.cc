// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/screen_aura.h"

#include "base/logging.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {

ScreenAura::ScreenAura() {
}

ScreenAura::~ScreenAura() {
}

gfx::Point ScreenAura::GetCursorScreenPointImpl() {
  return RootWindow::GetInstance()->last_mouse_location();
}

gfx::Rect ScreenAura::GetMonitorWorkAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  return GetWorkAreaBounds();
}

gfx::Rect ScreenAura::GetMonitorAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  return GetBounds();
}

gfx::Rect ScreenAura::GetMonitorWorkAreaNearestPointImpl(
    const gfx::Point& point) {
  return GetWorkAreaBounds();
}

gfx::Rect ScreenAura::GetMonitorAreaNearestPointImpl(const gfx::Point& point) {
  return GetBounds();
}

gfx::NativeWindow ScreenAura::GetWindowAtCursorScreenPointImpl() {
  const gfx::Point point = GetCursorScreenPoint();
  return RootWindow::GetInstance()->GetTopWindowContainingPoint(point);
}

gfx::Rect ScreenAura::GetBounds() {
  return gfx::Rect(aura::RootWindow::GetInstance()->GetHostSize());
}

gfx::Rect ScreenAura::GetWorkAreaBounds() {
  gfx::Rect bounds(GetBounds());
  bounds.Inset(work_area_insets_);
  return bounds;
}

gfx::Size ScreenAura::GetPrimaryMonitorSizeImpl() {
  return GetMonitorWorkAreaNearestPoint(gfx::Point()).size();
}

int ScreenAura::GetNumMonitorsImpl() {
  return 1;
}

}  // namespace aura
