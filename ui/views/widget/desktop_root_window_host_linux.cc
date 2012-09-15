// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_linux.h"

#include "ui/aura/root_window.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, public:

DesktopRootWindowHostLinux::DesktopRootWindowHostLinux() {
}

DesktopRootWindowHostLinux::~DesktopRootWindowHostLinux() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, DesktopRootWindowHost implementation:

void DesktopRootWindowHostLinux::Init(aura::Window* content_window,
                                      const Widget::InitParams& params) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Close() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::CloseNow() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

aura::RootWindowHost* DesktopRootWindowHostLinux::AsRootWindowHost() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

void DesktopRootWindowHostLinux::ShowWindowWithState(
    ui::WindowShowState show_state) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsVisible() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return true;
}

void DesktopRootWindowHostLinux::SetSize(const gfx::Size& size) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::CenterWindow(const gfx::Size& size) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
      // TODO(erg):
      NOTIMPLEMENTED();
}

gfx::Rect DesktopRootWindowHostLinux::GetWindowBoundsInScreen() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect DesktopRootWindowHostLinux::GetClientAreaBoundsInScreen() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect(100, 100);
}

gfx::Rect DesktopRootWindowHostLinux::GetRestoredBounds() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void DesktopRootWindowHostLinux::Activate() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Deactivate() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsActive() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return true;
}

void DesktopRootWindowHostLinux::Maximize() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Minimize() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Restore() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsMaximized() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return false;
}

bool DesktopRootWindowHostLinux::IsMinimized() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return false;
}

void DesktopRootWindowHostLinux::SetAlwaysOnTop(bool always_on_top) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

InputMethod* DesktopRootWindowHostLinux::CreateInputMethod() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

internal::InputMethodDelegate*
    DesktopRootWindowHostLinux::GetInputMethodDelegate() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

void DesktopRootWindowHostLinux::SetWindowTitle(const string16& title) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    const gfx::Rect& initial_bounds) {
  return new DesktopRootWindowHostLinux;
}

}  // namespace views
