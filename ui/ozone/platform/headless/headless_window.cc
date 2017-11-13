// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window.h"

#include <string>

#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

HeadlessWindow::HeadlessWindow(PlatformWindowDelegate* delegate,
                               HeadlessWindowManager* manager,
                               const gfx::Rect& bounds)
    : delegate_(delegate), manager_(manager), bounds_(bounds) {
  widget_ = manager_->AddWindow(this);
  delegate_->OnAcceleratedWidgetAvailable(widget_, 1.f);
}

HeadlessWindow::~HeadlessWindow() {
  manager_->RemoveWindow(widget_, this);
}

gfx::Rect HeadlessWindow::GetBounds() {
  return bounds_;
}

void HeadlessWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

void HeadlessWindow::SetTitle(const base::string16& title) {}

void HeadlessWindow::Show() {}

void HeadlessWindow::Hide() {}

void HeadlessWindow::Close() {}

void HeadlessWindow::PrepareForShutdown() {}

void HeadlessWindow::SetCapture() {}

void HeadlessWindow::ReleaseCapture() {}

void HeadlessWindow::ToggleFullscreen() {}

void HeadlessWindow::Maximize() {}

void HeadlessWindow::Minimize() {}

void HeadlessWindow::Restore() {}

void HeadlessWindow::SetCursor(PlatformCursor cursor) {}

void HeadlessWindow::MoveCursorTo(const gfx::Point& location) {}

void HeadlessWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {}

PlatformImeController* HeadlessWindow::GetPlatformImeController() {
  return nullptr;
}

}  // namespace ui
