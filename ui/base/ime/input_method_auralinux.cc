// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_auralinux.h"

#include "base/environment.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

InputMethodAuraLinux::InputMethodAuraLinux(
    internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodAuraLinux::~InputMethodAuraLinux() {}

// Overriden from InputMethod.

void InputMethodAuraLinux::Init(bool focused) {
  CHECK(LinuxInputMethodContextFactory::instance())
      << "This failure was likely caused because "
      << "ui::InitializeInputMethod(ForTesting) was not called "
      << "before instantiating this class.";
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

bool InputMethodAuraLinux::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

bool InputMethodAuraLinux::DispatchKeyEvent(const ui::KeyEvent& event) {
  DCHECK(event.type() == ET_KEY_PRESSED || event.type() == ET_KEY_RELEASED);
  DCHECK(system_toplevel_window_focused());

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event);

  // Let an IME handle the key event first.
  if (input_method_context_->DispatchKeyEvent(event)) {
    if (event.type() == ET_KEY_PRESSED) {
      const ui::KeyEvent fabricated_event(ET_KEY_PRESSED,
                                          VKEY_PROCESSKEY,
                                          event.flags(),
                                          false);  // is_char
      DispatchKeyEventPostIME(fabricated_event);
    }
    return true;
  }

  // Otherwise, insert the character.
  const bool handled = DispatchKeyEventPostIME(event);
  if (event.type() == ET_KEY_PRESSED && GetTextInputClient()) {
    const uint16 ch = event.GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(ch, event.flags());
      return true;
    }
  }
  return handled;
}

void InputMethodAuraLinux::OnTextInputTypeChanged(
    const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  input_method_context_->Reset();
  // TODO(yoichio): Support inputmode HTML attribute.
  input_method_context_->OnTextInputTypeChanged(client->GetTextInputType());
}

void InputMethodAuraLinux::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  input_method_context_->OnCaretBoundsChanged(
      GetTextInputClient()->GetCaretBounds());
}

void InputMethodAuraLinux::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  input_method_context_->Reset();
  input_method_context_->OnTextInputTypeChanged(client->GetTextInputType());
}

void InputMethodAuraLinux::OnInputLocaleChanged() {
}

std::string InputMethodAuraLinux::GetInputLocale() {
  return "";
}

bool InputMethodAuraLinux::IsActive() {
  // InputMethodAuraLinux is always ready and up.
  return true;
}

bool InputMethodAuraLinux::IsCandidatePopupOpen() const {
  // There seems no way to detect candidate windows or any popups.
  return false;
}

// Overriden from ui::LinuxInputMethodContextDelegate

void InputMethodAuraLinux::OnCommit(const base::string16& text) {
  if (!IsTextInputTypeNone())
    GetTextInputClient()->InsertText(text);
}

void InputMethodAuraLinux::OnPreeditChanged(
    const CompositionText& composition_text) {
  TextInputClient* text_input_client = GetTextInputClient();
  if (text_input_client)
    text_input_client->SetCompositionText(composition_text);
}

void InputMethodAuraLinux::OnPreeditEnd() {
  TextInputClient* text_input_client = GetTextInputClient();
  if (text_input_client && text_input_client->HasCompositionText())
    text_input_client->ClearCompositionText();
}

void InputMethodAuraLinux::OnPreeditStart() {}

// Overridden from InputMethodBase.

void InputMethodAuraLinux::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  input_method_context_->Reset();
  input_method_context_->OnTextInputTypeChanged(
      focused ? focused->GetTextInputType() : TEXT_INPUT_TYPE_NONE);

  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
}

}  // namespace ui
