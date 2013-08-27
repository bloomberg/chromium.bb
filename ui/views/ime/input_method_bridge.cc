// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/input_method_bridge.h"

#include "ui/base/events/event.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"

namespace views {

InputMethodBridge::InputMethodBridge(internal::InputMethodDelegate* delegate,
                                     ui::InputMethod* host,
                                     bool shared_input_method)
    : host_(host),
      shared_input_method_(shared_input_method),
      context_focused_(false) {
  DCHECK(host_);
  SetDelegate(delegate);
}

InputMethodBridge::~InputMethodBridge() {
  // By the time we get here it's very likely |widget_|'s NativeWidget has been
  // destroyed. This means any calls to |widget_| that go to the NativeWidget,
  // such as IsActive(), will crash. SetFocusedTextInputClient() may callback to
  // this and go into |widget_|. NULL out |widget_| so we don't attempt to use
  // it.
  DetachFromWidget();
}

bool InputMethodBridge::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                                 NativeEventResult* result) {
  return host_->OnUntranslatedIMEMessage(event, result);
}

void InputMethodBridge::DispatchKeyEvent(const ui::KeyEvent& key) {
  DCHECK(key.type() == ui::ET_KEY_PRESSED || key.type() == ui::ET_KEY_RELEASED);

  // We can just dispatch the event here since the |key| is already processed by
  // the system-wide IME.
  DispatchKeyEventPostIME(key);
}

void InputMethodBridge::OnTextInputTypeChanged(View* view) {
  if (IsViewFocused(view))
    host_->OnTextInputTypeChanged(GetTextInputClient());
}

void InputMethodBridge::OnCaretBoundsChanged(View* view) {
  if (IsViewFocused(view) && !IsTextInputTypeNone())
    host_->OnCaretBoundsChanged(GetTextInputClient());
}

void InputMethodBridge::CancelComposition(View* view) {
  if (IsViewFocused(view))
    host_->CancelComposition(GetTextInputClient());
}

void InputMethodBridge::OnInputLocaleChanged() {
  return host_->OnInputLocaleChanged();
}

std::string InputMethodBridge::GetInputLocale() {
  return host_->GetInputLocale();
}

base::i18n::TextDirection InputMethodBridge::GetInputTextDirection() {
  return host_->GetInputTextDirection();
}

bool InputMethodBridge::IsActive() {
  return host_->IsActive();
}

bool InputMethodBridge::IsCandidatePopupOpen() const {
  return host_->IsCandidatePopupOpen();
}

void InputMethodBridge::OnWillChangeFocus(View* focused_before, View* focused) {
  if (GetTextInputClient() && GetTextInputClient()->HasCompositionText()) {
    GetTextInputClient()->ConfirmCompositionText();
    CancelComposition(focused_before);
  }
}

void InputMethodBridge::OnDidChangeFocus(View* focused_before, View* focused) {
  DCHECK_EQ(GetFocusedView(), focused);
  host_->SetFocusedTextInputClient(GetTextInputClient());
  OnTextInputTypeChanged(focused);
  OnCaretBoundsChanged(focused);
}

ui::InputMethod* InputMethodBridge::GetHostInputMethod() const {
  return host_;
}


}  // namespace views
