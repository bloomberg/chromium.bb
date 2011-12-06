// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/logging.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

InputMethodBase::InputMethodBase()
  : delegate_(NULL),
    text_input_client_(NULL),
    system_toplevel_window_focused_(false) {
}

InputMethodBase::~InputMethodBase() {
}

void InputMethodBase::SetDelegate(
    internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void InputMethodBase::Init(bool focused) {
  if (focused)
    OnFocus();
}

void InputMethodBase::OnFocus() {
  DCHECK(!system_toplevel_window_focused_);
  system_toplevel_window_focused_ = true;
}

void InputMethodBase::OnBlur() {
  DCHECK(system_toplevel_window_focused_);
  system_toplevel_window_focused_ = false;
}

void InputMethodBase::SetFocusedTextInputClient(TextInputClient* client) {
  TextInputClient* old = text_input_client_;
  OnWillChangeFocusedClient(old, client);
  text_input_client_ = client;  // NULL allowed.
  OnDidChangeFocusedClient(old, client);
}

TextInputClient* InputMethodBase::GetTextInputClient() const {
  return system_toplevel_window_focused_ ? text_input_client_ : NULL;
}

void InputMethodBase::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  // TODO(yusukes): Support TextInputTypeTracker for TOUCH_UI.
}

TextInputType InputMethodBase::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : TEXT_INPUT_TYPE_NONE;
}

bool InputMethodBase::IsTextInputClientFocused(const TextInputClient* client) {
  return client && (client == GetTextInputClient());
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  TextInputClient* client = GetTextInputClient();
  if (client && client->GetTextInputType() != TEXT_INPUT_TYPE_NONE)
    client->OnInputMethodChanged();
}

void InputMethodBase::DispatchKeyEventPostIME(
    const base::NativeEvent& native_event) const {
  if (delegate_)
    delegate_->DispatchKeyEventPostIME(native_event);
}

void InputMethodBase::DispatchFabricatedKeyEventPostIME(EventType type,
                                                        KeyboardCode key_code,
                                                        int flags) const {
  if (delegate_)
    delegate_->DispatchFabricatedKeyEventPostIME(type, key_code, flags);
}

}  // namespace ui
