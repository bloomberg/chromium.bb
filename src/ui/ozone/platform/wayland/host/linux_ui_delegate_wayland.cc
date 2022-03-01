// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/linux_ui_delegate_wayland.h"

#include <utility>

#include "base/logging.h"
#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

LinuxUiDelegateWayland::LinuxUiDelegateWayland(WaylandConnection* connection)
    : connection_(connection) {
  DCHECK(connection_);
}

LinuxUiDelegateWayland::~LinuxUiDelegateWayland() = default;

LinuxUiBackend LinuxUiDelegateWayland::GetBackend() const {
  return LinuxUiBackend::kWayland;
}

bool LinuxUiDelegateWayland::ExportWindowHandle(
    gfx::AcceleratedWidget parent,
    base::OnceCallback<void(const std::string&)> callback) {
  auto* parent_window =
      connection_->wayland_window_manager()->GetWindow(parent);
  auto* foreign = connection_->xdg_foreign();
  if (!parent_window || !foreign)
    return false;

  DCHECK_EQ(parent_window->type(), PlatformWindowType::kWindow);

  foreign->ExportSurfaceToForeign(parent_window, std::move(callback));
  return true;
}

}  // namespace ui
