// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_WAYLAND_H_
#define UI_VIEWS_IME_INPUT_METHOD_WAYLAND_H_
#pragma once

#include <string>

#include "ui/views/ime/input_method_base.h"

namespace views {

class InputMethodWayland : public InputMethodBase {
 public:
  explicit InputMethodWayland(internal::InputMethodDelegate *delegate);

  virtual void DispatchKeyEvent(const KeyEvent& key) OVERRIDE;
  virtual void OnTextInputTypeChanged(View* view) OVERRIDE;
  virtual void OnCaretBoundsChanged(View* view) OVERRIDE;
  virtual void CancelComposition(View* view) OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;

 private:

  void ProcessKeyPressEvent(const KeyEvent& key);

  DISALLOW_COPY_AND_ASSIGN(InputMethodWayland);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_WAYLAND_H_
