// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_input_method.h"

namespace ui {

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : text_input_client_(NULL) {
}

MockInputMethod::~MockInputMethod() {
}

void MockInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
}

void MockInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
  text_input_client_ = client;
}

TextInputClient* MockInputMethod::GetTextInputClient() const {
  return text_input_client_;
}

void MockInputMethod::DispatchKeyEvent(const base::NativeEvent& native_event) {
}

void MockInputMethod::DispatchFabricatedKeyEvent(const ui::KeyEvent& event) {
}

void MockInputMethod::Init(bool focused) {
}

void MockInputMethod::OnFocus() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnFocus());
}

void MockInputMethod::OnBlur() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnBlur());
}

void MockInputMethod::OnTextInputTypeChanged(const TextInputClient* client) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnTextInputTypeChanged(client));
}

void MockInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnCaretBoundsChanged(client));
}

void MockInputMethod::CancelComposition(const TextInputClient* client) {
}

std::string MockInputMethod::GetInputLocale() {
  return "";
}

base::i18n::TextDirection MockInputMethod::GetInputTextDirection() {
  return base::i18n::UNKNOWN_DIRECTION;
}

bool MockInputMethod::IsActive() {
  return true;
}

ui::TextInputType MockInputMethod::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool MockInputMethod::CanComposeInline() const {
  return true;
}

void MockInputMethod::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MockInputMethod::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace ui
