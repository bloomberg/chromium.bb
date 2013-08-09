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

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

// Insert |text| into the active TextInputClient associated with |root_window|,
// if there is one.  Returns true if |text| was successfully inserted.  Note
// that this may convert |text| into ui::KeyEvents for injection in some
// special circumstances (i.e. VKEY_RETURN, VKEY_BACK).
KEYBOARD_EXPORT bool InsertText(const base::string16& text,
                                aura::RootWindow* root_window);

// Move cursor when swipe on the virtualkeyboard. Returns true if cursor was
// successfully moved according to |swipe_direction|.
KEYBOARD_EXPORT bool MoveCursor(int swipe_direction,
                                int modifier_flags,
                                aura::RootWindow* root_window);

// Get the list of keyboard resources.  |size| is populated with the number of
// resources in the returned array.
KEYBOARD_EXPORT const GritResourceMap* GetKeyboardExtensionResources(
    size_t* size);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UTIL_H_
