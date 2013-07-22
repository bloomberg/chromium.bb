// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window_host_ozone.h"

#include "ui/aura/root_window.h"
#include "ui/base/ozone/surface_factory_ozone.h"

namespace aura {

RootWindowHostOzone::RootWindowHostOzone(const gfx::Rect& bounds)
   : delegate_(NULL),
      widget_(0),
      bounds_(bounds),
      factory_(new ui::EventFactoryOzone()) {
  factory_->CreateStartupEventConverters();
  ui::SurfaceFactoryOzone* surface_factory =
      ui::SurfaceFactoryOzone::GetInstance();
  widget_ = surface_factory->GetAcceleratedWidget();

  surface_factory->AttemptToResizeAcceleratedWidget(widget_, bounds_);

  base::MessagePumpOzone::Current()->AddDispatcherForRootWindow(this);
}

RootWindowHostOzone::~RootWindowHostOzone() {
  base::MessagePumpOzone::Current()->RemoveDispatcherForRootWindow(0);
}

bool RootWindowHostOzone::Dispatch(const base::NativeEvent& ne) {
  ui::Event* event = static_cast<ui::Event*>(ne);
  if (event->IsTouchEvent()) {
    ui::TouchEvent* touchev = static_cast<ui::TouchEvent*>(ne);
    delegate_->OnHostTouchEvent(touchev);
  } else if (event->IsKeyEvent()) {
    ui::KeyEvent* keyev = static_cast<ui::KeyEvent*>(ne);
    delegate_->OnHostKeyEvent(keyev);
  }
  return true;
}

void RootWindowHostOzone::SetDelegate(RootWindowHostDelegate* delegate) {
  delegate_ = delegate;
}

RootWindow* RootWindowHostOzone::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget RootWindowHostOzone::GetAcceleratedWidget() {
  return widget_;
}

void RootWindowHostOzone::Show() { NOTIMPLEMENTED(); }

void RootWindowHostOzone::Hide() { NOTIMPLEMENTED(); }

void RootWindowHostOzone::ToggleFullScreen() { NOTIMPLEMENTED(); }

gfx::Rect RootWindowHostOzone::GetBounds() const { return bounds_; }

void RootWindowHostOzone::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Insets RootWindowHostOzone::GetInsets() const { return gfx::Insets(); }

void RootWindowHostOzone::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

gfx::Point RootWindowHostOzone::GetLocationOnNativeScreen() const {
  return bounds_.origin();
}

void RootWindowHostOzone::SetCapture() { NOTIMPLEMENTED(); }

void RootWindowHostOzone::ReleaseCapture() { NOTIMPLEMENTED(); }

void RootWindowHostOzone::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

bool RootWindowHostOzone::QueryMouseLocation(gfx::Point* location_return) {
  NOTIMPLEMENTED();
  return false;
}

bool RootWindowHostOzone::ConfineCursorToRootWindow() {
  NOTIMPLEMENTED();
  return false;
}

void RootWindowHostOzone::UnConfineCursor() { NOTIMPLEMENTED(); }

void RootWindowHostOzone::OnCursorVisibilityChanged(bool show) {
  NOTIMPLEMENTED();
}

void RootWindowHostOzone::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void RootWindowHostOzone::SetFocusWhenShown(bool focus_when_shown) {
  NOTIMPLEMENTED();
}

bool RootWindowHostOzone::CopyAreaToSkCanvas(const gfx::Rect& source_bounds,
                                             const gfx::Point& dest_offset,
                                             SkCanvas* canvas) {
  NOTIMPLEMENTED();
  return false;
}

void RootWindowHostOzone::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void RootWindowHostOzone::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void RootWindowHostOzone::PrepareForShutdown() { NOTIMPLEMENTED(); }

// static
RootWindowHost* RootWindowHost::Create(const gfx::Rect& bounds) {
  return new RootWindowHostOzone(bounds);
}

// static
gfx::Size RootWindowHost::GetNativeScreenSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

}  // namespace aura
