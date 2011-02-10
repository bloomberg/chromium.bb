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
#if defined(TOUCH_UI)
  ET_TOUCH_RELEASED,
  ET_TOUCH_PRESSED,
  ET_TOUCH_MOVED,
  ET_TOUCH_STATIONARY,
  ET_TOUCH_CANCELLED,
#endif
  ET_DROP_TARGET_EVENT
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

}  // namespace ui

#endif  // UI_BASE_EVENTS_H_

