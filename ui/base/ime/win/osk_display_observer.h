// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_OSK_DISPLAY_OBSERVER_H_
#define UI_BASE_IME_WIN_OSK_DISPLAY_OBSERVER_H_

namespace gfx {
class Rect;
}

namespace ui {

// Implemented by classes who wish to get notified about the on screen keyboard
// becoming visible/hidden.
class UI_BASE_IME_EXPORT OnScreenKeyboardObserver {
 public:
  virtual ~OnScreenKeyboardObserver() {}

  // The |keyboard_rect| parameter contains the bounds of the keyboard in dips.
  virtual void OnKeyboardVisible(const gfx::Rect& keyboard_rect) {}
  virtual void OnKeyboardHidden() {}
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_OSK_DISPLAY_OBSERVER_H_
