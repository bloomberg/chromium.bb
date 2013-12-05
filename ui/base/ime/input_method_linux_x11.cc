// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_linux_x11.h"

#include "base/environment.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

InputMethodLinuxX11::InputMethodLinuxX11(
    internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodLinuxX11::~InputMethodLinuxX11() {}

// static
void InputMethodLinuxX11::Initialize() {
  // Force a IBus IM context to run in synchronous mode.
  //
  // Background: IBus IM context runs by default in asynchronous mode.  In
  // this mode, gtk_im_context_filter_keypress() consumes all the key events
  // and returns true while asynchronously sending the event to an underlying
  // IME implementation.  When the event has not actually been consumed by
  // the underlying IME implementation, the context pushes the event back to
  // the GDK event queue marking the event as already handled by the IBus IM
  // context.
  //
  // The problem here is that those pushed-back GDK events are never handled
  // when base::MessagePumpX11 is used, which only handles X events.  So, we
  // make a IBus IM context run in synchronous mode by setting an environment
  // variable.  This is only the interface to change the mode.
  //
  // Another possible solution is to use GDK event loop instead of X event
  // loop.
  //
  // Since there is no reentrant version of setenv(3C), it's a caller's duty
  // to avoid race conditions.  This function should be called in the main
  // thread on a very early stage, and supposed to be called from
  // ui::InitializeInputMethod().
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("IBUS_ENABLE_SYNC_MODE", "1");
}

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

void InputMethodLinuxX11::OnTextInputTypeChanged(
    const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  input_method_context_->Reset();
  // TODO(yoichio): Support inputmode HTML attribute.
  input_method_context_->OnTextInputTypeChanged(client->GetTextInputType());
}

void InputMethodLinuxX11::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  input_method_context_->OnCaretBoundsChanged(
      GetTextInputClient()->GetCaretBounds());
}

void InputMethodLinuxX11::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  input_method_context_->Reset();
  input_method_context_->OnTextInputTypeChanged(client->GetTextInputType());
}

void InputMethodLinuxX11::OnInputLocaleChanged() {
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

}  // namespace ui
