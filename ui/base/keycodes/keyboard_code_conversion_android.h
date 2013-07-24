// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_ANDROID_H_
#define UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_ANDROID_H_

#include "ui/base/keycodes/keyboard_codes_posix.h"
#include "ui/base/ui_export.h"

namespace ui {

UI_EXPORT KeyboardCode KeyboardCodeFromAndroidKeyCode(int keycode);

}  // namespace ui

#endif  // UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_ANDROID_H_
