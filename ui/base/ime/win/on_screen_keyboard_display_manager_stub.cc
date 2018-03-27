// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/on_screen_keyboard_display_manager_stub.h"

namespace ui {

// OnScreenKeyboardDisplayManagerStub member definitions.
OnScreenKeyboardDisplayManagerStub::OnScreenKeyboardDisplayManagerStub() {}

OnScreenKeyboardDisplayManagerStub::~OnScreenKeyboardDisplayManagerStub() {}

bool OnScreenKeyboardDisplayManagerStub::DisplayVirtualKeyboard(
    OnScreenKeyboardObserver* observer) {
  return false;
}

bool OnScreenKeyboardDisplayManagerStub::DismissVirtualKeyboard() {
  return false;
}

void OnScreenKeyboardDisplayManagerStub::RemoveObserver(
    OnScreenKeyboardObserver* observer) {}

bool OnScreenKeyboardDisplayManagerStub::IsKeyboardVisible() const {
  return false;
}

}  // namespace ui
