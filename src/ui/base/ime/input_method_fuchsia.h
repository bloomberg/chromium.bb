// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_FUCHSIA_H_
#define UI_BASE_IME_INPUT_METHOD_FUCHSIA_H_

#include <fuchsia/ui/input/cpp/fidl.h>
#include <memory>

#include "base/macros.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/events/fuchsia/input_event_dispatcher_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class InputMethodKeyboardControllerFuchsia;

// Hnadles input from physical keyboards and the IME service.
class UI_BASE_IME_EXPORT InputMethodFuchsia
    : public InputMethodBase,
      public InputEventDispatcherDelegate {
 public:
  explicit InputMethodFuchsia(internal::InputMethodDelegate* delegate);
  ~InputMethodFuchsia() override;

  // InputMethodBase interface implementation.
  InputMethodKeyboardController* GetInputMethodKeyboardController() override;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;

 private:
  // InputEventDispatcherDelegate interface implementation.
  void DispatchEvent(ui::Event* event) override;

  std::unique_ptr<InputMethodKeyboardControllerFuchsia>
      virtual_keyboard_controller_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodFuchsia);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_FUCHSIA_H_
