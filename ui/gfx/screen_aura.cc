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
  DCHECK(instance_);
  return instance_->GetCursorScreenPointImpl();
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeWindow window) {
  DCHECK(instance_);
  return instance_->GetMonitorWorkAreaNearestWindowImpl(window);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeWindow window) {
  DCHECK(instance_);
  return instance_->GetMonitorAreaNearestWindowImpl(window);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestPoint(const gfx::Point& point) {
  DCHECK(instance_);
  return instance_->GetMonitorWorkAreaNearestPointImpl(point);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  DCHECK(instance_);
  return instance_->GetMonitorAreaNearestPointImpl(point);
}

// static
gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  DCHECK(instance_);
  return instance_->GetWindowAtCursorScreenPointImpl();
}

}  // namespace gfx
