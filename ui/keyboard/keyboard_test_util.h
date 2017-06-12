// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_TEST_UTIL_
#define UI_KEYBOARD_KEYBOARD_TEST_UTIL_

#include "ui/keyboard/keyboard_controller.h"

namespace keyboard {

// Waits until the keyboard is shown. Return false if there is no keyboard
// window created.
bool WaitUntilShown();

// Waits until the keyboard is hidden. Return false if there is no keyboard
// window created.
bool WaitUntilHidden();

// Waits until the keyboard state is changed to the given state.
void WaitControllerStateChangesTo(KeyboardControllerState state);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_TEST_UTIL_
