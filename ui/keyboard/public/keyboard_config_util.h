// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_PUBLIC_KEYBOARD_CONFIG_UTIL_H_
#define UI_KEYBOARD_PUBLIC_KEYBOARD_CONFIG_UTIL_H_

#include "ui/keyboard/public/keyboard_controller.mojom.h"

namespace keyboard {

inline mojom::KeyboardConfig GetDefaultKeyboardConfig() {
  return mojom::KeyboardConfig(
      true /* auto_complete */, true /* auto_correct */,
      true /* auto_capitalize */, true /* handwriting */,
      true /* spell_check */,
      // It denotes the preferred value, and can be true even if there is no
      // actual audio input device.
      true /* voice_input */,
      mojom::KeyboardOverscrollOverride::kNone /* overscroll_override */);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_PUBLIC_KEYBOARD_CONFIG_UTIL_H_
