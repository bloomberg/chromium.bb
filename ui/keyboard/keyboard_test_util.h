// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_TEST_UTIL_
#define UI_KEYBOARD_KEYBOARD_TEST_UTIL_

namespace keyboard {

// Waits until the keyboard is shown. Return false if there is no keyboard
// window created.
bool WaitUntilShown();

// Waits until the keyboard is hidden. Return false if there is no keyboard
// window created.
bool WaitUntilHidden();

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_TEST_UTIL_
