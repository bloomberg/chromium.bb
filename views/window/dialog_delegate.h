// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_DIALOG_DELEGATE_H_
#define VIEWS_WINDOW_DIALOG_DELEGATE_H_
#pragma once

#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/message_box_flags.h"
#include "views/window/dialog_client_view.h"
#include "views/window/window_delegate.h"

using ui::MessageBoxFlags;

namespace views {

class View;

///////////////////////////////////////////////////////////////////////////////
//
// DialogDelegate
//
//  DialogDelegate is an interface implemented by objects that wish to show a
//  dialog box Window. The window that is displayed uses this interface to
//  determine how it should be displayed and notify the delegate object of
//  certain events.
//
///////////////////////////////////////////////////////////////////////////////
class DialogDelegate : public WindowDelegate {
 public:
  virtual DialogDelegate* AsDialogDelegate();

  // Returns a mask specifying which of the available DialogButtons are visible
  // for the dialog. Note: If an OK button is provided, you should provide a
  // CANCEL button. A dialog box with just an OK button is frowned upon and
  // considered a very special case, so if you're planning on including one,
  // you should reconsider, or beng says there will be stabbings.
  //
  // To use the extra button you need to override GetDialogButtons()
  virtual int GetDialogButtons() const;

  // Returns whether accelerators are enabled on the button. This is invoked
  // when an accelerator is pressed, not at construction time. This
  // returns true.
  virtual bool AreAcceleratorsEnabled(
      ui::MessageBoxFlags::DialogButton button);

  // Returns the label of the specified DialogButton.
  virtual std::wstring GetDialogButtonLabel(
      ui::MessageBoxFlags::DialogButton button) const;

  // Override this function if with a view which will be shown in the same
  // row as the OK and CANCEL buttons but flush to the left and extending
  // up to the buttons.
  virtual View* GetExtraView();

  // Returns whether the height of the extra view should be at least as tall as
  // the buttons. The default (false) is to give the extra view it's preferred
  // height. By returning true the height becomes
  // max(extra_view preferred height, buttons preferred height).
  virtual bool GetSizeExtraViewHeightToButtons();

  // Returns the default dialog button. This should not be a mask as only
  // one button should ever be the default button.  Return
  // ui::MessageBoxFlags::DIALOGBUTTON_NONE if there is no default.  Default
  // behavior is to return ui::MessageBoxFlags::DIALOGBUTTON_OK or
  // ui::MessageBoxFlags::DIALOGBUTTON_CANCEL (in that order) if they are
  // present, ui::MessageBoxFlags::DIALOGBUTTON_NONE otherwise.
  virtual int GetDefaultDialogButton() const;

  // Returns whether the specified dialog button is enabled.
  virtual bool IsDialogButtonEnabled(
      ui::MessageBoxFlags::DialogButton button) const;

  // Returns whether the specified dialog button is visible.
  virtual bool IsDialogButtonVisible(
      ui::MessageBoxFlags::DialogButton button) const;

  // For Dialog boxes, if there is a "Cancel" button, this is called when the
  // user presses the "Cancel" button or the Close button on the window or
  // in the system menu, or presses the Esc key. This function should return
  // true if the window can be closed after it returns, or false if it must
  // remain open.
  virtual bool Cancel();

  // For Dialog boxes, this is called when the user presses the "OK" button,
  // or the Enter key.  Can also be called on Esc key or close button
  // presses if there is no "Cancel" button.  This function should return
  // true if the window can be closed after the window can be closed after
  // it returns, or false if it must remain open.  If |window_closing| is
  // true, it means that this handler is being called because the window is
  // being closed (e.g.  by Window::Close) and there is no Cancel handler,
  // so Accept is being called instead.
  virtual bool Accept(bool window_closing);
  virtual bool Accept();

  // Overridden from WindowDelegate:
  virtual View* GetInitiallyFocusedView();

  // Overridden from WindowDelegate:
  virtual ClientView* CreateClientView(Window* window);

  // Called when the window has been closed.
  virtual void OnClose() {}

  // A helper for accessing the DialogClientView object contained by this
  // delegate's Window.
  DialogClientView* GetDialogClientView() const;

 protected:
  // Overridden from WindowDelegate:
  virtual ui::AccessibilityTypes::Role GetAccessibleWindowRole() const OVERRIDE;
};

}  // namespace views

#endif  // VIEWS_WINDOW_DIALOG_DELEGATE_H_
