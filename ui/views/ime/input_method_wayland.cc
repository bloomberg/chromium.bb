// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/input_method_wayland.h"

#include "views/widget/widget.h"

namespace views {

InputMethodWayland::InputMethodWayland(
    internal::InputMethodDelegate *delegate) {
  set_delegate(delegate);
}

void InputMethodWayland::DispatchKeyEvent(const KeyEvent& key) {
  if (!GetTextInputClient()) {
    DispatchKeyEventPostIME(key);
    return;
  }

  if (key.type() == ui::ET_KEY_PRESSED) {
    ProcessKeyPressEvent(key);
  } else if (key.type() == ui::ET_KEY_RELEASED) {
    DispatchKeyEventPostIME(key);
  }
}

void InputMethodWayland::OnTextInputTypeChanged(View* view) {
  NOTIMPLEMENTED();
}

void InputMethodWayland::OnCaretBoundsChanged(View* view) {
  NOTIMPLEMENTED();
}

void InputMethodWayland::CancelComposition(View* view) {
  NOTIMPLEMENTED();
}

std::string InputMethodWayland::GetInputLocale() {
  return std::string("");
}

base::i18n::TextDirection InputMethodWayland::GetInputTextDirection() {
  return base::i18n::UNKNOWN_DIRECTION;
}

bool InputMethodWayland::IsActive() {
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// InputMethodWayland private

void InputMethodWayland::ProcessKeyPressEvent(const KeyEvent& key) {
  const View* old_focused_view = focused_view();
  DispatchKeyEventPostIME(key);

  // We shouldn't dispatch the character anymore if the key event caused focus
  // change.
  if (old_focused_view != focused_view())
    return;

  // If a key event was not filtered by |context_| or |context_simple_|, then
  // it means the key event didn't generate any result text. For some cases,
  // the key event may still generate a valid character, eg. a control-key
  // event (ctrl-a, return, tab, etc.). We need to send the character to the
  // focused text input client by calling TextInputClient::InsertChar().
  char16 ch = key.GetCharacter();
  ui::TextInputClient* client = GetTextInputClient();
  if (ch && client)
    client->InsertChar(ch, key.flags());
}

}  // namespace views
