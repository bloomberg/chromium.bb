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
                                     ui::InputMethod* host)
    : host_(host),
      context_focused_(false) {
  DCHECK(host_);
  set_delegate(delegate);
}

InputMethodBridge::~InputMethodBridge() {
  if (host_->GetTextInputClient() == this)
    host_->SetFocusedTextInputClient(NULL);
}

void InputMethodBridge::Init(Widget* widget) {
  InputMethodBase::Init(widget);
}

void InputMethodBridge::OnFocus() {
  InputMethodBase::OnFocus();

  // Ask the system-wide IME to send all TextInputClient messages to |this|
  // object.
  host_->SetFocusedTextInputClient(this);

  // TODO(yusukes): We don't need to call OnTextInputTypeChanged() once we move
  // text input type tracker code to ui::InputMethodBase.
  if (GetFocusedView())
    OnTextInputTypeChanged(GetFocusedView());
}

void InputMethodBridge::OnBlur() {
  DCHECK(widget_focused());

  ConfirmCompositionText();
  InputMethodBase::OnBlur();
  if (host_->GetTextInputClient() == this)
    host_->SetFocusedTextInputClient(NULL);
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

std::string InputMethodBridge::GetInputLocale() {
  return host_->GetInputLocale();
}

base::i18n::TextDirection InputMethodBridge::GetInputTextDirection() {
  return host_->GetInputTextDirection();
}

bool InputMethodBridge::IsActive() {
  return host_->IsActive();
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

ui::TextInputType InputMethodBridge::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
}

bool InputMethodBridge::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

gfx::Rect InputMethodBridge::ConvertRectToFocusedView(const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());
  View::ConvertPointToScreen(GetFocusedView(), &origin);
  View::ConvertPointToScreen(GetFocusedView(), &end);
  return gfx::Rect(origin.x(),
                   origin.y(),
                   end.x() - origin.x(),
                   end.y() - origin.y());
}

gfx::Rect InputMethodBridge::GetCaretBounds() {
  TextInputClient* client = GetTextInputClient();
  if (!client || !GetFocusedView())
    return gfx::Rect();

  const gfx::Rect rect = client->GetCaretBounds();
  return ConvertRectToFocusedView(rect);
}

bool InputMethodBridge::GetCompositionCharacterBounds(uint32 index,
                                                      gfx::Rect* rect) {
  DCHECK(rect);
  TextInputClient* client = GetTextInputClient();
  if (!client || !GetFocusedView())
    return false;

  gfx::Rect relative_rect;
  if (!client->GetCompositionCharacterBounds(index, &relative_rect))
    return false;
  *rect = ConvertRectToFocusedView(relative_rect);
  return true;
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

// Overridden from FocusChangeListener.
void InputMethodBridge::OnWillChangeFocus(View* focused_before, View* focused) {
  ConfirmCompositionText();
}

void InputMethodBridge::OnDidChangeFocus(View* focused_before, View* focused) {
  OnTextInputTypeChanged(GetFocusedView());
  OnCaretBoundsChanged(GetFocusedView());
}

}  // namespace views
