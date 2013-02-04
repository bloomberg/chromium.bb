// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_input_method.h"

#include "ui/base/ime/text_input_client.h"

namespace ui {

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : delegate_(NULL),
      text_input_client_(NULL),
      init_callcount_(0),
      on_focus_callcount_(0),
      on_blur_callcaount_(0),
      set_focused_text_input_client_callcount_(0),
      dispatch_keyevent_callcount_(0),
      dispatch_fabricated_keyevent_callcount_(0),
      on_text_input_type_changed_callcount_(0),
      on_caret_bounds_changed_callcount_(0),
      cancel_composition_callcount_(0),
      get_input_locale_callcount_(0),
      get_input_text_direction_callcount_(0),
      is_active_callcount_(0),
      latest_text_input_type_(ui::TEXT_INPUT_TYPE_NONE) {
  SetDelegate(delegate);
}

MockInputMethod::~MockInputMethod() {
}

void MockInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void MockInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
  text_input_client_ = client;
  ++set_focused_text_input_client_callcount_;
}

TextInputClient* MockInputMethod::GetTextInputClient() const {
  return text_input_client_;
}

void MockInputMethod::DispatchKeyEvent(const base::NativeEvent& native_event) {
  ++dispatch_keyevent_callcount_;
}

void MockInputMethod::DispatchFabricatedKeyEvent(const ui::KeyEvent& event) {
  ++dispatch_fabricated_keyevent_callcount_;
}

void MockInputMethod::Init(bool focused) {
  ++init_callcount_;
}

void MockInputMethod::OnFocus() {
  ++on_focus_callcount_;
}

void MockInputMethod::OnBlur() {
  ++on_blur_callcaount_;
}

void MockInputMethod::OnTextInputTypeChanged(const TextInputClient* client) {
  ++on_text_input_type_changed_callcount_;
  latest_text_input_type_ = client->GetTextInputType();
}

void MockInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
  ++on_caret_bounds_changed_callcount_;
}

void MockInputMethod::CancelComposition(const TextInputClient* client) {
  ++cancel_composition_callcount_;
}


std::string MockInputMethod::GetInputLocale() {
  ++get_input_locale_callcount_;
  return "";
}

base::i18n::TextDirection MockInputMethod::GetInputTextDirection() {
  ++get_input_text_direction_callcount_;
  return base::i18n::UNKNOWN_DIRECTION;
}

bool MockInputMethod::IsActive() {
  ++is_active_callcount_;
  return true;
}

ui::TextInputType MockInputMethod::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool MockInputMethod::CanComposeInline() const {
  return true;
}

}  // namespace ui
