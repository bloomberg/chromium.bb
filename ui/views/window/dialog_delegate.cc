// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_delegate.h"

#include "base/logging.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/text_button.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DialogDelegate:

DialogDelegate* DialogDelegate::AsDialogDelegate() { return this; }

int DialogDelegate::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

int DialogDelegate::GetDefaultDialogButton() const {
  if (GetDialogButtons() & ui::DIALOG_BUTTON_OK)
    return ui::DIALOG_BUTTON_OK;
  if (GetDialogButtons() & ui::DIALOG_BUTTON_CANCEL)
    return ui::DIALOG_BUTTON_CANCEL;
  return ui::DIALOG_BUTTON_NONE;
}

string16 DialogDelegate::GetDialogButtonLabel(ui::DialogButton button) const {
  // Empty string results in defaults for
  // ui::DIALOG_BUTTON_OK or ui::DIALOG_BUTTON_CANCEL.
  return string16();
}

bool DialogDelegate::IsDialogButtonEnabled(ui::DialogButton button) const {
  return true;
}

bool DialogDelegate::IsDialogButtonVisible(ui::DialogButton button) const {
  return true;
}

bool DialogDelegate::AreAcceleratorsEnabled(ui::DialogButton button) {
  return true;
}

View* DialogDelegate::GetExtraView() {
  return NULL;
}

bool DialogDelegate::GetSizeExtraViewHeightToButtons() {
  return false;
}

bool DialogDelegate::Cancel() {
  return true;
}

bool DialogDelegate::Accept(bool window_closiang) {
  return Accept();
}

bool DialogDelegate::Accept() {
  return true;
}

View* DialogDelegate::GetInitiallyFocusedView() {
  // Focus the default button if any.
  const DialogClientView* dcv = GetDialogClientView();
  int default_button = GetDefaultDialogButton();
  if (default_button == ui::DIALOG_BUTTON_NONE)
    return NULL;

  if ((default_button & GetDialogButtons()) == 0) {
    // The default button is a button we don't have.
    NOTREACHED();
    return NULL;
  }

  if (default_button & ui::DIALOG_BUTTON_OK)
    return dcv->ok_button();
  if (default_button & ui::DIALOG_BUTTON_CANCEL)
    return dcv->cancel_button();
  return NULL;
}

ClientView* DialogDelegate::CreateClientView(Widget* widget) {
  return new DialogClientView(widget, GetContentsView());
}

const DialogClientView* DialogDelegate::GetDialogClientView() const {
  return GetWidget()->client_view()->AsDialogClientView();
}

DialogClientView* DialogDelegate::GetDialogClientView() {
  return GetWidget()->client_view()->AsDialogClientView();
}

ui::AccessibilityTypes::Role DialogDelegate::GetAccessibleWindowRole() const {
  return ui::AccessibilityTypes::ROLE_DIALOG;
}

////////////////////////////////////////////////////////////////////////////////
// DialogDelegateView:

DialogDelegateView::DialogDelegateView() {
}

DialogDelegateView::~DialogDelegateView() {
}

Widget* DialogDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* DialogDelegateView::GetWidget() const {
  return View::GetWidget();
}

}  // namespace views
