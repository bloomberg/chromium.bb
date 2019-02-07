// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_fuchsia.h"

#include <fuchsia/ui/input/cpp/fidl.h>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/fuchsia/service_directory_client.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

InputMethodFuchsia::InputMethodFuchsia(internal::InputMethodDelegate* delegate)
    : InputMethodBase(delegate),
      event_converter_(this),
      ime_client_binding_(this),
      ime_service_(base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
                       ->ConnectToService<fuchsia::ui::input::ImeService>()),
      virtual_keyboard_controller_(ime_service_.get()) {}

InputMethodFuchsia::~InputMethodFuchsia() {}

InputMethodKeyboardController*
InputMethodFuchsia::GetInputMethodKeyboardController() {
  return &virtual_keyboard_controller_;
}

void InputMethodFuchsia::DispatchEvent(ui::Event* event) {
  DCHECK(event->IsKeyEvent());
  DispatchKeyEvent(event->AsKeyEvent());
}

ui::EventDispatchDetails InputMethodFuchsia::DispatchKeyEvent(
    ui::KeyEvent* event) {
  DCHECK(event->type() == ET_KEY_PRESSED || event->type() == ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event, base::NullCallback());

  // Insert the character.
  ui::EventDispatchDetails dispatch_details =
      DispatchKeyEventPostIME(event, base::NullCallback());
  if (!event->stopped_propagation() && !dispatch_details.dispatcher_destroyed &&
      event->type() == ET_KEY_PRESSED && GetTextInputClient()) {
    const uint16_t ch = event->GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(*event);
      event->StopPropagation();
    }
  }
  return dispatch_details;
}

void InputMethodFuchsia::OnCaretBoundsChanged(const TextInputClient* client) {}

void InputMethodFuchsia::CancelComposition(const TextInputClient* client) {}

bool InputMethodFuchsia::IsCandidatePopupOpen() const {
  return false;
}

void InputMethodFuchsia::OnFocus() {
  DCHECK(!ime_);

  // TODO(crbug.com/876934): Instantiate the IME with details about the text
  // being edited.
  fuchsia::ui::input::TextInputState state = {};
  state.text = "";
  ime_service_->GetInputMethodEditor(
      fuchsia::ui::input::KeyboardType::TEXT,
      fuchsia::ui::input::InputMethodAction::UNSPECIFIED, std::move(state),
      ime_client_binding_.NewBinding(), ime_.NewRequest());
}

void InputMethodFuchsia::OnBlur() {
  virtual_keyboard_controller_.DismissVirtualKeyboard();
  ime_client_binding_.Unbind();
  ime_.Unbind();
}

void InputMethodFuchsia::DidUpdateState(
    fuchsia::ui::input::TextInputState state,
    std::unique_ptr<fuchsia::ui::input::InputEvent> input_event) {
  if (input_event->is_keyboard())
    event_converter_.ProcessEvent(*input_event);
  else
    NOTIMPLEMENTED();
}

void InputMethodFuchsia::OnAction(
    fuchsia::ui::input::InputMethodAction action) {
  if (action != fuchsia::ui::input::InputMethodAction::UNSPECIFIED) {
    NOTIMPLEMENTED();
    return;
  }

  // Synthesize an ENTER keypress and send it to the Window.
  KeyEvent key_event(ET_KEY_PRESSED, KeyboardCode::VKEY_RETURN,
                     ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                     ui::EventTimeForNow());
  DispatchKeyEvent(&key_event);
}

}  // namespace ui
