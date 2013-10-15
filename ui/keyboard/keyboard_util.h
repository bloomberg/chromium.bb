// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_UTIL_H_
#define UI_KEYBOARD_KEYBOARD_UTIL_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/keyboard/keyboard_export.h"

namespace aura {
class RootWindow;
}
struct GritResourceMap;

namespace keyboard {

// Enumeration of swipe directions.
enum CursorMoveDirection {
  kCursorMoveRight = 0x01,
  kCursorMoveLeft = 0x02,
  kCursorMoveUp = 0x04,
  kCursorMoveDown = 0x08
};

// An enumeration of different keyboard control events that should be logged.
enum KeyboardControlEvent {
  KEYBOARD_CONTROL_SHOW = 0,
  KEYBOARD_CONTROL_HIDE_AUTO,
  KEYBOARD_CONTROL_HIDE_USER,
  KEYBOARD_CONTROL_MAX,
};

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

// Insert |text| into the active TextInputClient associated with |root_window|,
// if there is one. Returns true if |text| was successfully inserted. Note
// that this may convert |text| into ui::KeyEvents for injection in some
// special circumstances (i.e. VKEY_RETURN, VKEY_BACK).
KEYBOARD_EXPORT bool InsertText(const base::string16& text,
                                aura::RootWindow* root_window);

// Move cursor when swipe on the virtualkeyboard. Returns true if cursor was
// successfully moved according to |swipe_direction|.
KEYBOARD_EXPORT bool MoveCursor(int swipe_direction,
                                int modifier_flags,
                                aura::RootWindow* root_window);

// Sends a fabricated key event, where |type| is the event type, |key_value|
// is the unicode value of the character, |key_code| is the legacy key code
// value, and |shift_modifier| indicates if the shift key is being virtually
// pressed. The event is dispatched to the active TextInputClient associated
// with |root_window|. The type may be "keydown" or "keyup".
KEYBOARD_EXPORT bool SendKeyEvent(std::string type,
                                   int key_value,
                                   int key_code,
                                   bool shift_modifier,
                                   aura::RootWindow* root_window);

// Get the list of keyboard resources. |size| is populated with the number of
// resources in the returned array.
KEYBOARD_EXPORT const GritResourceMap* GetKeyboardExtensionResources(
    size_t* size);

// Logs the keyboard control event as a UMA stat.
void LogKeyboardControlEvent(KeyboardControlEvent event);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UTIL_H_
