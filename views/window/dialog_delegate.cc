// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/dialog_delegate.h"

#include "base/logging.h"
#include "views/controls/button/native_button.h"
#include "views/window/window.h"

namespace views {

// Overridden from WindowDelegate:

DialogDelegate* DialogDelegate::AsDialogDelegate() { return this; }

int DialogDelegate::GetDialogButtons() const {
  return ui::MessageBoxFlags::DIALOGBUTTON_OK |
      ui::MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool DialogDelegate::AreAcceleratorsEnabled(
    ui::MessageBoxFlags::DialogButton button) {
  return true;
}

std::wstring DialogDelegate::GetDialogButtonLabel(
    ui::MessageBoxFlags::DialogButton button) const {
  // empty string results in defaults for
  // ui::MessageBoxFlags::DIALOGBUTTON_OK,
  // ui::MessageBoxFlags::DIALOGBUTTON_CANCEL.
  return L"";
}

View* DialogDelegate::GetExtraView() {
  return NULL;
}

bool DialogDelegate::GetSizeExtraViewHeightToButtons() {
  return false;
}

int DialogDelegate::GetDefaultDialogButton() const {
  if (GetDialogButtons() & MessageBoxFlags::DIALOGBUTTON_OK)
    return MessageBoxFlags::DIALOGBUTTON_OK;
  if (GetDialogButtons() & MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

bool DialogDelegate::IsDialogButtonEnabled(
    ui::MessageBoxFlags::DialogButton button) const {
  return true;
}

bool DialogDelegate::IsDialogButtonVisible(
    ui::MessageBoxFlags::DialogButton button) const {
  return true;
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
  DialogClientView* dcv = GetDialogClientView();
  int default_button = GetDefaultDialogButton();
  if (default_button == MessageBoxFlags::DIALOGBUTTON_NONE)
    return NULL;

  if ((default_button & GetDialogButtons()) == 0) {
    // The default button is a button we don't have.
    NOTREACHED();
    return NULL;
  }

  if (default_button & MessageBoxFlags::DIALOGBUTTON_OK)
    return dcv->ok_button();
  if (default_button & MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return dcv->cancel_button();
  return NULL;
}

ClientView* DialogDelegate::CreateClientView(Window* window) {
  return new DialogClientView(window, GetContentsView());
}

DialogClientView* DialogDelegate::GetDialogClientView() const {
  return window()->client_view()->AsDialogClientView();
}

AccessibilityTypes::Role DialogDelegate::accessible_role() const {
  return AccessibilityTypes::ROLE_DIALOG;
}

}  // namespace views
