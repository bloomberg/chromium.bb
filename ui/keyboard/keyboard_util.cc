// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include "base/command_line.h"
#include "ui/keyboard/keyboard_switches.h"

namespace keyboard {

bool IsKeyboardEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVirtualKeyboard);
}

}  // namespace keyboard
