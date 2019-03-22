// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FUCHSIA_INPUT_METHOD_KEYBOARD_CONTROLLER_FUCHSIA_H_
#define UI_BASE_IME_FUCHSIA_INPUT_METHOD_KEYBOARD_CONTROLLER_FUCHSIA_H_

#include <fuchsia/ui/input/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/macros.h"
#include "ui/base/ime/input_method_fuchsia.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/events/fuchsia/input_event_dispatcher.h"

namespace ui {

// Handles input events from the Fuchsia on-screen keyboard.
class UI_BASE_IME_EXPORT InputMethodKeyboardControllerFuchsia
    : public InputMethodKeyboardController,
      public fuchsia::ui::input::InputMethodEditorClient {
 public:
  // |input_method|: Pointer to the parent InputMethod which owns |this|.
  explicit InputMethodKeyboardControllerFuchsia(
      InputMethodFuchsia* input_method);
  ~InputMethodKeyboardControllerFuchsia() override;

  // InputMethodKeyboardController implementation.
  bool DisplayVirtualKeyboard() override;
  void DismissVirtualKeyboard() override;
  void AddObserver(InputMethodKeyboardControllerObserver* observer) override;
  void RemoveObserver(InputMethodKeyboardControllerObserver* observer) override;
  bool IsKeyboardVisible() override;

 private:
  // InputMethodEditorClient implementation.
  void DidUpdateState(
      fuchsia::ui::input::TextInputState state,
      std::unique_ptr<fuchsia::ui::input::InputEvent> input_event) override;
  void OnAction(fuchsia::ui::input::InputMethodAction action) override;

  bool keyboard_visible_ = false;
  InputEventDispatcher event_converter_;
  fidl::Binding<fuchsia::ui::input::InputMethodEditorClient>
      ime_client_binding_;
  fuchsia::ui::input::ImeServicePtr ime_service_;
  fuchsia::ui::input::InputMethodEditorPtr ime_;
  fuchsia::ui::input::ImeVisibilityServicePtr ime_visibility_;

  InputMethodFuchsia* input_method_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodKeyboardControllerFuchsia);
};

}  // namespace ui

#endif  // UI_BASE_IME_FUCHSIA_INPUT_METHOD_KEYBOARD_CONTROLLER_FUCHSIA_H_
