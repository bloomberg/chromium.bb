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

namespace keyboard {

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

// Insert |text| into the active TextInputClient associated with |root_window|,
// if there is one.  Returns true if |text| was successfully inserted.  Note
// that this may convert |text| into ui::KeyEvents for injection in some
// special circumstances (i.e. VKEY_RETURN, VKEY_BACK).
KEYBOARD_EXPORT bool InsertText(const base::string16& text,
                                aura::RootWindow* root_window);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UTIL_H_
