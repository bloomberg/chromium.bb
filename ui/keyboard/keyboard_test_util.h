// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_TEST_UTIL_
#define UI_KEYBOARD_KEYBOARD_TEST_UTIL_

#include "ui/keyboard/keyboard_controller.h"

namespace gfx {
class Rect;
}

namespace keyboard {

// Waits until the keyboard is shown. Return false if there is no keyboard
// window created.
bool WaitUntilShown();

// Waits until the keyboard is hidden. Return false if there is no keyboard
// window created.
bool WaitUntilHidden();

// Waits until the keyboard state is changed to the given state.
void WaitControllerStateChangesTo(const KeyboardControllerState state);

// Gets the calculated keyboard bounds from |root_bounds|. The keyboard height
// is specified by |keyboard_height|. This should be only called when keyboard
// is in FULL_WDITH mode.
gfx::Rect FullWidthKeyboardBoundsFromRootBounds(const gfx::Rect& root_bounds,
                                                int keyboard_height);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_TEST_UTIL_
