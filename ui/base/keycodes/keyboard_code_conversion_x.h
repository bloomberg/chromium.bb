// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_
#define UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_

#include "ui/base/keycodes/keyboard_codes_posix.h"

typedef union _XEvent XEvent;

namespace ui {

KeyboardCode KeyboardCodeFromXKeyEvent(XEvent* xev);

}  // namespace ui

#endif  // UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_
