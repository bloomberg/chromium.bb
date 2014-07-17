// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/test_window.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/test/test_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

TestWindow::TestWindow(PlatformWindowDelegate* delegate,
                       TestWindowManager* manager,
                       const gfx::Rect& bounds)
    : delegate_(delegate), manager_(manager), bounds_(bounds) {
  widget_ = manager_->AddWindow(this);
  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(widget_);
}

TestWindow::~TestWindow() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  manager_->RemoveWindow(widget_, this);
}

base::FilePath TestWindow::path() {
  base::FilePath base_path = manager_->base_path();

  if (base_path == base::FilePath("/dev/null"))
    return base_path;

  // Disambiguate multiple window output files with the window id.
  return base_path.Append(base::IntToString(widget_));
}

gfx::Rect TestWindow::GetBounds() {
  return bounds_;
}

void TestWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

void TestWindow::Show() {
}

void TestWindow::Hide() {
}

void TestWindow::Close() {
}

void TestWindow::SetCapture() {
}

void TestWindow::ReleaseCapture() {
}

void TestWindow::ToggleFullscreen() {
}

void TestWindow::Maximize() {
}

void TestWindow::Minimize() {
}

void TestWindow::Restore() {
}

bool TestWindow::CanDispatchEvent(const ui::PlatformEvent& ne) {
  return true;
}

uint32_t TestWindow::DispatchEvent(const ui::PlatformEvent& ne) {
  ui::Event* event = static_cast<ui::Event*>(ne);
  delegate_->DispatchEvent(event);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
