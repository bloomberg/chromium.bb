// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/gamepad/Gamepad.h"

namespace blink {

GamepadButton* GamepadButton::Create() {
  return new GamepadButton();
}

GamepadButton::GamepadButton() : value_(0.), pressed_(false), touched_(false) {}

}  // namespace blink
