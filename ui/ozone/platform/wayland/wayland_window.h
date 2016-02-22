// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class WaylandDisplay;

class WaylandWindow : public PlatformWindow {
 public:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandDisplay* display,
                const gfx::Rect& bounds);
  ~WaylandWindow() override;

  bool Initialize();

  wl_surface* GetSurface();
  gfx::AcceleratedWidget GetWidget();

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // PlatformWindow
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;

  // xdg_surface_listener
  static void Configure(void* data,
                        xdg_surface* obj,
                        int32_t width,
                        int32_t height,
                        wl_array* states,
                        uint32_t serial);
  static void Close(void* data, xdg_surface* obj);

 private:
  PlatformWindowDelegate* delegate_;
  WaylandDisplay* display_;

  wl::Object<wl_surface> surface_;
  wl::Object<xdg_surface> xdg_surface_;

  gfx::Rect bounds_;
  gfx::Rect pending_bounds_;
  uint32_t pending_configure_serial_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
