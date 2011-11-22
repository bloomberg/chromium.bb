// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/views/widget/root_view.h"

namespace views {

namespace {

// The following two functions are copied from event_gtk.cc. These will be
// removed when GTK dependency is removed.
#if defined(TOOLKIT_USES_GTK)
uint16 GetCharacterFromGdkKeyval(guint keyval) {
  guint32 ch = gdk_keyval_to_unicode(keyval);

  // We only support BMP characters.
  return ch < 0xFFFE ? static_cast<uint16>(ch) : 0;
}

GdkEventKey* GetGdkEventKeyFromNative(GdkEvent* gdk_event) {
  DCHECK(gdk_event->type == GDK_KEY_PRESS ||
         gdk_event->type == GDK_KEY_RELEASE);
  return &gdk_event->key;
}
#endif

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

uint16 KeyEvent::GetCharacter() const {
  if (character_)
    return character_;

  if (!native_event()) {
#if defined(TOOLKIT_USES_GTK)
    // This event may have been created from a Gdk event.
    if (!IsControlDown() && gdk_event()) {
      uint16 ch = GetCharacterFromGdkKeyval(
          GetGdkEventKeyFromNative(gdk_event())->keyval);
      if (ch)
        return ch;
    }
#endif
    return ui::GetCharacterFromKeyCode(key_code_, flags());
  }

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  uint16 ch = ui::DefaultSymbolFromXEvent(native_event());
  return ch ? ch : ui::GetCharacterFromKeyCode(key_code_, flags());
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  if (unmodified_character_)
    return unmodified_character_;

  if (!native_event()) {
#if defined(TOOLKIT_USES_GTK)
    // This event may have been created from a Gdk event.
    if (gdk_event()) {
      GdkEventKey* key = GetGdkEventKeyFromNative(gdk_event());

      static const guint kIgnoredModifiers =
        GDK_CONTROL_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK |
        GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK | GDK_SUPER_MASK |
        GDK_HYPER_MASK | GDK_META_MASK;

      // We can't use things like (key->state & GDK_SHIFT_MASK), as it may mask
      // out bits used by X11 or Gtk internally.
      GdkModifierType modifiers =
        static_cast<GdkModifierType>(key->state & ~kIgnoredModifiers);
      guint keyval = 0;
      if (gdk_keymap_translate_keyboard_state(NULL, key->hardware_keycode,
            modifiers, key->group, &keyval, NULL, NULL, NULL)) {
        uint16 ch = GetCharacterFromGdkKeyval(keyval);
        if (ch)
          return ch;
      }
    }
#endif
    return ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
  }

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  XKeyEvent *key = &native_event()->xkey;

  static const unsigned int kIgnoredModifiers = ControlMask | LockMask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;

  // We can't use things like (key.state & ShiftMask), as it may mask out bits
  // used by X11 internally.
  key->state &= ~kIgnoredModifiers;
  uint16 ch = ui::DefaultSymbolFromXEvent(native_event());
  return ch ? ch :
      ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      touch_id_(ui::GetTouchId(native_event)),
      radius_x_(ui::GetTouchRadiusX(native_event)),
      radius_y_(ui::GetTouchRadiusY(native_event)),
      rotation_angle_(ui::GetTouchAngle(native_event)),
      force_(ui::GetTouchForce(native_event)) {
#if defined(USE_XI2_MT)
  if (type() == ui::ET_TOUCH_RELEASED) {
    // NOTE: The slot is allocated by TouchFactory for each XI_TouchBegin
    //       event, which carries a new tracking ID to identify a new touch
    //       sequence.
    ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
    float tracking_id;
    if (factory->ExtractTouchParam(*native_event,
                                   ui::TouchFactory::TP_TRACKING_ID,
                                   &tracking_id)) {
      factory->ReleaseSlotForTrackingID(tracking_id);
    }
  }
#else
  if (type() == ui::ET_TOUCH_PRESSED || type() == ui::ET_TOUCH_RELEASED) {
    ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
    float slot;
    if (factory->ExtractTouchParam(*native_event,
                                   ui::TouchFactory::TP_SLOT_ID, &slot)) {
      factory->SetSlotUsed(slot, type() == ui::ET_TOUCH_PRESSED);
    }
  }
#endif
}

}  // namespace views
