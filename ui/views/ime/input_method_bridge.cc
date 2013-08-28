// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/input_method_bridge.h"

#include "ui/base/events/event.h"
#include "ui/base/ime/input_method.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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
  if (host_->GetTextInputClient() == this)
    host_->SetFocusedTextInputClient(NULL);
}

void InputMethodBridge::OnFocus() {
  // Direct the shared IME to send TextInputClient messages to |this| object.
  if (shared_input_method_ || !host_->GetTextInputClient())
    host_->SetFocusedTextInputClient(this);

  // TODO(yusukes): We don't need to call OnTextInputTypeChanged() once we move
  // text input type tracker code to ui::InputMethodBase.
  if (GetFocusedView())
    OnTextInputTypeChanged(GetFocusedView());
}

void InputMethodBridge::OnBlur() {
  if (HasCompositionText()) {
    ConfirmCompositionText();
    host_->CancelComposition(this);
  }

  if (host_->GetTextInputClient() == this)
    host_->SetFocusedTextInputClient(NULL);
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
    host_->OnTextInputTypeChanged(this);
  InputMethodBase::OnTextInputTypeChanged(view);
}

void InputMethodBridge::OnCaretBoundsChanged(View* view) {
  if (IsViewFocused(view) && !IsTextInputTypeNone())
    host_->OnCaretBoundsChanged(this);
}

void InputMethodBridge::CancelComposition(View* view) {
  if (IsViewFocused(view))
    host_->CancelComposition(this);
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

// Overridden from TextInputClient. Forward an event from the system-wide IME
// to the text input |client|, which is e.g. views::NativeTextfieldViews.
void InputMethodBridge::SetCompositionText(
    const ui::CompositionText& composition) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->SetCompositionText(composition);
}

void InputMethodBridge::ConfirmCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->ConfirmCompositionText();
}

void InputMethodBridge::ClearCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->ClearCompositionText();
}

void InputMethodBridge::InsertText(const string16& text) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->InsertText(text);
}

void InputMethodBridge::InsertChar(char16 ch, int flags) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->InsertChar(ch, flags);
}

gfx::NativeWindow InputMethodBridge::GetAttachedWindow() const {
  TextInputClient* client = GetTextInputClient();
  return client ?
      client->GetAttachedWindow() : static_cast<gfx::NativeWindow>(NULL);
}

ui::TextInputType InputMethodBridge::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
}

ui::TextInputMode InputMethodBridge::GetTextInputMode() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputMode() : ui::TEXT_INPUT_MODE_DEFAULT;
}

bool InputMethodBridge::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

gfx::Rect InputMethodBridge::GetCaretBounds() {
  TextInputClient* client = GetTextInputClient();
  if (!client)
    return gfx::Rect();

  return client->GetCaretBounds();
}

bool InputMethodBridge::GetCompositionCharacterBounds(uint32 index,
                                                      gfx::Rect* rect) {
  DCHECK(rect);
  TextInputClient* client = GetTextInputClient();
  if (!client)
    return false;

  return client->GetCompositionCharacterBounds(index, rect);
}

bool InputMethodBridge::HasCompositionText() {
  TextInputClient* client = GetTextInputClient();
  return client ? client->HasCompositionText() : false;
}

bool InputMethodBridge::GetTextRange(ui::Range* range) {
  TextInputClient* client = GetTextInputClient();
  return client ?  client->GetTextRange(range) : false;
}

bool InputMethodBridge::GetCompositionTextRange(ui::Range* range) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetCompositionTextRange(range) : false;
}

bool InputMethodBridge::GetSelectionRange(ui::Range* range) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetSelectionRange(range) : false;
}

bool InputMethodBridge::SetSelectionRange(const ui::Range& range) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->SetSelectionRange(range) : false;
}

bool InputMethodBridge::DeleteRange(const ui::Range& range) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->DeleteRange(range) : false;
}

bool InputMethodBridge::GetTextFromRange(
    const ui::Range& range, string16* text) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextFromRange(range, text) : false;
}

void InputMethodBridge::OnInputMethodChanged() {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->OnInputMethodChanged();
}

bool InputMethodBridge::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  TextInputClient* client = GetTextInputClient();
  return client ?
      client->ChangeTextDirectionAndLayoutAlignment(direction) : false;
}

void InputMethodBridge::ExtendSelectionAndDelete(size_t before, size_t after) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->ExtendSelectionAndDelete(before, after);
}

void InputMethodBridge::EnsureCaretInRect(const gfx::Rect& rect) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->EnsureCaretInRect(rect);
}

// Overridden from FocusChangeListener.
void InputMethodBridge::OnWillChangeFocus(View* focused_before, View* focused) {
  if (HasCompositionText()) {
    ConfirmCompositionText();
    CancelComposition(focused_before);
  }
}

void InputMethodBridge::OnDidChangeFocus(View* focused_before, View* focused) {
  DCHECK_EQ(GetFocusedView(), focused);
  OnTextInputTypeChanged(focused);
  OnCaretBoundsChanged(focused);
}

ui::InputMethod* InputMethodBridge::GetHostInputMethod() const {
  return host_;
}


}  // namespace views
