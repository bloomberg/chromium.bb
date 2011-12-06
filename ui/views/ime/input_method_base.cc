// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_client.h"
#include "ui/views/ime/input_method_base.h"
#include "ui/views/ime/text_input_type_tracker.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#include "base/logging.h"

namespace views {

InputMethodBase::InputMethodBase()
    : delegate_(NULL),
      widget_(NULL),
      widget_focused_(false) {
}

InputMethodBase::~InputMethodBase() {
  if (widget_) {
    widget_->GetFocusManager()->RemoveFocusChangeListener(this);
    widget_ = NULL;
  }
}

void InputMethodBase::set_delegate(internal::InputMethodDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void InputMethodBase::Init(Widget* widget) {
  DCHECK(widget);
  DCHECK(widget->GetFocusManager());

  if (widget_) {
    NOTREACHED() << "The input method is already initialized.";
    return;
  }

  widget_ = widget;
  // The InputMethod is lazily created, so we need to tell it the currently
  // focused view.
  View* focused = widget->GetFocusManager()->GetFocusedView();
  if (focused)
    OnWillChangeFocus(NULL, focused);
  widget->GetFocusManager()->AddFocusChangeListener(this);
}

void InputMethodBase::OnFocus() {
  widget_focused_ = true;
  TextInputTypeTracker::GetInstance()->OnTextInputTypeChanged(
      GetTextInputType(), widget_);
}

void InputMethodBase::OnBlur() {
  widget_focused_ = false;
  TextInputTypeTracker::GetInstance()->OnTextInputTypeChanged(
      GetTextInputType(), widget_);
}

views::View* InputMethodBase::GetFocusedView() const {
  return widget_->GetFocusManager()->GetFocusedView();
}

void InputMethodBase::OnTextInputTypeChanged(View* view) {
  if (IsViewFocused(view)) {
    TextInputTypeTracker::GetInstance()->OnTextInputTypeChanged(
        GetTextInputType(), widget_);
  }
}

ui::TextInputClient* InputMethodBase::GetTextInputClient() const {
  return (widget_focused_ && GetFocusedView()) ?
      GetFocusedView()->GetTextInputClient() : NULL;
}

ui::TextInputType InputMethodBase::GetTextInputType() const {
  ui::TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
}

bool InputMethodBase::IsMock() const {
  return false;
}

void InputMethodBase::OnWillChangeFocus(View* focused_before, View* focused) {
}

void InputMethodBase::OnDidChangeFocus(View* focused_before, View* focused) {
  if (widget_focused_) {
    TextInputTypeTracker::GetInstance()->OnTextInputTypeChanged(
        GetTextInputType(), widget_);
  }
}

bool InputMethodBase::IsViewFocused(View* view) const {
  return widget_focused_ && view && GetFocusedView() == view;
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
    client->OnInputMethodChanged();
}

void InputMethodBase::DispatchKeyEventPostIME(const KeyEvent& key) const {
  if (delegate_)
    delegate_->DispatchKeyEventPostIME(key);
}

bool InputMethodBase::GetCaretBoundsInWidget(gfx::Rect* rect) const {
  DCHECK(rect);
  ui::TextInputClient* client = GetTextInputClient();
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  *rect = GetFocusedView()->ConvertRectToWidget(client->GetCaretBounds());

  // We need to do coordinate conversion if the focused view is inside a child
  // Widget.
  if (GetFocusedView()->GetWidget() != widget_)
    return Widget::ConvertRect(GetFocusedView()->GetWidget(), widget_, rect);
  return true;
}

}  // namespace views
