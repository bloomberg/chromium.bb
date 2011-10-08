// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/screen_aura.h"

#include "base/logging.h"
#include "ui/aura/desktop.h"
#include "ui/gfx/native_widget_types.h"

namespace {

gfx::Rect GetMonitorAreaOrWorkAreaNearestPoint(const gfx::Point& point,
                                               bool work_area) {
  // TODO(oshima): Take point/work_area into account. Support multiple monitors.
  return gfx::Rect(aura::Desktop::GetInstance()->GetSize());
}

}  // namespace

namespace aura {
namespace internal {

ScreenAura::ScreenAura() {
}

ScreenAura::~ScreenAura() {
}

gfx::Rect ScreenAura::GetMonitorWorkAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  gfx::Rect bounds = GetMonitorAreaNearestWindow(window);
  // Emulate that a work area can be smaller than its monitor.
  bounds.Inset(10, 10, 10, 10);
  return bounds;
}

gfx::Rect ScreenAura::GetMonitorAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  // TODO(oshima): Take point/work_area into account. Support multiple monitors.
  return gfx::Rect(aura::Desktop::GetInstance()->GetSize());
}

gfx::Rect ScreenAura::GetMonitorWorkAreaNearestPointImpl(
    const gfx::Point& point) {
  return GetMonitorAreaOrWorkAreaNearestPoint(point, true);
}

gfx::Rect ScreenAura::GetMonitorAreaNearestPointImpl(const gfx::Point& point) {
  return GetMonitorAreaOrWorkAreaNearestPoint(point, false);
}

gfx::NativeWindow ScreenAura::GetWindowAtCursorScreenPointImpl() {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace internal
}  // namespace aura
