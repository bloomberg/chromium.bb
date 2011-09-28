// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory.h"
#include "views/widget/root_view.h"

namespace views {

namespace {

int GetTouchIDFromXEvent(XEvent* xev) {
  float slot = 0;
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  if (!factory->IsRealTouchDevice(xievent->sourceid)) {
    // TODO(sad): Come up with a way to generate touch-ids for multi-touch
    // events when touch-events are generated from a mouse.
    return slot;
  }

#if defined(USE_XI2_MT)
  float tracking_id;
  if (!factory->ExtractTouchParam(
         *xev, ui::TouchFactory::TP_TRACKING_ID, &tracking_id))
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
  else
    slot = factory->GetSlotForTrackingID(tracking_id);
#else
  if (!factory->ExtractTouchParam(
         *xev, ui::TouchFactory::TP_SLOT_ID, &slot))
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
#endif
  return slot;
}

uint16 GetCharacterFromXKeyEvent(XKeyEvent* key) {
  char buf[6];
  int bytes_written = XLookupString(key, buf, 6, NULL, NULL);
  DCHECK_LE(bytes_written, 6);

  string16 result;
  return (bytes_written > 0 && UTF8ToUTF16(buf, bytes_written, &result) &&
          result.length() == 1) ? result[0] : 0;
}

float GetTouchParamFromXEvent(XEvent* xev,
                              ui::TouchFactory::TouchParam tp,
                              float default_value) {
  ui::TouchFactory::GetInstance()->ExtractTouchParam(*xev, tp, &default_value);
  return default_value;
}

float GetTouchForceFromXEvent(XEvent* xev) {
  float force = 0.0;
  force = GetTouchParamFromXEvent(xev, ui::TouchFactory::TP_PRESSURE, 0.0);
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(xev->xcookie.data)->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!ui::TouchFactory::GetInstance()->NormalizeTouchParam(
      deviceid, ui::TouchFactory::TP_PRESSURE, &force))
    force = 0.0;
  return force;
}

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
    return GetCharacterFromKeyCode(key_code_, flags());
  }

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  uint16 ch = GetCharacterFromXKeyEvent(&native_event()->xkey);
  return ch ? ch : GetCharacterFromKeyCode(key_code_, flags());
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
    return GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
  }

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  XKeyEvent key = native_event()->xkey;

  static const unsigned int kIgnoredModifiers = ControlMask | LockMask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;

  // We can't use things like (key.state & ShiftMask), as it may mask out bits
  // used by X11 internally.
  key.state &= ~kIgnoredModifiers;
  uint16 ch = GetCharacterFromXKeyEvent(&key);
  return ch ? ch :
      GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(const ui::NativeEvent& native_event)
    : LocatedEvent(native_event),
      touch_id_(GetTouchIDFromXEvent(native_event)),
      radius_x_(GetTouchParamFromXEvent(native_event,
                                        ui::TouchFactory::TP_TOUCH_MAJOR,
                                        2.0) / 2.0),
      radius_y_(GetTouchParamFromXEvent(native_event,
                                        ui::TouchFactory::TP_TOUCH_MINOR,
                                        2.0) / 2.0),
      rotation_angle_(GetTouchParamFromXEvent(native_event,
                                              ui::TouchFactory::TP_ORIENTATION,
                                              0.0)),
      force_(GetTouchForceFromXEvent(native_event)) {
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
