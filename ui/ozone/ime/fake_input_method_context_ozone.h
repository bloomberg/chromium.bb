// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_IME_FAKE_INPUT_METHOD_CONTEXT_OZONE_H_
#define UI_OZONE_IME_FAKE_INPUT_METHOD_CONTEXT_OZONE_H_

#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A fake implementation of LinuxInputMethodContext, which does nothing.
class FakeInputMethodContextOzone :
    public LinuxInputMethodContext {
 public:
  FakeInputMethodContextOzone();
  virtual ~FakeInputMethodContextOzone();

  // Overriden from ui::LinuxInputMethodContext
  virtual bool DispatchKeyEvent(const ui::KeyEvent& key_event) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void OnTextInputTypeChanged(ui::TextInputType text_input_type)
      OVERRIDE;
  virtual void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeInputMethodContextOzone);
};

}  // namespace ui

#endif  // UI_OZONE_IME_FAKE_INPUT_METHOD_CONTEXT_OZONE_H_
