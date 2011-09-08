// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_H_
#define UI_BASE_EVENTS_H_
#pragma once

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

// Event flags currently supported.  Although this is a "views"
// file, this header is used on non-views platforms (e.g. OSX).  For
// example, these EventFlags are used by the automation provider for
// all platforms.
enum EventFlags {
  EF_CAPS_LOCK_DOWN     = 1 << 0,
  EF_SHIFT_DOWN         = 1 << 1,
  EF_CONTROL_DOWN       = 1 << 2,
  EF_ALT_DOWN           = 1 << 3,
  EF_LEFT_BUTTON_DOWN   = 1 << 4,
  EF_MIDDLE_BUTTON_DOWN = 1 << 5,
  EF_RIGHT_BUTTON_DOWN  = 1 << 6,
  EF_COMMAND_DOWN       = 1 << 7,  // Only useful on OSX
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

}  // namespace ui

#endif  // UI_BASE_EVENTS_H_

