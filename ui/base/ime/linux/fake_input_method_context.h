// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_

#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A fake implementation of LinuxInputMethodContext, which does nothing.
class FakeInputMethodContext : public LinuxInputMethodContext {
 public:
  FakeInputMethodContext();

  // Overriden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  void Reset() override;
  void OnTextInputTypeChanged(ui::TextInputType text_input_type) override;
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeInputMethodContext);
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_FAKE_INPUT_METHOD_CONTEXT_H_
