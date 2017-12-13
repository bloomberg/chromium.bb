// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_XDG_SURFACE_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_XDG_SURFACE_WRAPPER_H_

#include "base/strings/string16.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;

// Wrapper interface for different xdg shell versions.
class XDGSurfaceWrapper {
 public:
  virtual ~XDGSurfaceWrapper() {}

  // Initializes the surface.
  virtual bool Initialize(WaylandConnection* connection,
                          wl_surface* surface) = 0;

  // Sets a native window to maximized state.
  virtual void SetMaximized() = 0;

  // Unsets a native window from maximized state.
  virtual void UnSetMaximized() = 0;

  // Sets a native window to fullscreen state.
  virtual void SetFullscreen() = 0;

  // Unsets a native window from fullscreen state.
  virtual void UnSetFullscreen() = 0;

  // Sets a native window to minimized state.
  virtual void SetMinimized() = 0;

  // Tells wayland to start interactive window drag.
  virtual void SurfaceMove(WaylandConnection* connection) = 0;

  // Tells wayland to start interactive window resize.
  virtual void SurfaceResize(WaylandConnection* connection,
                             uint32_t hittest) = 0;

  // Sets a title of a native window.
  virtual void SetTitle(const base::string16& title) = 0;

  // Sends acknowledge configure event back to wayland.
  virtual void AckConfigure() = 0;

  // Sets a desired window geometry once wayland requests client to do so.
  virtual void SetWindowGeometry(const gfx::Rect& bounds) = 0;
};

bool CheckIfWlArrayHasValue(struct wl_array* wl_array, uint32_t value);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_XDG_SURFACE_WRAPPER_H_
