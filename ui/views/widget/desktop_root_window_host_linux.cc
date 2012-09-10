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

bool DesktopRootWindowHostLinux::IsVisible() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return true;
}

gfx::Rect DesktopRootWindowHostLinux::GetClientAreaBoundsInScreen() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect(100, 100);
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
