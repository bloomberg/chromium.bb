// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/wayland/wayland_display.h"
#include "ui/wayland/wayland_screen.h"

namespace gfx {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::Rect static GetPrimaryMonitorBounds() {
  ui::WaylandDisplay* display = ui::WaylandDisplay::GetDisplay(
      gfx::GLSurfaceEGL::GetNativeDisplay());
  std::list<ui::WaylandScreen*> screens = display->GetScreenList();

  return screens.empty() ? gfx::Rect() : screens.front()->GetAllocation();
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeView view) {
  // TODO(dnicoara): use |view|.
  return GetPrimaryMonitorBounds();
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestPoint(const gfx::Point& point) {
  return GetMonitorAreaNearestPoint(point);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace gfx

