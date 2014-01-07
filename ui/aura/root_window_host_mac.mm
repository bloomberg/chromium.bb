// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "ui/aura/root_window_host_mac.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_delegate.h"

namespace aura {

RootWindowHostMac::RootWindowHostMac(const gfx::Rect& bounds) {
  window_.reset(
      [[NSWindow alloc]
          initWithContentRect:NSRectFromCGRect(bounds.ToCGRect())
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  CreateCompositor(GetAcceleratedWidget());
}

RootWindowHostMac::~RootWindowHostMac() {
}

RootWindow* RootWindowHostMac::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget RootWindowHostMac::GetAcceleratedWidget() {
  return [window_ contentView];
}
void RootWindowHostMac::Show() {
  [window_ makeKeyAndOrderFront:nil];
}

void RootWindowHostMac::Hide() {
  [window_ orderOut:nil];
}

void RootWindowHostMac::ToggleFullScreen() {
}

gfx::Rect RootWindowHostMac::GetBounds() const {
  return gfx::Rect(NSRectToCGRect([window_ frame]));
}

void RootWindowHostMac::SetBounds(const gfx::Rect& bounds) {
  [window_ setFrame:NSRectFromCGRect(bounds.ToCGRect()) display:YES animate:NO];
}

gfx::Insets RootWindowHostMac::GetInsets() const {
  NOTIMPLEMENTED();
  return gfx::Insets();
}

void RootWindowHostMac::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

gfx::Point RootWindowHostMac::GetLocationOnNativeScreen() const {
  NOTIMPLEMENTED();
  return gfx::Point(0, 0);
}

void RootWindowHostMac::SetCapture() {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::SetCursor(gfx::NativeCursor cursor_type) {
  NOTIMPLEMENTED();
}

bool RootWindowHostMac::QueryMouseLocation(gfx::Point* location_return) {
  NOTIMPLEMENTED();
  return false;
}

bool RootWindowHostMac::ConfineCursorToRootWindow() {
  return false;
}

void RootWindowHostMac::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::OnCursorVisibilityChanged(bool show) {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::PostNativeEvent(const base::NativeEvent& event) {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::OnDeviceScaleFactorChanged(float device_scale_factor) {
  NOTIMPLEMENTED();
}

void RootWindowHostMac::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

// static
RootWindowHost* RootWindowHost::Create(const gfx::Rect& bounds) {
  return new RootWindowHostMac(bounds);
}

// static
gfx::Size RootWindowHost::GetNativeScreenSize() {
  NOTIMPLEMENTED();
  return gfx::Size(1024, 768);
}

}  // namespace aura
