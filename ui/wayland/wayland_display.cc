// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_display.h"

#include <string.h>
#include <wayland-client.h>

#include "ui/wayland/wayland_buffer.h"
#include "ui/wayland/wayland_input_device.h"
#include "ui/wayland/wayland_screen.h"
#include "ui/wayland/wayland_window.h"

namespace ui {

// static
WaylandDisplay* WaylandDisplay::Connect(char* name) {
  WaylandDisplay* display = new WaylandDisplay(name);
  if (!display->display_) {
    delete display;
    return NULL;
  }

  wl_display_set_user_data(display->display_, display);
  // Register the display initialization handler and iterate over the initial
  // connection events sent by the server. This is required since the display
  // will send registration events needed to initialize everything else. This
  // will create the compositor, etc.., which are required in creating
  // a drawing context.
  wl_display_add_global_listener(display->display_,
                                 WaylandDisplay::DisplayHandleGlobal,
                                 display);
  wl_display_iterate(display->display_, WL_DISPLAY_READABLE);

  return display;
}

// static
WaylandDisplay* WaylandDisplay::GetDisplay(wl_display* display) {
  return static_cast<WaylandDisplay*>(wl_display_get_user_data(display));
}

WaylandDisplay::WaylandDisplay(char* name) : display_(NULL),
                                             compositor_(NULL),
                                             shell_(NULL),
                                             shm_(NULL) {
  display_ = wl_display_connect(name);
}

WaylandDisplay::~WaylandDisplay() {
  if (display_)
    wl_display_destroy(display_);
  if (compositor_)
    wl_compositor_destroy(compositor_);
  if (shell_)
    wl_shell_destroy(shell_);
  if (shm_)
    wl_shm_destroy(shm_);
  for (std::list<WaylandInputDevice*>::iterator i = input_list_.begin();
       i != input_list_.end(); ++i) {
    delete *i;
  }
  for (std::list<WaylandScreen*>::iterator i = screen_list_.begin();
       i != screen_list_.end(); ++i) {
    delete *i;
  }
}

wl_surface* WaylandDisplay::CreateSurface() {
  return wl_compositor_create_surface(compositor_);
}

void WaylandDisplay::SetCursor(WaylandBuffer* buffer,
                               int32_t x, int32_t y) {
  // Currently there is no way of knowing which input device should have the
  // buffer attached, so we just attach to every input device.
  for (std::list<WaylandInputDevice*>::iterator i = input_list_.begin();
       i != input_list_.end(); ++i) {
    (*i)->Attach(buffer->buffer(), x, y);
  }
}

std::list<WaylandScreen*> WaylandDisplay::GetScreenList() const {
  return screen_list_;
}

// static
void WaylandDisplay::DisplayHandleGlobal(wl_display* display,
                                         uint32_t id,
                                         const char* interface,
                                         uint32_t version,
                                         void* data) {
  WaylandDisplay* disp = static_cast<WaylandDisplay*>(data);

  static const wl_shell_listener kShellListener = {
    WaylandDisplay::ShellHandleConfigure,
  };

  if (strcmp(interface, "wl_compositor") == 0) {
    disp->compositor_ = static_cast<wl_compositor*>(
        wl_display_bind(display, id, &wl_compositor_interface));
  } else if (strcmp(interface, "wl_output") == 0) {
    WaylandScreen* screen = new WaylandScreen(disp, id);
    disp->screen_list_.push_back(screen);
  } else if (strcmp(interface, "wl_input_device") == 0) {
    WaylandInputDevice *input_device = new WaylandInputDevice(display, id);
    disp->input_list_.push_back(input_device);
  } else if (strcmp(interface, "wl_shell") == 0) {
    disp->shell_ = static_cast<wl_shell*>(
        wl_display_bind(display, id, &wl_shell_interface));
    wl_shell_add_listener(disp->shell_, &kShellListener, disp);
  } else if (strcmp(interface, "wl_shm") == 0) {
    disp->shm_ = static_cast<wl_shm*>(
        wl_display_bind(display, id, &wl_shm_interface));
  }
}

// static
void WaylandDisplay::ShellHandleConfigure(void* data,
                                          wl_shell* shell,
                                          uint32_t time,
                                          uint32_t edges,
                                          wl_surface* surface,
                                          int32_t width,
                                          int32_t height) {
  WaylandWindow* window = static_cast<WaylandWindow*>(
      wl_surface_get_user_data(surface));
  window->Configure(time, edges, 0, 0, width, height);
}

}  // namespace ui
