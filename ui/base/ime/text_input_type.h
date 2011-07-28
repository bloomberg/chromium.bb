// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_TYPE_H_
#define UI_BASE_IME_TEXT_INPUT_TYPE_H_
#pragma once

namespace ui {

// Intentionally keep sync with WebKit::WebTextInputType defined in:
// third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h
enum TextInputType {
  // Input caret is not in an editable node, no input method shall be used.
  TEXT_INPUT_TYPE_NONE,

  // Input caret is in a normal editable node, any input method can be used.
  TEXT_INPUT_TYPE_TEXT,

  // Input caret is in a password box, an input method may be used only if
  // it's suitable for password input.
  TEXT_INPUT_TYPE_PASSWORD,

  TEXT_INPUT_TYPE_SEARCH,
  TEXT_INPUT_TYPE_EMAIL,
  TEXT_INPUT_TYPE_NUMBER,
  TEXT_INPUT_TYPE_TELEPHONE,
  TEXT_INPUT_TYPE_URL,
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_TYPE_H_
