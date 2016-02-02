// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

InputMethodBase::InputMethodBase()
  : delegate_(NULL),
    text_input_client_(NULL) {
}

InputMethodBase::~InputMethodBase() {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnInputMethodDestroyed(this));
}

void InputMethodBase::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void InputMethodBase::OnFocus() {}

void InputMethodBase::OnBlur() {}

void InputMethodBase::SetFocusedTextInputClient(TextInputClient* client) {
  SetFocusedTextInputClientInternal(client);
}

void InputMethodBase::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ != client)
    return;
  SetFocusedTextInputClientInternal(NULL);
}

TextInputClient* InputMethodBase::GetTextInputClient() const {
  return text_input_client_;
}

void InputMethodBase::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputStateChanged(client);
}

TextInputType InputMethodBase::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : TEXT_INPUT_TYPE_NONE;
}

TextInputMode InputMethodBase::GetTextInputMode() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputMode() : TEXT_INPUT_MODE_DEFAULT;
}

int InputMethodBase::GetTextInputFlags() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputFlags() : 0;
}

bool InputMethodBase::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

void InputMethodBase::ShowImeIfNeeded() {
  FOR_EACH_OBSERVER(InputMethodObserver, observer_list_, OnShowImeIfNeeded());
}

void InputMethodBase::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InputMethodBase::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool InputMethodBase::IsTextInputClientFocused(const TextInputClient* client) {
  return client && (client == GetTextInputClient());
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  TextInputClient* client = GetTextInputClient();
  if (!IsTextInputTypeNone())
    client->OnInputMethodChanged();
}

ui::EventDispatchDetails InputMethodBase::DispatchKeyEventPostIME(
    ui::KeyEvent* event) const {
  ui::EventDispatchDetails details;
  if (delegate_)
    details = delegate_->DispatchKeyEventPostIME(event);
  return details;
}

void InputMethodBase::NotifyTextInputStateChanged(
    const TextInputClient* client) {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnTextInputStateChanged(client));
}

void InputMethodBase::NotifyTextInputCaretBoundsChanged(
    const TextInputClient* client) {
  FOR_EACH_OBSERVER(
      InputMethodObserver, observer_list_, OnCaretBoundsChanged(client));
}

void InputMethodBase::SetFocusedTextInputClientInternal(
    TextInputClient* client) {
  TextInputClient* old = text_input_client_;
  if (old == client)
    return;
  OnWillChangeFocusedClient(old, client);
  text_input_client_ = client;  // NULL allowed.
  OnDidChangeFocusedClient(old, client);
  NotifyTextInputStateChanged(text_input_client_);
}

std::vector<gfx::Rect> InputMethodBase::GetCompositionBounds(
    const TextInputClient* client) {
  std::vector<gfx::Rect> bounds;
  if (client->HasCompositionText()) {
    uint32_t i = 0;
    gfx::Rect rect;
    while (client->GetCompositionCharacterBounds(i++, &rect))
      bounds.push_back(rect);
  } else {
    // For case of no composition at present, use caret bounds which is required
    // by the IME extension for certain features (e.g. physical keyboard
    // auto-correct).
    bounds.push_back(client->GetCaretBounds());
  }
  return bounds;
}

}  // namespace ui
