// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_H_
#define UI_BASE_EVENTS_H_
#pragma once

#include "base/event_types.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {

// Event types. (prefixed because of a conflict with windows headers)
enum EventType {
  ET_UNKNOWN = 0,
  ET_MOUSE_PRESSED,
  ET_MOUSE_DRAGGED,
  ET_MOUSE_RELEASED,
  ET_MOUSE_MOVED,
  ET_MOUSE_ENTERED,
  ET_MOUSE_EXITED,
  ET_KEY_PRESSED,
  ET_KEY_RELEASED,
  ET_MOUSEWHEEL,
  ET_TOUCH_RELEASED,
  ET_TOUCH_PRESSED,
  ET_TOUCH_MOVED,
  ET_TOUCH_STATIONARY,
  ET_TOUCH_CANCELLED,
  ET_DROP_TARGET_EVENT,
  ET_FOCUS_CHANGE,
};

// Event flags currently supported
enum EventFlags {
  EF_CAPS_LOCK_DOWN     = 1 << 0,
  EF_SHIFT_DOWN         = 1 << 1,
  EF_CONTROL_DOWN       = 1 << 2,
  EF_ALT_DOWN           = 1 << 3,
  EF_LEFT_BUTTON_DOWN   = 1 << 4,
  EF_MIDDLE_BUTTON_DOWN = 1 << 5,
  EF_RIGHT_BUTTON_DOWN  = 1 << 6,
  EF_COMMAND_DOWN       = 1 << 7,  // Only useful on OSX
  EF_EXTENDED           = 1 << 8,  // Windows extended key (see WM_KEYDOWN doc)
};

// Flags specific to mouse events
enum MouseEventFlags {
  EF_IS_DOUBLE_CLICK    = 1 << 16,
  EF_IS_NON_CLIENT      = 1 << 17
};

enum TouchStatus {
  TOUCH_STATUS_UNKNOWN = 0,  // Unknown touch status. This is used to indicate
                             // that the touch event was not handled.
  TOUCH_STATUS_START,        // The touch event initiated a touch sequence.
  TOUCH_STATUS_CONTINUE,     // The touch event is part of a previously
                             // started touch sequence.
  TOUCH_STATUS_END,          // The touch event ended the touch sequence.
  TOUCH_STATUS_CANCEL,       // The touch event was cancelled, but didn't
                             // terminate the touch sequence.
  TOUCH_STATUS_SYNTH_MOUSE   // The touch event was not processed, but a
                             // synthetic mouse event generated from the
                             // unused touch event was handled.
};

// Get the EventType from a native event.
UI_EXPORT EventType EventTypeFromNative(const base::NativeEvent& native_event);

// Get the EventFlags from a native event.
UI_EXPORT int EventFlagsFromNative(const base::NativeEvent& native_event);

// Get the location from a native event.
UI_EXPORT gfx::Point EventLocationFromNative(
    const base::NativeEvent& native_event);

// Returns the KeyboardCode from a native event.
UI_EXPORT KeyboardCode KeyboardCodeFromNative(
    const base::NativeEvent& native_event);

// Returns true if the message is a mouse event.
UI_EXPORT bool IsMouseEvent(const base::NativeEvent& native_event);

// Gets the mouse wheel offset from a native event.
UI_EXPORT int GetMouseWheelOffset(const base::NativeEvent& native_event);

// Gets the touch id from a native event.
UI_EXPORT int GetTouchId(const base::NativeEvent& native_event);

// Gets the radius along the X/Y axis from a native event. Default is 1.0.
UI_EXPORT float GetTouchRadiusX(const base::NativeEvent& native_event);
UI_EXPORT float GetTouchRadiusY(const base::NativeEvent& native_event);

// Gets the angle of the major axis away from the X axis. Default is 0.0.
UI_EXPORT float GetTouchAngle(const base::NativeEvent& native_event);

// Gets the force from a native_event. Normalized to be [0, 1]. Default is 0.0.
UI_EXPORT float GetTouchForce(const base::NativeEvent& native_event);

// Creates and returns no-op event.
UI_EXPORT base::NativeEvent CreateNoopEvent();

}  // namespace ui

#endif  // UI_BASE_EVENTS_H_
