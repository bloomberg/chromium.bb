// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include <gdk/gdk.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"

namespace views {

namespace {

ui::EventType EventTypeFromNative(NativeEvent native_event) {
  // Add new event types as necessary.
  switch (native_event->type) {
    case GDK_ENTER_NOTIFY:
      return ui::ET_MOUSE_ENTERED;
    case GDK_KEY_PRESS:
      return ui::ET_KEY_PRESSED;
    case GDK_KEY_RELEASE:
      return ui::ET_KEY_RELEASED;
    case GDK_LEAVE_NOTIFY:
      return ui::ET_MOUSE_EXITED;
    case GDK_SCROLL:
      return ui::ET_MOUSEWHEEL;
    default:
      NOTREACHED();
      break;
  }
  return ui::ET_UNKNOWN;
}

GdkEventKey* GetGdkEventKeyFromNative(NativeEvent native_event) {
  DCHECK(native_event->type == GDK_KEY_PRESS ||
         native_event->type == GDK_KEY_RELEASE);
  return &native_event->key;
}

gfx::Point GetMouseEventLocation(NativeEvent native_event) {
  double x = 0, y = 0;
  if (gdk_event_get_coords(native_event, &x, &y))
    return gfx::Point(static_cast<int>(x), static_cast<int>(y));
  return gfx::Point();
}

int GetMouseWheelOffset(NativeEvent native_event) {
  DCHECK(native_event->type == GDK_SCROLL);
  int offset = (native_event->scroll.direction == GDK_SCROLL_UP ||
                native_event->scroll.direction == GDK_SCROLL_LEFT) ? 1 : -1;
  return MouseWheelEvent::kWheelDelta * offset;
}

unsigned int GetGdkStateFromNative(NativeEvent native_event) {
  switch (native_event->type) {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return native_event->key.state;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return native_event->button.state;
    case GDK_SCROLL:
      return native_event->scroll.state;
    case GDK_MOTION_NOTIFY:
      return native_event->motion.state;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      return native_event->crossing.state;
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

#if !defined(TOUCH_UI)
uint16 GetCharacterFromGdkKeyval(guint keyval) {
  guint32 ch = gdk_keyval_to_unicode(keyval);

  // We only support BMP characters.
  return ch < 0xFFFE ? static_cast<uint16>(ch) : 0;
}
#endif

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Event, public:

// static
int Event::GetFlagsFromGdkEvent(NativeEvent native_event) {
  int flags = GetFlagsFromGdkState(GetGdkStateFromNative(native_event));
  if (native_event->type == GDK_2BUTTON_PRESS)
    flags |= ui::EF_IS_DOUBLE_CLICK;
  if (native_event->type == GDK_BUTTON_PRESS ||
      native_event->type == GDK_2BUTTON_PRESS ||
      native_event->type == GDK_3BUTTON_PRESS ||
      native_event->type == GDK_BUTTON_RELEASE) {
    switch (native_event->button.button) {
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

////////////////////////////////////////////////////////////////////////////////
// Event, private:

void Event::Init() {
  native_event_ = NULL;
  native_event_2_ = NULL;
}

void Event::InitWithNativeEvent(NativeEvent native_event) {
  native_event_ = native_event;
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  native_event_2_ = NULL;
}

#if !defined(TOUCH_UI)
void Event::InitWithNativeEvent2(NativeEvent2 native_event_2,
                                 FromNativeEvent2) {
  // No one should ever call this on non-Touch Linux.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
  native_event_2_ = NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(NativeEvent native_event)
    : Event(native_event, EventTypeFromNative(native_event),
            GetFlagsFromGdkEvent(native_event)),
      location_(GetMouseEventLocation(native_event)) {
}

#if !defined(TOUCH_UI)
LocatedEvent::LocatedEvent(NativeEvent2 native_event_2,
                           FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // No one should ever call this on Gtk-views.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}
#endif

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(NativeEvent native_event)
    : LocatedEvent(native_event) {
}

#if !defined(TOUCH_UI)
MouseEvent::MouseEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native) {
  // No one should ever call this on Gtk-views.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}
#endif

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(NativeEvent native_event)
    : Event(native_event, EventTypeFromNative(native_event),
            GetFlagsFromGdkEvent(native_event)),
      key_code_(ui::KeyboardCodeFromGdkEventKey(
                GetGdkEventKeyFromNative(native_event))) {
}

#if !defined(TOUCH_UI)
KeyEvent::KeyEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // No one should ever call this on Gtk-views.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

uint16 KeyEvent::GetCharacter() const {
  // Gtk doesn't support control characters.
  if (IsControlDown() || !native_event())
    return GetCharacterFromKeyCode(key_code_, flags());

  uint16 ch = GetCharacterFromGdkKeyval(
      GetGdkEventKeyFromNative(native_event())->keyval);
  return ch ? ch : GetCharacterFromKeyCode(key_code_, flags());
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  if (!native_event())
    return GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);

  GdkEventKey* key = GetGdkEventKeyFromNative(native_event());

  static const guint kIgnoredModifiers =
      GDK_CONTROL_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK |
      GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK | GDK_SUPER_MASK |
      GDK_HYPER_MASK | GDK_META_MASK;

  // We can't use things like (key->state & GDK_SHIFT_MASK), as it may mask out
  // bits used by X11 or Gtk internally.
  GdkModifierType modifiers =
      static_cast<GdkModifierType>(key->state & ~kIgnoredModifiers);
  guint keyval = 0;
  uint16 ch = 0;
  if (gdk_keymap_translate_keyboard_state(NULL, key->hardware_keycode,
                                          modifiers, key->group, &keyval,
                                          NULL, NULL, NULL)) {
    ch = GetCharacterFromGdkKeyval(keyval);
  }

  return ch ? ch :
      GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent native_event)
    : MouseEvent(native_event),
      offset_(GetMouseWheelOffset(native_event)) {
}

#if !defined(TOUCH_UI)
MouseWheelEvent::MouseWheelEvent(NativeEvent2 native_event_2,
                                 FromNativeEvent2 from_native)
    : MouseEvent(native_event_2, from_native) {
  // No one should ever call this on Gtk-views.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}
#endif

}  // namespace views
