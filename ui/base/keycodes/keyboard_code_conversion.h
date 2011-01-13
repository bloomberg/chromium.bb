// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
#define UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
#pragma once

#include "ui/base/keycodes/keyboard_codes.h"

#include <string>

namespace ui {

// Convert a KeyIdentifer (see Section 6.3.3 here:
// http://www.w3.org/TR/DOM-Level-3-Events/#keyset-keyidentifiers)
// to a ui::KeyboardCode.
KeyboardCode KeyCodeFromKeyIdentifier(const std::string& key_identifier);

}  // namespace ui

#endif  // UI_BASE_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
