// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_javascript_dialog.h"

#include <utility>

#include "base/strings/string_util.h"
#include "content/shell/app/resource.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"

namespace content {

class ShellJavaScriptDialog;

INT_PTR CALLBACK ShellJavaScriptDialog::DialogProc(HWND dialog,
                                                   UINT message,
                                                   WPARAM wparam,
                                                   LPARAM lparam) {
  switch (message) {
    case WM_INITDIALOG: {
      SetWindowLongPtr(dialog, DWLP_USER, static_cast<LONG_PTR>(lparam));
      ShellJavaScriptDialog* owner =
          reinterpret_cast<ShellJavaScriptDialog*>(lparam);
      owner->dialog_win_ = dialog;
      SetDlgItemText(dialog, IDC_DIALOGTEXT, owner->message_text_.c_str());
      if (owner->dialog_type_ == JAVASCRIPT_DIALOG_TYPE_PROMPT)
        SetDlgItemText(dialog, IDC_PROMPTEDIT,
                       owner->default_prompt_text_.c_str());
      break;
    }
    case WM_DESTROY: {
      ShellJavaScriptDialog* owner = reinterpret_cast<ShellJavaScriptDialog*>(
          GetWindowLongPtr(dialog, DWLP_USER));
      if (owner->dialog_win_) {
        owner->dialog_win_ = 0;
        std::move(owner->callback_).Run(false, base::string16());
        owner->manager_->DialogClosed(owner);
      }
      break;
    }
    case WM_COMMAND: {
      ShellJavaScriptDialog* owner = reinterpret_cast<ShellJavaScriptDialog*>(
          GetWindowLongPtr(dialog, DWLP_USER));
      base::string16 user_input;
      bool finish = false;
      bool result = false;
      switch (LOWORD(wparam)) {
        case IDOK:
          finish = true;
          result = true;
          if (owner->dialog_type_ == JAVASCRIPT_DIALOG_TYPE_PROMPT) {
            int length =
                GetWindowTextLength(GetDlgItem(dialog, IDC_PROMPTEDIT)) + 1;
            GetDlgItemText(dialog, IDC_PROMPTEDIT,
                           base::WriteInto(&user_input, length), length);
          }
          break;
        case IDCANCEL:
          finish = true;
          result = false;
          break;
      }
      if (finish) {
        owner->dialog_win_ = 0;
        std::move(owner->callback_).Run(result, user_input);
        DestroyWindow(dialog);
        owner->manager_->DialogClosed(owner);
      }
      break;
    }
    default:
      return DefWindowProc(dialog, message, wparam, lparam);
  }
  return 0;
}

ShellJavaScriptDialog::ShellJavaScriptDialog(
    ShellJavaScriptDialogManager* manager,
    gfx::NativeWindow parent_window,
    JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    JavaScriptDialogManager::DialogClosedCallback callback)
    : callback_(std::move(callback)),
      manager_(manager),
      dialog_type_(dialog_type),
      message_text_(message_text),
      default_prompt_text_(default_prompt_text) {
  int dialog_resource;
  if (dialog_type == JAVASCRIPT_DIALOG_TYPE_ALERT)
    dialog_resource = IDD_ALERT;
  else if (dialog_type == JAVASCRIPT_DIALOG_TYPE_CONFIRM)
    dialog_resource = IDD_CONFIRM;
  else  // JAVASCRIPT_DIALOG_TYPE_PROMPT
    dialog_resource = IDD_PROMPT;

  dialog_win_ =
      CreateDialogParam(GetModuleHandle(0), MAKEINTRESOURCE(dialog_resource), 0,
                        DialogProc, reinterpret_cast<LPARAM>(this));
  ShowWindow(dialog_win_, SW_SHOWNORMAL);
}

ShellJavaScriptDialog::~ShellJavaScriptDialog() {
  Cancel();
}

void ShellJavaScriptDialog::Cancel() {
  if (dialog_win_)
    DestroyWindow(dialog_win_);
  dialog_win_ = 0;
  if (callback_)
    std::move(callback_).Run(false, base::string16());
}

}  // namespace content
