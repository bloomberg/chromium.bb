// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_DISPLAY_H_
#define UI_WAYLAND_WAYLAND_DISPLAY_H_

#include <stdint.h>

#include <list>

#include "base/basictypes.h"

struct wl_compositor;
struct wl_display;
struct wl_shell;
struct wl_shm;
struct wl_surface;

namespace ui {

class WaylandBuffer;
class WaylandInputDevice;
class WaylandScreen;

// WaylandDisplay is a wrapper around wl_display. Once we get a valid
// wl_display, the Wayland server will send different events to register
// the Wayland compositor, shell, screens, input devices, ...
class WaylandDisplay {
 public:
  // Attempt to create a connection to the display. If it fails this returns
  // NULL
  static WaylandDisplay* Connect(char* name);

  // Get the WaylandDisplay associated with the native Wayland display
  static WaylandDisplay* GetDisplay(wl_display* display);

  ~WaylandDisplay();

  // Creates a wayland surface. This is used to create a window surface.
  // The returned pointer should be deleted by the caller.
  wl_surface* CreateSurface();

  // Sets the specified buffer as the surface for the cursor. (x, y) is
  // the hotspot for the cursor.
  void SetCursor(WaylandBuffer* buffer, int32_t x, int32_t y);

  // Returns a pointer to the wl_display.
  wl_display* display() const { return display_; }

  // Returns a list of the registered screens.
  std::list<WaylandScreen*> GetScreenList() const;

  wl_shell* shell() const { return shell_; }

  wl_shm* shm() const { return shm_; }

 private:
  WaylandDisplay(char* name);

  // This handler resolves all server events used in initialization. It also
  // handles input device registration, screen registration.
  static void DisplayHandleGlobal(wl_display* display,
                                  uint32_t id,
                                  const char* interface,
                                  uint32_t version,
                                  void* data);

  // Used when the shell requires configuration. This is called when a
  // window is configured and receives its size.
  // TODO(dnicoara) Need to look if there is one shell per window. Then it
  // makes more sense to move this into the WaylandWindow and it would keep
  // track of the shell.
  static void ShellHandleConfigure(void* data,
                                   wl_shell* shell,
                                   uint32_t time,
                                   uint32_t edges,
                                   wl_surface* surface,
                                   int32_t width,
                                   int32_t height);

  // WaylandDisplay manages the memory of all these pointers.
  wl_display* display_;
  wl_compositor* compositor_;
  wl_shell* shell_;
  wl_shm* shm_;
  std::list<WaylandScreen*> screen_list_;
  std::list<WaylandInputDevice*> input_list_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplay);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_DISPLAY_H_
