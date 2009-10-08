// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/event.h"

#include "views/view.h"

namespace views {

Event::Event(EventType type, int flags)
    : type_(type),
#if defined(OS_WIN)
      time_stamp_(GetTickCount()),
#else
      time_stamp_(0),
#endif
      flags_(flags) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* from, View* to)
    : Event(model),
      location_(model.location_) {
  if (to)
    View::ConvertPointToView(from, to, &location_);
}

KeyEvent::KeyEvent(EventType type, base::KeyboardCode key_code,
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

}  // namespace views
