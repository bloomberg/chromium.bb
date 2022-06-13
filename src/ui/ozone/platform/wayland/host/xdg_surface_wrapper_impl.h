// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_IMPL_H_

#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"

#include <cstdint>

#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Surface wrapper for xdg-shell stable
class XDGSurfaceWrapperImpl : public ShellSurfaceWrapper {
 public:
  XDGSurfaceWrapperImpl(WaylandWindow* wayland_window,
                        WaylandConnection* connection);
  XDGSurfaceWrapperImpl(const XDGSurfaceWrapperImpl&) = delete;
  XDGSurfaceWrapperImpl& operator=(const XDGSurfaceWrapperImpl&) = delete;
  ~XDGSurfaceWrapperImpl() override;

  // ShellSurfaceWrapper overrides:
  bool Initialize() override;
  void AckConfigure(uint32_t serial) override;
  bool IsConfigured() override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;

  // xdg_surface_listener
  static void Configure(void* data,
                        struct xdg_surface* xdg_surface,
                        uint32_t serial);

  struct xdg_surface* xdg_surface() const;

 private:
  // Non-owing WaylandWindow that uses this surface wrapper.
  WaylandWindow* const wayland_window_;
  WaylandConnection* const connection_;

  bool is_configured_ = false;

  wl::Object<struct xdg_surface> xdg_surface_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_SURFACE_WRAPPER_IMPL_H_
