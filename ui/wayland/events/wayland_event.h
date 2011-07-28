// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_EVENTS_WAYLAND_EVENT_H_
#define UI_WAYLAND_EVENTS_WAYLAND_EVENT_H_

#include <linux/input.h>
#include <stdint.h>

// Wayland event information is being passed in as arguments to the callbacks.
// (See wayland_input_device.{h,cc} for information on the callbacks and how
// events are processed.)
// In order to provide a more generic look for events we wrap these arguments
// in specific event structs. Then define a WaylandEvent as a union of all
// types of events that Wayland will send.
//
// The following fields are common for most event types and their use is
// similar:
// - time:
//    The time of the event. This should be monotonically increasing.
// - state:
//    The value of the button event as given by evdev. This is 0 if button
//    isn't pressed.
// - modifiers:
//    Stores all the keyboard modifiers (Ctrl, Alt, Shift, ...) currently
//    active. The modifiers are values as defined by xkbcommon.

namespace ui {

// Types of events Wayland will send
enum WaylandEventType {
  WAYLAND_BUTTON,
  WAYLAND_KEY,
  WAYLAND_MOTION,
  WAYLAND_POINTER_FOCUS,
  WAYLAND_KEYBOARD_FOCUS,
  WAYLAND_GEOMETRY_CHANGE,
};

// These are the mouse events expected. The event type Wayland sends is an
// evdev event. The following is the correct mapping from evdev to expected
// events type.
enum WaylandEventButtonType {
  LEFT_BUTTON     = BTN_LEFT,
  MIDDLE_BUTTON   = BTN_RIGHT,
  RIGHT_BUTTON    = BTN_MIDDLE,
  SCROLL_UP       = BTN_SIDE,
  SCROLL_DOWN     = BTN_EXTRA,
};

struct WaylandEventButton {
  WaylandEventType type;
  uint32_t time;
  // WaylandEventButtonType defines some of the values button can take
  uint32_t button;
  uint32_t state;
  uint32_t modifiers;
  int32_t x;
  int32_t y;
};

struct WaylandEventKey {
  WaylandEventType type;
  uint32_t time;
  // The raw key value that evdev returns.
  uint32_t key;
  // The key symbol returned by processing the raw key using the xkbcommon
  // library.
  uint32_t sym;
  uint32_t state;
  uint32_t modifiers;
};

// Triggered when there is a motion event. The motion event is triggered
// only if there is a window under focus.
struct WaylandEventMotion {
  WaylandEventType type;
  uint32_t time;
  uint32_t modifiers;
  int32_t x;
  int32_t y;
};

// Triggered when a window enters/exits pointer focus. The state tells us
// if the window lost focus (state == 0) or gained focus (state != 0).
struct WaylandEventPointerFocus {
  WaylandEventType type;
  uint32_t time;
  uint32_t state;
  int32_t x;
  int32_t y;
};

// Triggered when a window enters/exits keyboard focus. The state tells us
// if the window lost focus (state == 0) or gained focus (state != 0).
struct WaylandEventKeyboardFocus {
  WaylandEventType type;
  uint32_t time;
  uint32_t state;
  uint32_t modifiers;
};

// Event triggered when a window's geometry changes. The event contains the
// position and dimensions of the window.
struct WaylandEventGeometryChange {
  WaylandEventType type;
  uint32_t time;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

union WaylandEvent {
  WaylandEventType            type;
  WaylandEventButton          button;
  WaylandEventKey             key;
  WaylandEventMotion          motion;
  WaylandEventPointerFocus    pointer_focus;
  WaylandEventKeyboardFocus   keyboard_focus;
  WaylandEventGeometryChange  geometry_change;
};

}  // namespace ui

#endif  // UI_WAYLAND_EVENTS_WAYLAND_EVENT_H_
