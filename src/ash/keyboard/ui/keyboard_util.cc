// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_util.h"

#include <string>

#include "ash/keyboard/ui/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"

namespace keyboard {

namespace {

// Until src/chrome is fully transitioned to use ChromeKeyboardControllerClient
// we need to test whether KeyboardController exists; it is null in OopMash.
// TODO(stevenjb): Remove remaining calls from src/chrome.
// https://crbug.com/84332.

bool GetFlag(mojom::KeyboardEnableFlag flag) {
  return KeyboardController::HasInstance()
             ? KeyboardController::Get()->IsEnableFlagSet(flag)
             : false;
}

void SetOrClearEnableFlag(mojom::KeyboardEnableFlag flag, bool enabled) {
  auto* controller = KeyboardController::Get();
  if (!controller)
    return;
  if (enabled)
    controller->SetEnableFlag(flag);
  else
    controller->ClearEnableFlag(flag);
}

}  // namespace

void SetAccessibilityKeyboardEnabled(bool enabled) {
  SetOrClearEnableFlag(mojom::KeyboardEnableFlag::kAccessibilityEnabled,
                       enabled);
}

bool GetAccessibilityKeyboardEnabled() {
  return GetFlag(mojom::KeyboardEnableFlag::kAccessibilityEnabled);
}

void SetKeyboardEnabledFromShelf(bool enabled) {
  SetOrClearEnableFlag(mojom::KeyboardEnableFlag::kShelfEnabled, enabled);
}

bool GetKeyboardEnabledFromShelf() {
  return GetFlag(mojom::KeyboardEnableFlag::kShelfEnabled);
}

void SetTouchKeyboardEnabled(bool enabled) {
  SetOrClearEnableFlag(mojom::KeyboardEnableFlag::kTouchEnabled, enabled);
}

bool GetTouchKeyboardEnabled() {
  return GetFlag(mojom::KeyboardEnableFlag::kTouchEnabled);
}

bool IsKeyboardEnabled() {
  return KeyboardController::Get()->IsEnabled();
}

}  // namespace keyboard
