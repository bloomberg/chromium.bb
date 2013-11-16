// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_linux_x11.h"

#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"

namespace ui {

InputMethodLinuxX11::InputMethodLinuxX11(
    internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodLinuxX11::~InputMethodLinuxX11() {}

// Overriden from InputMethod.

void InputMethodLinuxX11::Init(bool focused) {
  CHECK(LinuxInputMethodContextFactory::instance());
  input_method_context_ =
      LinuxInputMethodContextFactory::instance()->CreateInputMethodContext(
          this);
  CHECK(input_method_context_.get());

  InputMethodBase::Init(focused);

  if (focused) {
    input_method_context_->OnTextInputTypeChanged(
        GetTextInputClient() ?
        GetTextInputClient()->GetTextInputType() :
        TEXT_INPUT_TYPE_TEXT);
  }
}

bool InputMethodLinuxX11::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

bool InputMethodLinuxX11::DispatchKeyEvent(const ui::KeyEvent& event) {
  DCHECK(event.type() == ET_KEY_PRESSED || event.type() == ET_KEY_RELEASED);
  DCHECK(system_toplevel_window_focused());

  if (!event.HasNativeEvent())
    return DispatchFabricatedKeyEvent(event);

  // If no text input client, do nothing.
  const base::NativeEvent& native_key_event = event.native_event();
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(native_key_event);

  // Let an IME handle the key event first.
  if (input_method_context_->DispatchKeyEvent(native_key_event)) {
    if (event.type() == ET_KEY_PRESSED)
      DispatchFabricatedKeyEventPostIME(ET_KEY_PRESSED, VKEY_PROCESSKEY,
                                        event.flags());
    return true;
  }

  // Otherwise, insert the character.
  const bool handled = DispatchKeyEventPostIME(native_key_event);
  if (event.type() == ET_KEY_PRESSED && GetTextInputClient()) {
    const uint16 ch = event.GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(ch, event.flags());
      return true;
    }
  }
  return handled;
}

void InputMethodLinuxX11::OnTextInputTypeChanged(
    const TextInputClient* client) {
  if (IsTextInputClientFocused(client)) {
    input_method_context_->Reset();
    // TODO(yoichio): Support inputmode HTML attribute.
    input_method_context_->OnTextInputTypeChanged(client->GetTextInputType());
  }
  InputMethodBase::OnTextInputTypeChanged(client);
}

void InputMethodLinuxX11::OnCaretBoundsChanged(const TextInputClient* client) {
  if (IsTextInputClientFocused(client)) {
    input_method_context_->OnCaretBoundsChanged(
        GetTextInputClient()->GetCaretBounds());
  }
  InputMethodBase::OnCaretBoundsChanged(client);
}

void InputMethodLinuxX11::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  input_method_context_->Reset();
  input_method_context_->OnTextInputTypeChanged(client->GetTextInputType());
}

void InputMethodLinuxX11::OnInputLocaleChanged() {
  InputMethodBase::OnInputLocaleChanged();
}

std::string InputMethodLinuxX11::GetInputLocale() {
  return "";
}

base::i18n::TextDirection InputMethodLinuxX11::GetInputTextDirection() {
  return input_method_context_->GetInputTextDirection();
}

bool InputMethodLinuxX11::IsActive() {
  // InputMethodLinuxX11 is always ready and up.
  return true;
}

bool InputMethodLinuxX11::IsCandidatePopupOpen() const {
  // There seems no way to detect candidate windows or any popups.
  return false;
}

// Overriden from ui::LinuxInputMethodContextDelegate

void InputMethodLinuxX11::OnCommit(const base::string16& text) {
  TextInputClient* text_input_client = GetTextInputClient();
  if (text_input_client)
    text_input_client->InsertText(text);
}

void InputMethodLinuxX11::OnPreeditChanged(
    const CompositionText& composition_text) {
  TextInputClient* text_input_client = GetTextInputClient();
  if (text_input_client)
    text_input_client->SetCompositionText(composition_text);
}

void InputMethodLinuxX11::OnPreeditEnd() {
  TextInputClient* text_input_client = GetTextInputClient();
  if (text_input_client && text_input_client->HasCompositionText())
    text_input_client->ClearCompositionText();
}

void InputMethodLinuxX11::OnPreeditStart() {}

// Overridden from InputMethodBase.

void InputMethodLinuxX11::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  input_method_context_->Reset();
  input_method_context_->OnTextInputTypeChanged(
      focused ? focused->GetTextInputType() : TEXT_INPUT_TYPE_NONE);

  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
}

// private

bool InputMethodLinuxX11::DispatchFabricatedKeyEvent(
    const ui::KeyEvent& event) {
  // Let a post IME handler handle the key event.
  if (DispatchFabricatedKeyEventPostIME(event.type(), event.key_code(),
                                        event.flags()))
    return true;

  // Otherwise, insert the character.
  if (event.type() == ET_KEY_PRESSED && GetTextInputClient()) {
    const uint16 ch = event.GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(ch, event.flags());
      return true;
    }
  }

  return false;
}

}  // namespace ui
