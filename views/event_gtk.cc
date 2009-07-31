// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/event.h"

#include <gdk/gdk.h>

namespace views {

// TODO(jcampan): the same physical key can send different keyvals (ex: a or A).
// In order for accelerators to work, we need to normalize that.  The right
// solution should probably to get the key-code out of the keystate.
KeyEvent::KeyEvent(GdkEventKey* event, bool make_upper_case)
    : Event(event->type == GDK_KEY_PRESS ?
            Event::ET_KEY_PRESSED : Event::ET_KEY_RELEASED,
            GetFlagsFromGdkState(event->state)),
      // TODO(erg): All these values are iffy.
      character_(make_upper_case ? gdk_keyval_to_upper (event->keyval) :
                                   event->keyval),
      repeat_count_(0),
      message_flags_(0) {
}

int Event::GetFlagsFromGdkState(int state) {
  int flags = 0;
  if (state & GDK_CONTROL_MASK)
    flags |= Event::EF_CONTROL_DOWN;
  if (state & GDK_SHIFT_MASK)
    flags |= Event::EF_SHIFT_DOWN;
  if (state & GDK_MOD1_MASK)
    flags |= Event::EF_ALT_DOWN;
  if (state & GDK_BUTTON1_MASK)
    flags |= Event::EF_LEFT_BUTTON_DOWN;
  if (state & GDK_BUTTON2_MASK)
    flags |= Event::EF_MIDDLE_BUTTON_DOWN;
  if (state & GDK_BUTTON3_MASK)
    flags |= Event::EF_RIGHT_BUTTON_DOWN;
  return flags;
}

}  // namespace views
