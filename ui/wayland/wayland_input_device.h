// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_INPUT_DEVICE_H_
#define UI_WAYLAND_WAYLAND_INPUT_DEVICE_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "ui/gfx/point.h"
#include "ui/wayland/wayland_widget.h"

struct xkb_desc;
struct wl_array;
struct wl_buffer;
struct wl_display;
struct wl_input_device;
struct wl_surface;

namespace ui {

class WaylandWindow;

// This class represents an input device that was registered with Wayland.
// The purpose of this class is to parse and wrap events into generic
// WaylandEvent types and dispatch the event to the appropriate WaylandWindow.
//
// How Wayland events work:
// ------------------------
//
// When the On*Focus events are triggered, the input device receives a
// reference to the surface that just received/lost focus. Each surface is
// associated with a unique WaylandWindow. When processing the focus events we
// keep track of the currently focused window such that when we receive
// different events (mouse button press or key press) we only send the event to
// the window in focus.
class WaylandInputDevice {
 public:
  WaylandInputDevice(wl_display* display, uint32_t id);
  ~WaylandInputDevice();

  // Used to change the surface of the input device (normally pointer image).
  void Attach(wl_buffer* buffer, int32_t x, int32_t y);

 private:
  // Input device callback functions. These will create 'WaylandEvent's and
  // send them to the currently focused window.
  // Args:
  //  - data: Pointer to the WaylandInputDevice object associated with the
  //          'input_device'
  //  - input_device:
  //          The input device that sent the event
  //  - time: The time of the event.
  static void OnMotionNotify(void* data,
                             wl_input_device* input_device,
                             uint32_t time,
                             int32_t x,
                             int32_t y,
                             int32_t sx,
                             int32_t sy);

  static void OnButtonNotify(void* data,
                             wl_input_device* input_device,
                             uint32_t time,
                             uint32_t button,
                             uint32_t state);

  static void OnKeyNotify(void* data,
                          wl_input_device* input_device,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state);

  // On*Focus events also have a Wayland surface associated with them. If the
  // surface is NULL, then the event signifies a loss of focus. Otherwise we
  // use the surface to get the WaylandWindow that receives focus.
  static void OnPointerFocus(void* data,
                             wl_input_device* input_device,
                             uint32_t time,
                             wl_surface* surface,
                             int32_t x,
                             int32_t y,
                             int32_t sx,
                             int32_t sy);

  static void OnKeyboardFocus(void* data,
                              wl_input_device* input_device,
                              uint32_t time,
                              wl_surface* surface,
                              wl_array* keys);

  wl_input_device* input_device_;

  // These keep track of the window that's currently under focus. NULL if no
  // window is under focus.
  WaylandWindow* pointer_focus_;
  WaylandWindow* keyboard_focus_;

  // Keeps track of the currently active keyboard modifiers. We keep this
  // since we want to advertise keyboard modifiers with mouse events.
  uint32_t keyboard_modifiers_;

  // Keeps track of the last position for the motion event. We want to
  // publish this with events such as button notify which doesn't have a
  // position associated by default.
  gfx::Point global_position_;
  gfx::Point surface_position_;

  // Keep track of the time of last event. Useful when we get buffer Attach
  // calls and the calls wouldn't have a way of getting an event time.
  uint32_t last_event_time_;

  // keymap used to transform keyboard events.
  xkb_desc* xkb_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputDevice);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_INPUT_DEVICE_H_
