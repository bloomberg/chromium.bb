// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_WIN_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_WIN_H_

#include "ui/events/events_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

// Methods to convert ui::KeyboardCode/Windows virtual key type methods.
EVENTS_EXPORT WORD WindowsKeyCodeForKeyboardCode(KeyboardCode keycode);
EVENTS_EXPORT KeyboardCode KeyboardCodeForWindowsKeyCode(WORD keycode);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_WIN_H_
