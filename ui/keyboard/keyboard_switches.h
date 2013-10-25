// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_SWITCHES_H_
#define UI_KEYBOARD_KEYBOARD_SWITCHES_H_

#include "ui/keyboard/keyboard_export.h"

namespace keyboard {
namespace switches {

// Enables the swipe selection feature on the virtual keyboard.
KEYBOARD_EXPORT extern const char kEnableSwipeSelection[];

// Enables the virtual keyboard.
KEYBOARD_EXPORT extern const char kEnableVirtualKeyboard[];

// Enables the keyboard usability test.
KEYBOARD_EXPORT extern const char kKeyboardUsabilityTest[];

}  // namespace switches
}  // namespace keyboard

#endif  //  UI_KEYBOARD_KEYBOARD_SWITCHES_H_
