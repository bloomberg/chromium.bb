// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include "views/view.h"
#include "views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Event, protected:

Event::Event(ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  Init();
}

Event::Event(NativeEvent native_event, ui::EventType type, int flags)
    : type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  InitWithNativeEvent(native_event);
}

Event::Event(NativeEvent2 native_event_2, ui::EventType type, int flags,
             FromNativeEvent2 from_native)
    : native_event_2_(native_event_2),
      type_(type),
      time_stamp_(base::Time::NowFromSystemTime()),
      flags_(flags) {
  InitWithNativeEvent2(native_event_2, from_native);
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(ui::EventType type, const gfx::Point& location,
                           int flags)
    : Event(type, flags),
      location_(location) {
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* source,
                           View* target)
    : Event(model),
      location_(model.location_) {
  if (target)
    View::ConvertPointToView(source, target, &location_);
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, private:

LocatedEvent::LocatedEvent(const LocatedEvent& model, RootView* root)
    : Event(model),
      location_(model.location_) {
  View::ConvertPointFromWidget(root, &location_);
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(ui::EventType type, ui::KeyboardCode key_code,
                   int event_flags)
    : Event(type, event_flags),
      key_code_(key_code) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(ui::EventType type,
                       View* from,
                       View* to,
                       const gfx::Point &l,
                       int flags)
    : LocatedEvent(MouseEvent(type, l.x(), l.y(), flags), from, to) {
}

MouseEvent::MouseEvent(const MouseEvent& model, View* from, View* to)
    : LocatedEvent(model, from, to) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

#if defined(TOUCH_UI)
TouchEvent::TouchEvent(ui::EventType type, int x, int y, int flags,
                       int touch_id)
      : LocatedEvent(type, gfx::Point(x, y), flags),
        touch_id_(touch_id) {
}


TouchEvent::TouchEvent(ui::EventType type,
                       View* from,
                       View* to,
                       const gfx::Point& l,
                       int flags,
                       int touch_id)
    : LocatedEvent(TouchEvent(type, l.x(), l.y(), flags, touch_id), from, to),
      touch_id_(touch_id) {
}

TouchEvent::TouchEvent(const TouchEvent& model, View* from, View* to)
    : LocatedEvent(model, from, to),
      touch_id_(model.touch_id_) {
}
#endif

}  // namespace views
