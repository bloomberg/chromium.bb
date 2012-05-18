// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_H_
#define UI_BASE_EVENTS_H_
#pragma once

#include "base/event_types.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace gfx {
class Point;
}

namespace base {
class TimeDelta;
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
  ET_MOUSE_CAPTURE_CHANGED,  // Event has no location.
  ET_TOUCH_RELEASED,
  ET_TOUCH_PRESSED,
  ET_TOUCH_MOVED,
  ET_TOUCH_STATIONARY,
  ET_TOUCH_CANCELLED,
  ET_DROP_TARGET_EVENT,
  ET_FOCUS_CHANGE,
  ET_TRANSLATED_KEY_PRESS,
  ET_TRANSLATED_KEY_RELEASE,

  // GestureEvent types
  ET_GESTURE_SCROLL_BEGIN,
  ET_GESTURE_SCROLL_END,
  ET_GESTURE_SCROLL_UPDATE,
  ET_GESTURE_TAP,
  ET_GESTURE_TAP_DOWN,
  ET_GESTURE_DOUBLE_TAP,
  ET_GESTURE_PINCH_BEGIN,
  ET_GESTURE_PINCH_END,
  ET_GESTURE_PINCH_UPDATE,
  ET_GESTURE_LONG_PRESS,
  ET_GESTURE_THREE_FINGER_SWIPE,

  // Scroll support.
  // TODO[davemoore] we need to unify these events w/ touch and gestures.
  ET_SCROLL,
  ET_SCROLL_FLING_START,
  ET_SCROLL_FLING_CANCEL,
};

// Event flags currently supported
enum EventFlags {
  EF_NONE                = 0,       // Used to denote no flags explicitly
  EF_CAPS_LOCK_DOWN      = 1 << 0,
  EF_SHIFT_DOWN          = 1 << 1,
  EF_CONTROL_DOWN        = 1 << 2,
  EF_ALT_DOWN            = 1 << 3,
  EF_LEFT_MOUSE_BUTTON   = 1 << 4,
  EF_MIDDLE_MOUSE_BUTTON = 1 << 5,
  EF_RIGHT_MOUSE_BUTTON  = 1 << 6,
  EF_COMMAND_DOWN        = 1 << 7,  // Only useful on OSX
  EF_EXTENDED            = 1 << 8,  // Windows extended key (see WM_KEYDOWN doc)
};

// Flags specific to mouse events
enum MouseEventFlags {
  EF_IS_DOUBLE_CLICK    = 1 << 16,
  EF_IS_TRIPLE_CLICK    = 1 << 17,
  EF_IS_NON_CLIENT      = 1 << 18,
  EF_IS_SYNTHESIZED     = 1 << 19,  // Only for Aura.  See ui/aura/root_window.h
  EF_FROM_TOUCH         = 1 << 20,  // Indicates this mouse event is generated
                                    // from an unconsumed touch/gesture event.
};

enum TouchStatus {
  TOUCH_STATUS_UNKNOWN = 0,  // Unknown touch status. This is used to indicate
                             // that the touch event was not handled.
  TOUCH_STATUS_START,        // The touch event initiated a touch sequence.
  TOUCH_STATUS_CONTINUE,     // The touch event is part of a previously
                             // started touch sequence.
  TOUCH_STATUS_END,          // The touch event ended the touch sequence.
  TOUCH_STATUS_SYNTH_MOUSE,  // The touch event was not processed, but a
                             // synthetic mouse event generated from the
                             // unused touch event was handled.
  TOUCH_STATUS_QUEUED,       // The touch event has not been processed yet, but
                             // may be processed asynchronously later. This also
                             // places a lock on touch-events (i.e. all
                             // subsequent touch-events should be sent to the
                             // current handler).
  TOUCH_STATUS_QUEUED_END,   // Similar to TOUCH_STATUS_QUEUED, except that
                             // subsequent touch-events can be sent to any
                             // handler.
};

// Updates the list of devices for cached properties.
UI_EXPORT void UpdateDeviceList();

enum GestureStatus {
  GESTURE_STATUS_UNKNOWN = 0,  // Unknown Gesture status. This is used to
                               // indicate that the Gesture event was not
                               // handled.
  GESTURE_STATUS_CONSUMED,     // The Gesture event got consumed.
  GESTURE_STATUS_SYNTH_MOUSE   // The Gesture event was not processed, but a
                               // synthetic mouse event generated from the
                               // unused Gesture event was handled.
};

// Get the EventType from a native event.
UI_EXPORT EventType EventTypeFromNative(const base::NativeEvent& native_event);

// Get the EventFlags from a native event.
UI_EXPORT int EventFlagsFromNative(const base::NativeEvent& native_event);

UI_EXPORT base::TimeDelta EventTimeFromNative(
    const base::NativeEvent& native_event);

// Get the location from a native event.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/root_window_host*.
UI_EXPORT gfx::Point EventLocationFromNative(
    const base::NativeEvent& native_event);

#if defined(USE_X11)
// Returns the 'real' button for an event. The button reported in slave events
// does not take into account any remapping (e.g. using xmodmap), while the
// button reported in master events do. This is a utility function to always
// return the mapped button.
UI_EXPORT int EventButtonFromNative(const base::NativeEvent& native_event);
#endif

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

// Gets the fling velocity from a native event. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
UI_EXPORT bool GetFlingData(const base::NativeEvent& native_event,
                            float* vx,
                            float* vy,
                            bool* is_cancel);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled. |x_offset| and |y_offset| can be NULL.
UI_EXPORT bool GetScrollOffsets(const base::NativeEvent& native_event,
                                float* x_offset,
                                float* y_offset);

UI_EXPORT bool GetGestureTimes(const base::NativeEvent& native_event,
                               double* start_time,
                               double* end_time);

// Enable/disable natural scrolling for touchpads.
UI_EXPORT void SetNaturalScroll(bool enabled);

// In natural scrolling enabled for touchpads?
UI_EXPORT bool IsNaturalScrollEnabled();

// Was this event generated by a touchpad device?
// The caller is responsible for ensuring that this is a mouse/touchpad event
// before calling this function.
UI_EXPORT bool IsTouchpadEvent(const base::NativeEvent& event);

// Returns true if event is noop.
UI_EXPORT bool IsNoopEvent(const base::NativeEvent& event);

// Creates and returns no-op event.
UI_EXPORT base::NativeEvent CreateNoopEvent();

#if defined(OS_WIN)
int GetModifiersFromACCEL(const ACCEL& accel);
#endif

}  // namespace ui

#endif  // UI_BASE_EVENTS_H_
