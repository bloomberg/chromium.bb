// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EVENT_H_
#define VIEWS_EVENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"

#if defined(OS_LINUX)
typedef struct _GdkEventKey GdkEventKey;
#endif

#if defined(TOUCH_UI)
typedef union _XEvent XEvent;
#endif

namespace ui {
class OSExchangeData;
}
using ui::OSExchangeData;

namespace views {

class View;

////////////////////////////////////////////////////////////////////////////////
//
// Event class
//
// An event encapsulates an input event that can be propagated into view
// hierarchies. An event has a type, some flags and a time stamp.
//
// Each major event type has a corresponding Event subclass.
//
// Events are immutable but support copy
//
////////////////////////////////////////////////////////////////////////////////
class Event {
 public:
  // Event types. (prefixed because of a conflict with windows headers)
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
#if defined(TOUCH_UI)
                   ET_TOUCH_RELEASED,
                   ET_TOUCH_PRESSED,
                   ET_TOUCH_MOVED,
                   ET_TOUCH_STATIONARY,
                   ET_TOUCH_CANCELLED,
#endif
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

  // Return the event type
  EventType GetType() const {
    return type_;
  }

  // Return the event time stamp.
  const base::Time& GetTimeStamp() const {
    return time_stamp_;
  }

  // Return the flags
  int GetFlags() const {
    return flags_;
  }

  void set_flags(int flags) {
    flags_ = flags;
  }

  // Return whether the shift modifier is down
  bool IsShiftDown() const {
    return (flags_ & EF_SHIFT_DOWN) != 0;
  }

  // Return whether the control modifier is down
  bool IsControlDown() const {
    return (flags_ & EF_CONTROL_DOWN) != 0;
  }

  bool IsCapsLockDown() const {
    return (flags_ & EF_CAPS_LOCK_DOWN) != 0;
  }

  // Return whether the alt modifier is down
  bool IsAltDown() const {
    return (flags_ & EF_ALT_DOWN) != 0;
  }

  bool IsMouseEvent() const {
    return type_ == ET_MOUSE_PRESSED ||
           type_ == ET_MOUSE_DRAGGED ||
           type_ == ET_MOUSE_RELEASED ||
           type_ == ET_MOUSE_MOVED ||
           type_ == ET_MOUSE_ENTERED ||
           type_ == ET_MOUSE_EXITED ||
           type_ == ET_MOUSEWHEEL;
  }

#if defined(TOUCH_UI)
  bool IsTouchEvent() const {
    return type_ == ET_TOUCH_RELEASED ||
           type_ == ET_TOUCH_PRESSED ||
           type_ == ET_TOUCH_MOVED ||
           type_ == ET_TOUCH_STATIONARY ||
           type_ == ET_TOUCH_CANCELLED;
  }
#endif

#if defined(OS_WIN)
  // Returns the EventFlags in terms of windows flags.
  int GetWindowsFlags() const;

  // Convert windows flags to views::Event flags
  static int ConvertWindowsFlags(uint32 win_flags);
#elif defined(OS_LINUX)
  // Convert the state member on a GdkEvent to views::Event flags
  static int GetFlagsFromGdkState(int state);
#endif

 protected:
  Event(EventType type, int flags);

  Event(const Event& model)
      : type_(model.GetType()),
        time_stamp_(model.GetTimeStamp()),
        flags_(model.GetFlags()) {
  }

 private:
  void operator=(const Event&);

  EventType type_;
  base::Time time_stamp_;
  int flags_;
};

////////////////////////////////////////////////////////////////////////////////
//
// LocatedEvent class
//
// A generic event that is used for any events that is located at a specific
// position in the screen.
//
////////////////////////////////////////////////////////////////////////////////
class LocatedEvent : public Event {
 public:
  LocatedEvent(EventType type, const gfx::Point& location, int flags)
      : Event(type, flags),
        location_(location) {
  }

  // Create a new LocatedEvent which is identical to the provided model.
  // If from / to views are provided, the model location will be converted
  // from 'from' coordinate system to 'to' coordinate system
  LocatedEvent(const LocatedEvent& model, View* from, View* to);

  // Returns the X location.
  int x() const {
    return location_.x();
  }

  // Returns the Y location.
  int y() const {
    return location_.y();
  }

  // Returns the location.
  const gfx::Point& location() const {
    return location_;
  }

 private:
  gfx::Point location_;
};

////////////////////////////////////////////////////////////////////////////////
//
// MouseEvent class
//
// A mouse event is used for any input event related to the mouse.
//
////////////////////////////////////////////////////////////////////////////////
class MouseEvent : public LocatedEvent {
 public:
  // Flags specific to mouse events
  enum MouseEventFlags {
    EF_IS_DOUBLE_CLICK = 1 << 16,
    EF_IS_NON_CLIENT = 1 << 17
  };

  // Create a new mouse event
  MouseEvent(EventType type, int x, int y, int flags)
      : LocatedEvent(type, gfx::Point(x, y), flags) {
  }

  // Create a new mouse event from a type and a point. If from / to views
  // are provided, the point will be converted from 'from' coordinate system to
  // 'to' coordinate system.
  MouseEvent(EventType type,
             View* from,
             View* to,
             const gfx::Point &l,
             int flags);

  // Create a new MouseEvent which is identical to the provided model.
  // If from / to views are provided, the model location will be converted
  // from 'from' coordinate system to 'to' coordinate system
  MouseEvent(const MouseEvent& model, View* from, View* to);

#if defined(TOUCH_UI)
  // Create a mouse event from an X mouse event.
  explicit MouseEvent(XEvent* xevent);
#endif

  // Conveniences to quickly test what button is down
  bool IsOnlyLeftMouseButton() const {
    return (GetFlags() & EF_LEFT_BUTTON_DOWN) &&
      !(GetFlags() & (EF_MIDDLE_BUTTON_DOWN | EF_RIGHT_BUTTON_DOWN));
  }

  bool IsLeftMouseButton() const {
    return (GetFlags() & EF_LEFT_BUTTON_DOWN) != 0;
  }

  bool IsOnlyMiddleMouseButton() const {
    return (GetFlags() & EF_MIDDLE_BUTTON_DOWN) &&
      !(GetFlags() & (EF_LEFT_BUTTON_DOWN | EF_RIGHT_BUTTON_DOWN));
  }

  bool IsMiddleMouseButton() const {
    return (GetFlags() & EF_MIDDLE_BUTTON_DOWN) != 0;
  }

  bool IsOnlyRightMouseButton() const {
    return (GetFlags() & EF_RIGHT_BUTTON_DOWN) &&
      !(GetFlags() & (EF_LEFT_BUTTON_DOWN | EF_MIDDLE_BUTTON_DOWN));
  }

  bool IsRightMouseButton() const {
    return (GetFlags() & EF_RIGHT_BUTTON_DOWN) != 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseEvent);
};

#if defined(TOUCH_UI)
////////////////////////////////////////////////////////////////////////////////
//
// TouchEvent class
//
// A touch event is generated by touch screen and advanced track
// pad devices. There is a deliberate direct correspondence between
// TouchEvent and PlatformTouchPoint.
//
////////////////////////////////////////////////////////////////////////////////
class TouchEvent : public LocatedEvent {
 public:
  // Create a new touch event.
  TouchEvent(EventType type, int x, int y, int flags, int touch_id);

  // Create a new touch event from a type and a point. If from / to views
  // are provided, the point will be converted from 'from' coordinate system to
  // 'to' coordinate system.
  TouchEvent(EventType type,
             View* from,
             View* to,
             const gfx::Point& l,
             int flags,
             int touch_id);

  // Create a new TouchEvent which is identical to the provided model.
  // If from / to views are provided, the model location will be converted
  // from 'from' coordinate system to 'to' coordinate system.
  TouchEvent(const TouchEvent& model, View* from, View* to);

#if defined(HAVE_XINPUT2)
  explicit TouchEvent(XEvent* xev);
#endif

  // Return the touch point for this event.
  bool identity() const {
    return touch_id_;
  }

 private:
  // The identity (typically finger) of the touch starting at 0 and incrementing
  // for each separable additional touch that the hardware can detect.
  const int touch_id_;

  DISALLOW_COPY_AND_ASSIGN(TouchEvent);
};
#endif

////////////////////////////////////////////////////////////////////////////////
//
// KeyEvent class
//
// A key event is used for any input event related to the keyboard.
// Note: this event is about key pressed, not typed characters.
//
////////////////////////////////////////////////////////////////////////////////
class KeyEvent : public Event {
 public:
  // Create a new key event
  KeyEvent(EventType type,
           ui::KeyboardCode key_code,
           int event_flags,
           int repeat_count,
           int message_flags);

#if defined(OS_WIN)
  KeyEvent(EventType type,
           ui::KeyboardCode key_code,
           int event_flags,
           int repeat_count,
           int message_flags,
           UINT message);
#endif
#if defined(OS_LINUX)
  explicit KeyEvent(const GdkEventKey* event);

  const GdkEventKey* native_event() const { return native_event_; }
#endif
#if defined(TOUCH_UI)
  // Create a key event from an X key event.
  explicit KeyEvent(XEvent* xevent);
#endif

  // This returns a VKEY_ value as defined in app/keyboard_codes.h which is
  // the Windows value.
  // On GTK, you can use the methods in keyboard_code_conversion_gtk.cc to
  // convert this value back to a GDK value if needed.
  ui::KeyboardCode GetKeyCode() const {
    return key_code_;
  }

#if defined(OS_WIN)
  bool IsExtendedKey() const;

  UINT message() const {
    return message_;
  }
#endif

  int GetRepeatCount() const {
    return repeat_count_;
  }

#if defined(OS_WIN)
  static int GetKeyStateFlags();
#endif

 private:

  ui::KeyboardCode key_code_;
  int repeat_count_;
  int message_flags_;
#if defined(OS_WIN)
  UINT message_;
#elif defined(OS_LINUX)
  const GdkEventKey* native_event_;
#endif
  DISALLOW_COPY_AND_ASSIGN(KeyEvent);
};

////////////////////////////////////////////////////////////////////////////////
//
// MouseWheelEvent class
//
// A MouseWheelEvent is used to propagate mouse wheel user events
//
////////////////////////////////////////////////////////////////////////////////
class MouseWheelEvent : public LocatedEvent {
 public:
  // Create a new key event
  MouseWheelEvent(int offset, int x, int y, int flags)
      : LocatedEvent(ET_MOUSEWHEEL, gfx::Point(x, y), flags),
        offset_(offset) {
  }

#if defined(TOUCH_UI)
  explicit MouseWheelEvent(XEvent* xev);
#endif

  int GetOffset() const {
    return offset_;
  }

 private:
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelEvent);
};

////////////////////////////////////////////////////////////////////////////////
//
// DropTargetEvent class
//
// A DropTargetEvent is sent to the view the mouse is over during a drag and
// drop operation.
//
////////////////////////////////////////////////////////////////////////////////
class DropTargetEvent : public LocatedEvent {
 public:
  DropTargetEvent(const OSExchangeData& data,
                  int x,
                  int y,
                  int source_operations)
      : LocatedEvent(ET_DROP_TARGET_EVENT, gfx::Point(x, y), 0),
        data_(data),
        source_operations_(source_operations) {
  }

  // Data associated with the drag/drop session.
  const OSExchangeData& GetData() const { return data_; }

  // Bitmask of supported ui::DragDropTypes::DragOperation by the source.
  int GetSourceOperations() const { return source_operations_; }

 private:
  const OSExchangeData& data_;
  int source_operations_;

  DISALLOW_COPY_AND_ASSIGN(DropTargetEvent);
};

}  // namespace views

#endif  // VIEWS_EVENT_H_
