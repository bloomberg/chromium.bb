// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/input_method_keyboard_controller_fuchsia.h"

#include <utility>

#include "base/fuchsia/component_context.h"
#include "base/logging.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

InputMethodKeyboardControllerFuchsia::InputMethodKeyboardControllerFuchsia(
    InputMethodFuchsia* input_method)
    : event_converter_(input_method),
      ime_client_binding_(this),
      ime_service_(base::fuchsia::ComponentContext::GetDefault()
                       ->ConnectToService<fuchsia::ui::input::ImeService>()),
      ime_visibility_(
          base::fuchsia::ComponentContext::GetDefault()
              ->ConnectToService<fuchsia::ui::input::ImeVisibilityService>()),
      input_method_(input_method) {
  DCHECK(ime_service_);
  DCHECK(input_method_);

  ime_service_.set_error_handler([this]() {
    LOG(ERROR) << "Lost connection to IME service.";
    ime_.Unbind();
    ime_client_binding_.Unbind();
  });

  ime_visibility_.events().OnKeyboardVisibilityChanged = [this](bool visible) {
    keyboard_visible_ = visible;
  };
}

InputMethodKeyboardControllerFuchsia::~InputMethodKeyboardControllerFuchsia() =
    default;

bool InputMethodKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  if (!ime_) {
    // TODO(crbug.com/876934): Instantiate the IME with details about the
    // current composition.
    fuchsia::ui::input::TextInputState state = {};
    state.text = "";
    ime_service_->GetInputMethodEditor(
        fuchsia::ui::input::KeyboardType::TEXT,
        fuchsia::ui::input::InputMethodAction::UNSPECIFIED, std::move(state),
        ime_client_binding_.NewBinding(), ime_.NewRequest());
  }

  if (!keyboard_visible_) {
    ime_service_->ShowKeyboard();
  }

  return true;
}

void InputMethodKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  if (keyboard_visible_) {
    ime_service_->HideKeyboard();
  }
}

void InputMethodKeyboardControllerFuchsia::AddObserver(
    InputMethodKeyboardControllerObserver* observer) {}

void InputMethodKeyboardControllerFuchsia::RemoveObserver(
    InputMethodKeyboardControllerObserver* observer) {}

bool InputMethodKeyboardControllerFuchsia::IsKeyboardVisible() {
  return keyboard_visible_;
}

void InputMethodKeyboardControllerFuchsia::DidUpdateState(
    fuchsia::ui::input::TextInputState state,
    std::unique_ptr<fuchsia::ui::input::InputEvent> input_event) {
  if (input_event->is_keyboard())
    event_converter_.ProcessEvent(*input_event);
}

void InputMethodKeyboardControllerFuchsia::OnAction(
    fuchsia::ui::input::InputMethodAction action) {
  // Synthesize an ENTER keypress and send it to the Window.
  KeyEvent key_event(ET_KEY_PRESSED, KeyboardCode::VKEY_RETURN,
                     ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                     ui::EventTimeForNow());
  input_method_->DispatchKeyEvent(&key_event);
}

}  // namespace ui
