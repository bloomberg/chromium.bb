// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_input_device.h"

#include <X11/extensions/XKBcommon.h>
#include <wayland-client.h>

#include "ui/wayland/events/wayland_event.h"
#include "ui/wayland/wayland_widget.h"
#include "ui/wayland/wayland_window.h"

namespace ui {

WaylandInputDevice::WaylandInputDevice(
    wl_display* display,
    uint32_t id)
    : input_device_(static_cast<wl_input_device*>(
          wl_display_bind(display, id, &wl_input_device_interface))),
      pointer_focus_(NULL),
      keyboard_focus_(NULL),
      keyboard_modifiers_(0) {

  // List of callback functions for input device events.
  static const struct wl_input_device_listener kInputDeviceListener = {
    WaylandInputDevice::OnMotionNotify,
    WaylandInputDevice::OnButtonNotify,
    WaylandInputDevice::OnKeyNotify,
    WaylandInputDevice::OnPointerFocus,
    WaylandInputDevice::OnKeyboardFocus,
  };

  wl_input_device_add_listener(input_device_, &kInputDeviceListener, this);
  wl_input_device_set_user_data(input_device_, this);

  struct xkb_rule_names names;
  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";

  xkb_ = xkb_compile_keymap_from_rules(&names);
}

WaylandInputDevice::~WaylandInputDevice() {
  if (input_device_)
    wl_input_device_destroy(input_device_);
}

void WaylandInputDevice::Attach(wl_buffer* buffer, int32_t x, int32_t y) {
  wl_input_device_attach(input_device_, last_event_time_, buffer, x, y);
}

void WaylandInputDevice::OnMotionNotify(void* data,
                                        wl_input_device* input_device,
                                        uint32_t time,
                                        int32_t x,
                                        int32_t y,
                                        int32_t sx,
                                        int32_t sy) {
  WaylandInputDevice* device = static_cast<WaylandInputDevice*>(data);
  WaylandWindow* window = device->pointer_focus_;

  device->last_event_time_ = time;
  device->global_position_.SetPoint(x, y);
  device->surface_position_.SetPoint(sx, sy);

  WaylandEvent event;
  event.type = WAYLAND_MOTION;
  event.motion.time = time;
  event.motion.modifiers = device->keyboard_modifiers_;
  event.motion.x = sx;
  event.motion.y = sy;

  window->widget()->OnMotionNotify(event);
}

void WaylandInputDevice::OnButtonNotify(void* data,
                                        wl_input_device* input_device,
                                        uint32_t time,
                                        uint32_t button,
                                        uint32_t state) {
  WaylandInputDevice* device = static_cast<WaylandInputDevice*>(data);
  WaylandWindow* window = device->pointer_focus_;

  device->last_event_time_ = time;

  WaylandEvent event;
  event.type = WAYLAND_BUTTON;
  event.button.time = time;
  event.button.button = button;
  event.button.state = state;
  event.button.modifiers = device->keyboard_modifiers_;
  event.button.x = device->surface_position_.x();
  event.button.y = device->surface_position_.y();

  window->widget()->OnButtonNotify(event);
}

void WaylandInputDevice::OnKeyNotify(void* data,
                                     wl_input_device* input_device,
                                     uint32_t time,
                                     uint32_t key,
                                     uint32_t state) {
  WaylandInputDevice* device = static_cast<WaylandInputDevice*>(data);
  WaylandWindow* window = device->keyboard_focus_;
  struct xkb_desc *xkb = device->xkb_;

  device->last_event_time_ = time;

  WaylandEvent event;
  event.type = WAYLAND_KEY;
  event.key.time = time;
  event.key.key = key;
  event.key.state = state;

  uint32_t code = key + xkb->min_key_code;
  uint32_t level = 0;
  if ((device->keyboard_modifiers_ & XKB_COMMON_SHIFT_MASK) &&
      XkbKeyGroupWidth(xkb, code, 0) > 1) {
    level = 1;
  }

  event.key.sym = XkbKeySymEntry(xkb, code, level, 0);
  if (state)
    device->keyboard_modifiers_ |= xkb->map->modmap[code];
  else
    device->keyboard_modifiers_ &= ~xkb->map->modmap[code];

  event.key.modifiers = device->keyboard_modifiers_;

  window->widget()->OnKeyNotify(event);
}

void WaylandInputDevice::OnPointerFocus(void* data,
                                        wl_input_device* input_device,
                                        uint32_t time,
                                        wl_surface* surface,
                                        int32_t x,
                                        int32_t y,
                                        int32_t sx,
                                        int32_t sy) {
  WaylandInputDevice* device = static_cast<WaylandInputDevice*>(data);
  WaylandWindow* window = device->pointer_focus_;

  device->last_event_time_ = time;

  WaylandEvent event;
  event.type = WAYLAND_POINTER_FOCUS;
  event.pointer_focus.time = time;
  event.pointer_focus.x = sx;
  event.pointer_focus.y = sy;

  // If we have a window, then this means it loses focus
  if (window) {
    event.pointer_focus.state = 0;
    device->pointer_focus_ = NULL;
    window->widget()->OnPointerFocus(event);
  }

  // If we have a surface, then a new window is in focus
  if (surface) {
    event.pointer_focus.state = 1;
    window = static_cast<WaylandWindow*>(wl_surface_get_user_data(surface));
    device->pointer_focus_ = window;
    window->widget()->OnPointerFocus(event);
  }
}

void WaylandInputDevice::OnKeyboardFocus(void* data,
                                         wl_input_device* input_device,
                                         uint32_t time,
                                         wl_surface* surface,
                                         wl_array* keys) {
  WaylandInputDevice* device = static_cast<WaylandInputDevice*>(data);
  WaylandWindow* window = device->keyboard_focus_;
  struct xkb_desc* xkb = device->xkb_;

  device->last_event_time_ = time;

  WaylandEvent event;
  event.type = WAYLAND_KEYBOARD_FOCUS;
  event.keyboard_focus.time = time;
  device->keyboard_modifiers_ = 0;

  uint32_t* codes = static_cast<uint32_t*>(keys->data);
  int codes_size = keys->size / sizeof(uint32_t);
  for (int i = 0; i < codes_size; i++) {
    uint32_t code = codes[i] + xkb->min_key_code;
    device->keyboard_modifiers_ |= xkb->map->modmap[code];
  }
  event.keyboard_focus.modifiers = device->keyboard_modifiers_;

  // If there is a window, then it loses focus
  if (window) {
    event.keyboard_focus.state = 0;
    device->keyboard_focus_ = NULL;
    window->widget()->OnKeyboardFocus(event);
  }

  // If we have a surface, then a window gains focus
  if (surface) {
    event.keyboard_focus.state = 1;
    window = static_cast<WaylandWindow*>(wl_surface_get_user_data(surface));
    device->keyboard_focus_ = window;
    window->widget()->OnKeyboardFocus(event);
  }
}

}  // namespace ui
