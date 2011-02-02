// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENTS_EVENT_H_
#define UI_VIEWS_EVENTS_EVENT_H_
#pragma once

#include "base/basictypes.h"
#include "gfx/point.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/native_types.h"

class OSExchangeData;

namespace ui {

class View;

class Event {
 public:
  // Event types.
  enum EventType { ET_UNKNOWN = 0,
                   ET_MOUSE_PRESSED,
                   ET_MOUSE_DRAGGED,
                   ET_MOUSE_RELEASED,
                   ET_MOUSE_MOVED,
                   ET_MOUSE_ENTERED,
                   ET_MOUSE_EXITED,
                   ET_KEY_PRESSED,
                   ET_KEY_RELEASED,
                   ET_MOUSEWHEEL,
                   ET_DROP_TARGET_EVENT };

  // Event flags currently supported.  Although this is a "views"
  // file, this header is used on non-views platforms (e.g. OSX).  For
  // example, these EventFlags are used by the automation provider for
  // all platforms.
  enum EventFlags { EF_CAPS_LOCK_DOWN     = 1 << 0,
                    EF_SHIFT_DOWN         = 1 << 1,
                    EF_CONTROL_DOWN       = 1 << 2,
                    EF_ALT_DOWN           = 1 << 3,
                    EF_LEFT_BUTTON_DOWN   = 1 << 4,
                    EF_MIDDLE_BUTTON_DOWN = 1 << 5,
                    EF_RIGHT_BUTTON_DOWN  = 1 << 6,
                    EF_COMMAND_DOWN       = 1 << 7,  // Only useful on OSX
  };

  EventType type() const { return type_; }
  int flags() const { return flags_; }
  void set_flags(int flags) { flags_ = flags; }

  // The following methods return true if the respective keys were pressed at
  // the time the event was created.
  bool IsShiftDown() const { return (flags_ & EF_SHIFT_DOWN) != 0; }
  bool IsControlDown() const { return (flags_ & EF_CONTROL_DOWN) != 0; }
  bool IsCapsLockDown() const { return (flags_ & EF_CAPS_LOCK_DOWN) != 0; }
  bool IsAltDown() const { return (flags_ & EF_ALT_DOWN) != 0; }

  // Return a mask of active modifier keycodes from
  // ui/base/keycodes/keyboard_codes.h
  int GetModifiers() const;

  // Returns true if the event is any kind of mouse event.
  bool IsMouseEvent() const {
    return type_ == ET_MOUSE_PRESSED ||
           type_ == ET_MOUSE_RELEASED ||
           type_ == ET_MOUSE_MOVED ||
           type_ == ET_MOUSE_EXITED ||
           type_ == ET_MOUSEWHEEL;
  }

 protected:
  Event(EventType type, int flags);

 private:
  EventType type_;
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(Event);
};

class LocatedEvent : public Event {
 public:
  int x() const { return location_.x(); }
  int y() const { return location_.y(); }
  const gfx::Point& location() const { return location_; }

 protected:
  // Constructors called from subclasses.

  // Simple initialization from cracked metadata.
  LocatedEvent(EventType type, const gfx::Point& location, int flags);

  // During event processing, event locations are translated from the
  // coordinates of a source View to a target as the tree is descended. This
  // translation occurs by constructing a new event from another event object,
  // specifying a |source| and |target| View to facilitate coordinate
  // conversion. Events that are processed in this manner will have a similar
  // constructor that calls into this one.
  LocatedEvent(const LocatedEvent& other, View* source, View* target);

 private:
  gfx::Point location_;

  DISALLOW_COPY_AND_ASSIGN(LocatedEvent);
};

class MouseEvent : public LocatedEvent {
 public:
  // Flags specific to mouse events
  enum MouseEventFlags {
    EF_IS_DOUBLE_CLICK = 1 << 16,
    EF_IS_NON_CLIENT = 1 << 17
  };

  explicit MouseEvent(NativeEvent native_event);

  MouseEvent(const MouseEvent& other, View* source, View* target);

  // Conveniences to quickly test what button is down:
  bool IsOnlyLeftMouseButton() const {
    return (flags() & EF_LEFT_BUTTON_DOWN) &&
        !(flags() & (EF_MIDDLE_BUTTON_DOWN | EF_RIGHT_BUTTON_DOWN));
  }
  bool IsLeftMouseButton() const {
    return (flags() & EF_LEFT_BUTTON_DOWN) != 0;
  }
  bool IsOnlyMiddleMouseButton() const {
    return (flags() & EF_MIDDLE_BUTTON_DOWN) &&
        !(flags() & (EF_LEFT_BUTTON_DOWN | EF_RIGHT_BUTTON_DOWN));
  }
  bool IsMiddleMouseButton() const {
    return (flags() & EF_MIDDLE_BUTTON_DOWN) != 0;
  }
  bool IsOnlyRightMouseButton() const {
    return (flags() & EF_RIGHT_BUTTON_DOWN) &&
        !(flags() & (EF_LEFT_BUTTON_DOWN | EF_MIDDLE_BUTTON_DOWN));
  }
  bool IsRightMouseButton() const {
    return (flags() & EF_RIGHT_BUTTON_DOWN) != 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseEvent);
};

class KeyEvent : public Event {
 public:
  explicit KeyEvent(NativeEvent native_event);

  KeyboardCode key_code() const { return key_code_; }

  int repeat_count() const { return repeat_count_; }

 private:
  KeyboardCode key_code_;
  int repeat_count_;
  int message_flags_;

  DISALLOW_COPY_AND_ASSIGN(KeyEvent);
};

class MouseWheelEvent : public LocatedEvent {
 public:
  explicit MouseWheelEvent(NativeEvent native_event);

  int offset() const { return offset_; }

 private:
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelEvent);
};

/*
class DropTargetEvent : public LocatedEvent {
 public:
  explicit DropTargetEvent(NativeEvent native_event);

  const OSExchangeData& data() const { return data_; }
  int source_operations() const { return source_operations_; }

 private:
  const OSExchangeData& data_;
  int source_operations_;

  DISALLOW_COPY_AND_ASSIGN(DropTargetEvent);
};
*/

}  // namespace ui

#endif  // UI_VIEWS_EVENTS_EVENT_H_
