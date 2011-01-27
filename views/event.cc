// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/event.h"

#include "views/view.h"

namespace views {

Event::Event(EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* from, View* to)
    : Event(model),
      location_(model.location_) {
  if (to)
    View::ConvertPointToView(from, to, &location_);
}

KeyEvent::KeyEvent(EventType type, ui::KeyboardCode key_code,
                   int event_flags, int repeat_count, int message_flags)
    : Event(type, event_flags),
      key_code_(key_code),
      repeat_count_(repeat_count),
      message_flags_(message_flags) {
}

MouseEvent::MouseEvent(EventType type,
                       View* from,
                       View* to,
                       const gfx::Point &l,
                       int flags)
    : LocatedEvent(LocatedEvent(type, gfx::Point(l.x(), l.y()), flags),
                                from,
                                to) {
};

MouseEvent::MouseEvent(const MouseEvent& model, View* from, View* to)
    : LocatedEvent(model, from, to) {
}

#if defined(TOUCH_UI)
TouchEvent::TouchEvent(EventType type, int x, int y, int flags, int touch_id)
      : LocatedEvent(type, gfx::Point(x, y), flags),
        touch_id_(touch_id) {
}


TouchEvent::TouchEvent(EventType type,
                       View* from,
                       View* to,
                       const gfx::Point& l,
                       int flags,
                       int touch_id)
    : LocatedEvent(LocatedEvent(type, gfx::Point(l.x(), l.y()), flags),
                                from,
                                to),
      touch_id_(touch_id) {
}

TouchEvent::TouchEvent(const TouchEvent& model, View* from, View* to)
    : LocatedEvent(model, from, to),
      touch_id_(model.touch_id_) {
}
#endif

}  // namespace views
