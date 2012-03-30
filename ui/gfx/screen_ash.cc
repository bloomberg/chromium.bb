// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

// gfx can't depend upon ash, otherwise we have circular dependencies. So,
// gfx::Screen is pluggable and Desktop plugs in the real implementation.

// static
Screen* Screen::instance_ = NULL;

// static
void Screen::SetInstance(Screen* screen) {
  delete instance_;
  instance_ = screen;
}

// static
gfx::Point Screen::GetCursorScreenPoint() {
  // TODO(erg): Figure out what to do about the Screen class. For now, I've
  // added default values for when a Screen instance class isn't passed in, but
  // this is the wrong thing.
  if (!instance_)
    return gfx::Point();
  return instance_->GetCursorScreenPointImpl();
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeWindow window) {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorWorkAreaNearestWindowImpl(window);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeWindow window) {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorAreaNearestWindowImpl(window);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestPoint(const gfx::Point& point) {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorWorkAreaNearestPointImpl(point);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorAreaNearestPointImpl(point);
}

// static
gfx::Rect Screen::GetPrimaryMonitorWorkArea() {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorWorkAreaNearestPoint(gfx::Point());
}

// static
gfx::Rect Screen::GetPrimaryMonitorBounds() {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorAreaNearestPoint(gfx::Point());
}

// static
gfx::Rect Screen::GetMonitorWorkAreaMatching(const gfx::Rect& match_rect) {
  if (!instance_)
    return gfx::Rect(0, 0, 800, 800);
  return instance_->GetMonitorWorkAreaNearestPoint(gfx::Point());
}

// static
gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  if (!instance_)
    return NULL;
  return instance_->GetWindowAtCursorScreenPointImpl();
}

// static
gfx::Size Screen::GetPrimaryMonitorSize() {
  if (!instance_)
    return gfx::Size(800, 800);
  return instance_->GetPrimaryMonitorSizeImpl();
}

// static
int Screen::GetNumMonitors() {
  if (!instance_)
    return 1;
  return instance_->GetNumMonitorsImpl();
}

}  // namespace gfx
