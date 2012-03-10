// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

// gfx can't depend upon aura, otherwise we have circular dependencies. So,
// gfx::Screen is pluggable for aura and Desktop plugs in the real
// implementation.

// static
Screen* Screen::instance_ = NULL;

// static
void Screen::SetInstance(Screen* screen) {
  delete instance_;
  instance_ = screen;
}

// static
gfx::Point Screen::GetCursorScreenPoint() {
  return instance_->GetCursorScreenPointImpl();
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeWindow window) {
  return instance_->GetMonitorWorkAreaNearestWindowImpl(window);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeWindow window) {
  return instance_->GetMonitorAreaNearestWindowImpl(window);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestPoint(const gfx::Point& point) {
  return instance_->GetMonitorWorkAreaNearestPointImpl(point);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  return instance_->GetMonitorAreaNearestPointImpl(point);
}

// static
gfx::Rect Screen::GetPrimaryMonitorWorkArea() {
  return instance_->GetMonitorWorkAreaNearestPoint(gfx::Point());
}

// static
gfx::Rect Screen::GetPrimaryMonitorBounds() {
  return instance_->GetMonitorAreaNearestPoint(gfx::Point());
}

// static
gfx::Rect Screen::GetMonitorWorkAreaMatching(const gfx::Rect& match_rect) {
  return instance_->GetMonitorWorkAreaNearestPoint(gfx::Point());
}

// static
gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  return instance_->GetWindowAtCursorScreenPointImpl();
}

// static
gfx::Size Screen::GetPrimaryMonitorSize() {
  return instance_->GetPrimaryMonitorSizeImpl();
}

// static
int Screen::GetNumMonitors() {
  return instance_->GetNumMonitorsImpl();
}

}  // namespace gfx
