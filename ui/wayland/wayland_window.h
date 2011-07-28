// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_WINDOW_H_
#define UI_WAYLAND_WAYLAND_WINDOW_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "ui/gfx/point.h"

struct wl_surface;

namespace ui {

class WaylandDisplay;
class WaylandWidget;

// WaylandWindow wraps a wl_surface and some basic operations for the surface.
// WaylandWindow also keeps track of the WaylandWidget that will process all
// events related to the window.
class WaylandWindow {
 public:
  // Creates a toplevel window.
  WaylandWindow(WaylandWidget* widget, WaylandDisplay* display);
  // Creates a transient window with an offset of (x,y) from parent.
  WaylandWindow(WaylandWidget* widget,
                WaylandDisplay* display,
                WaylandWindow* parent,
                int32_t x,
                int32_t y);

  ~WaylandWindow();

  void SetVisible(bool visible);
  bool IsVisible() const;

  // Sets the window to fullscreen if |fullscreen| is true. Otherwise it sets
  // it as a normal window.
  void set_fullscreen(bool fullscreen) { fullscreen_ = fullscreen; }
  bool fullscreen() const { return fullscreen_; }

  // Returns a pointer to the parent window. NULL is this window doesn't have
  // a parent.
  WaylandWindow* parent_window() const { return parent_window_; }

  WaylandWidget* widget() const { return widget_; }

  // Returns the pointer to the surface associated with the window.
  // The WaylandWindow object owns the pointer.
  wl_surface* surface() const { return surface_; }

  void Configure(uint32_t time, uint32_t edges, int32_t x, int32_t y,
                 int32_t width, int32_t height);

 private:
  // The widget that will process events for this window. This is not owned
  // by the window.
  WaylandWidget* widget_;

  // Pointer to the display this window is using. This doesn't own the pointer
  // to the display.
  WaylandDisplay* display_;

  // When creating a transient window, |parent_window_| is set to point to the
  // parent of this window. We will then use |parent_window_| to align this
  // window at the specified offset in |relative_position_|.
  // |parent_window_| is not owned by this window.
  WaylandWindow* parent_window_;

  // Position relative to parent window. This is only used by
  // a transient window.
  gfx::Point relative_position_;

  // The native wayland surface associated with this window.
  wl_surface* surface_;

  // Whether the window is in fullscreen mode.
  bool fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_WINDOW_H_
