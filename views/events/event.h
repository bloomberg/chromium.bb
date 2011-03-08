// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EVENTS_EVENT_H_
#define VIEWS_EVENTS_EVENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"
#include "views/native_types.h"

#if defined(TOUCH_UI)
typedef union _XEvent XEvent;
#endif

namespace ui {
class OSExchangeData;
}
using ui::OSExchangeData;

namespace views {

class RootView;
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
  // This type exists to distinguish between the NativeEvent and NativeEvent2
  // constructors.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  struct FromNativeEvent2 {};

  const NativeEvent& native_event() const { return native_event_; }
  const NativeEvent2& native_event_2() const { return native_event_2_; }
  ui::EventType type() const { return type_; }
  const base::Time& time_stamp() const { return time_stamp_; }
  int flags() const { return flags_; }
  void set_flags(int flags) { flags_ = flags; }

  // The following methods return true if the respective keys were pressed at
  // the time the event was created.
  bool IsShiftDown() const { return (flags_ & ui::EF_SHIFT_DOWN) != 0; }
  bool IsControlDown() const { return (flags_ & ui::EF_CONTROL_DOWN) != 0; }
  bool IsCapsLockDown() const { return (flags_ & ui::EF_CAPS_LOCK_DOWN) != 0; }
  bool IsAltDown() const { return (flags_ & ui::EF_ALT_DOWN) != 0; }

  bool IsMouseEvent() const {
    return type_ == ui::ET_MOUSE_PRESSED ||
           type_ == ui::ET_MOUSE_DRAGGED ||
           type_ == ui::ET_MOUSE_RELEASED ||
           type_ == ui::ET_MOUSE_MOVED ||
           type_ == ui::ET_MOUSE_ENTERED ||
           type_ == ui::ET_MOUSE_EXITED ||
           type_ == ui::ET_MOUSEWHEEL;
  }

#if defined(TOUCH_UI)
  bool IsTouchEvent() const {
    return type_ == ui::ET_TOUCH_RELEASED ||
           type_ == ui::ET_TOUCH_PRESSED ||
           type_ == ui::ET_TOUCH_MOVED ||
           type_ == ui::ET_TOUCH_STATIONARY ||
           type_ == ui::ET_TOUCH_CANCELLED;
  }
#endif

#if defined(OS_WIN)
  // Returns the EventFlags in terms of windows flags.
  int GetWindowsFlags() const;
#elif defined(OS_LINUX)
  // Convert the state member on a GdkEvent to views::Event flags
  static int GetFlagsFromGdkState(unsigned int state);
#endif

 protected:
  Event(ui::EventType type, int flags);
  Event(NativeEvent native_event, ui::EventType type, int flags);
  // Because the world is complicated, sometimes we have two different kinds of
  // NativeEvent in play in the same executable. See native_types.h for the tale
  // of woe.
  Event(NativeEvent2 native_event, ui::EventType type, int flags,
        FromNativeEvent2);

  Event(const Event& model)
      : native_event_(model.native_event()),
        type_(model.type()),
        time_stamp_(model.time_stamp()),
        flags_(model.flags()) {
  }

 private:
  void operator=(const Event&);

  // Safely initializes the native event members of this class.
  void Init();
  void InitWithNativeEvent(NativeEvent native_event);
  void InitWithNativeEvent2(NativeEvent2 native_event_2, FromNativeEvent2);

  NativeEvent native_event_;
  NativeEvent2 native_event_2_;
  ui::EventType type_;
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
  int x() const { return location_.x(); }
  int y() const { return location_.y(); }
  const gfx::Point& location() const { return location_; }

 protected:
  explicit LocatedEvent(NativeEvent native_event);
  LocatedEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native);

  // TODO(msw): Kill this legacy constructor when we update uses.
  // Simple initialization from cracked metadata.
  LocatedEvent(ui::EventType type, const gfx::Point& location, int flags);

  // Create a new LocatedEvent which is identical to the provided model.
  // If source / target views are provided, the model location will be converted
  // from |source| coordinate system to |target| coordinate system.
  LocatedEvent(const LocatedEvent& model, View* source, View* target);

  // This constructor is to allow converting the location of an event from the
  // widget's coordinate system to the RootView's coordinate system.
  LocatedEvent(const LocatedEvent& model, RootView* root);

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
  explicit MouseEvent(NativeEvent native_event);
  MouseEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native);

  // Create a new MouseEvent which is identical to the provided model.
  // If source / target views are provided, the model location will be converted
  // from |source| coordinate system to |target| coordinate system.
  MouseEvent(const MouseEvent& model, View* source, View* target);

  // TODO(msw): Kill this legacy constructor when we update uses.
  // Create a new mouse event
  MouseEvent(ui::EventType type, int x, int y, int flags)
      : LocatedEvent(type, gfx::Point(x, y), flags) {
  }

  // TODO(msw): Kill this legacy constructor when we update uses.
  // Create a new mouse event from a type and a point. If source / target views
  // are provided, the point will be converted from |source| coordinate system
  // to |target| coordinate system.
  MouseEvent(ui::EventType type,
             View* source,
             View* target,
             const gfx::Point &l,
             int flags);

  // Conveniences to quickly test what button is down
  bool IsOnlyLeftMouseButton() const {
    return (flags() & ui::EF_LEFT_BUTTON_DOWN) &&
      !(flags() & (ui::EF_MIDDLE_BUTTON_DOWN | ui::EF_RIGHT_BUTTON_DOWN));
  }

  bool IsLeftMouseButton() const {
    return (flags() & ui::EF_LEFT_BUTTON_DOWN) != 0;
  }

  bool IsOnlyMiddleMouseButton() const {
    return (flags() & ui::EF_MIDDLE_BUTTON_DOWN) &&
      !(flags() & (ui::EF_LEFT_BUTTON_DOWN | ui::EF_RIGHT_BUTTON_DOWN));
  }

  bool IsMiddleMouseButton() const {
    return (flags() & ui::EF_MIDDLE_BUTTON_DOWN) != 0;
  }

  bool IsOnlyRightMouseButton() const {
    return (flags() & ui::EF_RIGHT_BUTTON_DOWN) &&
      !(flags() & (ui::EF_LEFT_BUTTON_DOWN | ui::EF_MIDDLE_BUTTON_DOWN));
  }

  bool IsRightMouseButton() const {
    return (flags() & ui::EF_RIGHT_BUTTON_DOWN) != 0;
  }

 protected:
  MouseEvent(const MouseEvent& model, RootView* root)
      : LocatedEvent(model, root) {
  }

 private:
  friend class RootView;

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
  TouchEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native);

  // Create a new touch event.
  TouchEvent(ui::EventType type, int x, int y, int flags, int touch_id);

  // Create a new touch event from a type and a point. If from / to views
  // are provided, the point will be converted from 'source' coordinate system
  // to 'target' coordinate system.
  TouchEvent(ui::EventType type,
             View* source,
             View* target,
             const gfx::Point& l,
             int flags,
             int touch_id);

  // Create a new TouchEvent which is identical to the provided model.
  // If source / target views are provided, the model location will be converted
  // from |source| coordinate system to |target| coordinate system.
  TouchEvent(const TouchEvent& model, View* source, View* target);

  bool identity() const { return touch_id_; }

 private:
  friend class RootView;

  TouchEvent(const TouchEvent& model, RootView* root)
      : LocatedEvent(model, root),
        touch_id_(model.touch_id_) {
  }

  // The identity (typically finger) of the touch starting at 0 and incrementing
  // for each separable additional touch that the hardware can detect.
  const int touch_id_;

  DISALLOW_COPY_AND_ASSIGN(TouchEvent);
};
#endif

////////////////////////////////////////////////////////////////////////////////
// KeyEvent class
//
//  KeyEvent encapsulates keyboard input events - key press and release.
//
////////////////////////////////////////////////////////////////////////////////
class KeyEvent : public Event {
 public:
  explicit KeyEvent(NativeEvent native_event);
  KeyEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native);

  // Creates a new KeyEvent synthetically (i.e. not in response to an input
  // event from the host environment). This is typically only used in testing as
  // some metadata obtainable from the underlying native event is not present.
  // TODO(beng): see if we can kill this.
  KeyEvent(ui::EventType type,
           ui::KeyboardCode key_code,
           int event_flags);

  ui::KeyboardCode key_code() const { return key_code_; }

 private:
  ui::KeyboardCode key_code_;

  DISALLOW_COPY_AND_ASSIGN(KeyEvent);
};

////////////////////////////////////////////////////////////////////////////////
//
// MouseWheelEvent class
//
// A MouseWheelEvent is used to propagate mouse wheel user events.
// Note: e.GetOffset() > 0 means scroll up / left.
//
////////////////////////////////////////////////////////////////////////////////
class MouseWheelEvent : public MouseEvent {
 public:
  // See |offset| for details.
  static const int kWheelDelta;

  explicit MouseWheelEvent(NativeEvent native_event);
  MouseWheelEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native);

  // The amount to scroll. This is in multiples of kWheelDelta.
  int offset() const { return offset_; }

 private:
  friend class RootView;

  MouseWheelEvent(const MouseWheelEvent& model, RootView* root)
      : MouseEvent(model, root),
        offset_(model.offset_) {
  }

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
      : LocatedEvent(ui::ET_DROP_TARGET_EVENT, gfx::Point(x, y), 0),
        data_(data),
        source_operations_(source_operations) {
  }

  const OSExchangeData& data() const { return data_; }
  int source_operations() const { return source_operations_; }

 private:
  // Data associated with the drag/drop session.
  const OSExchangeData& data_;

  // Bitmask of supported ui::DragDropTypes::DragOperation by the source.
  int source_operations_;

  DISALLOW_COPY_AND_ASSIGN(DropTargetEvent);
};

}  // namespace views

#endif  // VIEWS_EVENTS_EVENT_H_
