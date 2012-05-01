// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen_impl.h"

namespace gfx {

// gfx can't depend upon aura, otherwise we have circular dependencies. So,
// gfx::Screen is pluggable and Desktop plugs in the real implementation.
namespace {
ScreenImpl* g_instance_ = NULL;
}

// static
void Screen::SetInstance(ScreenImpl* screen) {
  delete g_instance_;
  g_instance_ = screen;
}

// TODO(oshima): Implement ScreenImpl for Linux/aura and remove this
// ifdef.

// static
Point Screen::GetCursorScreenPoint() {
  return g_instance_->GetCursorScreenPoint();
}

// static
NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  return g_instance_->GetWindowAtCursorScreenPoint();
}

// static
int Screen::GetNumMonitors() {
  return g_instance_->GetNumMonitors();
}

// static
Monitor Screen::GetMonitorNearestWindow(NativeView window) {
  return g_instance_->GetMonitorNearestWindow(window);
}

// static
Monitor Screen::GetMonitorNearestPoint(const Point& point) {
  return g_instance_->GetMonitorNearestPoint(point);
}

// static
Monitor Screen::GetPrimaryMonitor() {
  return g_instance_->GetPrimaryMonitor();
}

// static
Monitor Screen::GetMonitorMatching(const gfx::Rect& match_rect) {
  return g_instance_->GetMonitorNearestPoint(match_rect.CenterPoint());
}

}  // namespace gfx
