// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include <gdk/gdk.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/gfx/point.h"

namespace {

unsigned int GetGdkStateFromNative(GdkEvent* gdk_event) {
  switch (gdk_event->type) {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return gdk_event->key.state;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return gdk_event->button.state;
    case GDK_SCROLL:
      return gdk_event->scroll.state;
    case GDK_MOTION_NOTIFY:
      return gdk_event->motion.state;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      return gdk_event->crossing.state;
    default:
      NOTREACHED();
      break;
  }
  return 0;
}

int GetFlagsFromGdkState(unsigned int state) {
  int flags = 0;
  flags |= (state & GDK_LOCK_MASK) ? ui::EF_CAPS_LOCK_DOWN : 0;
  flags |= (state & GDK_CONTROL_MASK) ? ui::EF_CONTROL_DOWN : 0;
  flags |= (state & GDK_SHIFT_MASK) ? ui::EF_SHIFT_DOWN : 0;
  flags |= (state & GDK_MOD1_MASK) ? ui::EF_ALT_DOWN : 0;
  flags |= (state & GDK_BUTTON1_MASK) ? ui::EF_LEFT_BUTTON_DOWN : 0;
  flags |= (state & GDK_BUTTON2_MASK) ? ui::EF_MIDDLE_BUTTON_DOWN : 0;
  flags |= (state & GDK_BUTTON3_MASK) ? ui::EF_RIGHT_BUTTON_DOWN : 0;
  return flags;
}

////////////////////////////////////////////////////////////////////////////////
// These functions mirror ui/base/events.h, but GTK is in the midst of removal.

ui::EventType EventTypeFromNative(GdkEvent* gdk_event) {
  // Add new event types as necessary.
  switch (gdk_event->type) {
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_PRESS:
      return ui::ET_MOUSE_PRESSED;
    case GDK_BUTTON_RELEASE:
      return ui::ET_MOUSE_RELEASED;
    case GDK_DRAG_MOTION:
      return ui::ET_MOUSE_DRAGGED;
    case GDK_ENTER_NOTIFY:
      return ui::ET_MOUSE_ENTERED;
    case GDK_KEY_PRESS:
      return ui::ET_KEY_PRESSED;
    case GDK_KEY_RELEASE:
      return ui::ET_KEY_RELEASED;
    case GDK_LEAVE_NOTIFY:
      return ui::ET_MOUSE_EXITED;
    case GDK_MOTION_NOTIFY:
      if (gdk_event->motion.state & GDK_BUTTON1_MASK ||
          gdk_event->motion.state & GDK_BUTTON2_MASK ||
          gdk_event->motion.state & GDK_BUTTON3_MASK ||
          gdk_event->motion.state & GDK_BUTTON4_MASK ||
          gdk_event->motion.state & GDK_BUTTON5_MASK) {
        return ui::ET_MOUSE_DRAGGED;
      }
      return ui::ET_MOUSE_MOVED;
    case GDK_SCROLL:
      return ui::ET_MOUSEWHEEL;
    default:
      NOTREACHED();
      break;
  }
  return ui::ET_UNKNOWN;
}

int EventFlagsFromNative(GdkEvent* gdk_event) {
  int flags = GetFlagsFromGdkState(GetGdkStateFromNative(gdk_event));
  if (gdk_event->type == GDK_2BUTTON_PRESS)
    flags |= ui::EF_IS_DOUBLE_CLICK;
  if (gdk_event->type == GDK_BUTTON_PRESS ||
      gdk_event->type == GDK_2BUTTON_PRESS ||
      gdk_event->type == GDK_3BUTTON_PRESS ||
      gdk_event->type == GDK_BUTTON_RELEASE) {
    switch (gdk_event->button.button) {
      case 1:
        return flags | ui::EF_LEFT_BUTTON_DOWN;
      case 2:
        return flags | ui::EF_MIDDLE_BUTTON_DOWN;
      case 3:
        return flags | ui::EF_RIGHT_BUTTON_DOWN;
    }
  }
  return flags;
}

gfx::Point EventLocationFromNative(GdkEvent* gdk_event) {
  double x = 0, y = 0;
  if (gdk_event_get_coords(gdk_event, &x, &y))
    return gfx::Point(static_cast<int>(x), static_cast<int>(y));
  return gfx::Point();
}

ui::KeyboardCode KeyboardCodeFromNative(GdkEvent* gdk_event) {
  DCHECK(gdk_event->type == GDK_KEY_PRESS ||
         gdk_event->type == GDK_KEY_RELEASE);
  return ui::KeyboardCodeFromGdkEventKey(&gdk_event->key);
}

int GetMouseWheelOffset(GdkEvent* gdk_event) {
  DCHECK(gdk_event->type == GDK_SCROLL);
  int offset = (gdk_event->scroll.direction == GDK_SCROLL_UP ||
                gdk_event->scroll.direction == GDK_SCROLL_LEFT) ? 1 : -1;
  return offset;
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Event, protected:

Event::Event(GdkEvent* gdk_event, ui::EventType type, int flags)
    : native_event_(NULL),
      gdk_event_(gdk_event),
      type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(GdkEvent* gdk_event)
    : Event(gdk_event,
            EventTypeFromNative(gdk_event),
            EventFlagsFromNative(gdk_event)),
      location_(EventLocationFromNative(gdk_event)) {
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(GdkEvent* gdk_event)
    : Event(gdk_event,
            EventTypeFromNative(gdk_event),
            EventFlagsFromNative(gdk_event)),
      key_code_(KeyboardCodeFromNative(gdk_event)),
      character_(0),
      unmodified_character_(0) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(GdkEvent* gdk_event)
    : LocatedEvent(gdk_event) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(GdkEvent* gdk_event)
    : MouseEvent(gdk_event),
      offset_(kWheelDelta * GetMouseWheelOffset(gdk_event)) {
}

}  // namespace views
