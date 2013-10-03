// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_OBSERVER_H_
#define UI_BASE_IME_INPUT_METHOD_OBSERVER_H_

#include "ui/base/ui_export.h"

namespace ui {

class InputMethod;
class TextInputClient;

class UI_EXPORT InputMethodObserver {
 public:
  virtual ~InputMethodObserver() {}

  // Called whenever the text input type is changed for the focused client.
  // Currently only used by the mock input method for testing.
  virtual void OnTextInputTypeChanged(const TextInputClient* client) = 0;

  // Called when the top-level system window gets keyboard focus. Currently
  // only used by the mock input method for testing.
  virtual void OnFocus() = 0;

  // Called when the top-level system window loses keyboard focus. Currently
  // only used by the mock input method for testing.
  virtual void OnBlur() = 0;

  // Called when the focused window receives native IME messages that are not
  // translated into other predefined event callbacks. Currently this method is
  // used only for IME testing on Windows.
  virtual void OnUntranslatedIMEMessage(const base::NativeEvent& event) = 0;

  // Called whenever the caret bounds is changed for the input client.
  // Currently only used by the mock input method for testing.
  virtual void OnCaretBoundsChanged(const TextInputClient* client) = 0;

  // Called whenever the input locale is changed for the focused client.
  // This method is currently used only on Windows and only for testing.
  virtual void OnInputLocaleChanged() = 0;

  // Called when either:
  //  - the TextInputClient is changed (e.g. by a change of focus)
  //  - the TextInputType of the TextInputClient changes
  virtual void OnTextInputStateChanged(const TextInputClient* client) = 0;

  // Called when the observed InputMethod is being destroyed.
  virtual void OnInputMethodDestroyed(const InputMethod* input_method) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_OBSERVER_H_
