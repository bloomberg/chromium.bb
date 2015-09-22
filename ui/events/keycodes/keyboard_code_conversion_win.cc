// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

const WORD kCtrlPauseScanCode = 0xe046;
const WORD kPauseScanCode = 0x0045;

}  // namespace

WORD WindowsKeyCodeForKeyboardCode(KeyboardCode keycode) {
  return static_cast<WORD>(keycode);
}

KeyboardCode KeyboardCodeForWindowsKeyCode(WORD keycode) {
  return static_cast<KeyboardCode>(keycode);
}

DomCode CodeForWindowsScanCode(WORD scan_code) {
  // Ctrl-Pause generates a special scancode; make sure
  // we undo this special mapping.
  if (scan_code == kCtrlPauseScanCode)
    scan_code = kPauseScanCode;

  return ui::KeycodeConverter::NativeKeycodeToDomCode(scan_code);
}

}  // namespace ui
