// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/mock_input_method.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

namespace views {

MockInputMethod::MockInputMethod()
    : untranslated_ime_message_called_(false),
      text_input_type_changed_(false),
      cancel_composition_called_(false) {
}

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : untranslated_ime_message_called_(false),
      text_input_type_changed_(false),
      cancel_composition_called_(false) {
  SetDelegate(delegate);
}

MockInputMethod::~MockInputMethod() {
}

void MockInputMethod::OnFocus() {}

void MockInputMethod::OnBlur() {}

bool MockInputMethod::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  untranslated_ime_message_called_ = true;
  if (result)
    *result = InputMethod::NativeEventResult();
  return false;
}

void MockInputMethod::DispatchKeyEvent(const ui::KeyEvent& key) {
  bool handled = !IsTextInputTypeNone() && HasComposition();

  ClearStates();
  if (handled) {
    DCHECK(!key.is_char());
    ui::KeyEvent mock_key(ui::ET_KEY_PRESSED,
                          ui::VKEY_PROCESSKEY,
                          key.flags());
    DispatchKeyEventPostIME(mock_key);
  } else {
    DispatchKeyEventPostIME(key);
  }

  ui::TextInputClient* client = GetTextInputClient();
  if (client) {
    if (handled) {
      if (result_text_.length())
        client->InsertText(result_text_);
      if (composition_.text.length())
        client->SetCompositionText(composition_);
      else
        client->ClearCompositionText();
    } else if (key.type() == ui::ET_KEY_PRESSED) {
      base::char16 ch = key.GetCharacter();
      client->InsertChar(ch, key.flags());
    }
  }

  ClearComposition();
}

void MockInputMethod::OnTextInputTypeChanged(View* view) {
  if (IsViewFocused(view))
    text_input_type_changed_ = true;
  InputMethodBase::OnTextInputTypeChanged(view);
}

void MockInputMethod::OnCaretBoundsChanged(View* view) {
}

void MockInputMethod::CancelComposition(View* view) {
  if (IsViewFocused(view)) {
    cancel_composition_called_ = true;
    ClearComposition();
  }
}

void MockInputMethod::OnInputLocaleChanged() {
}

std::string MockInputMethod::GetInputLocale() {
  return "en-US";
}

bool MockInputMethod::IsActive() {
  return true;
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void MockInputMethod::ShowImeIfNeeded() {
}

void MockInputMethod::OnWillChangeFocus(View* focused_before, View* focused)  {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();
  ClearComposition();
}

void MockInputMethod::Clear() {
  ClearStates();
  ClearComposition();
}

void MockInputMethod::SetCompositionTextForNextKey(
    const ui::CompositionText& composition) {
  composition_ = composition;
}

void MockInputMethod::SetResultTextForNextKey(const base::string16& result) {
  result_text_ = result;
}

void MockInputMethod::ClearStates() {
  untranslated_ime_message_called_ = false;
  text_input_type_changed_ = false;
  cancel_composition_called_ = false;
}

bool MockInputMethod::HasComposition() {
  return composition_.text.length() || result_text_.length();
}

void MockInputMethod::ClearComposition() {
  composition_.Clear();
  result_text_.clear();
}

}  // namespace views
