// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_LINUX_UI_SHELL_DIALOG_H_
#define UI_SHELL_DIALOGS_LINUX_UI_SHELL_DIALOG_H_

#include "ui/base/linux_ui.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {
// This subclass of the base LinuxUI class provides a way for subclasses to
// override the CreateSelectFileDialog function declared here to return
// native file dialogs.
class SHELL_DIALOGS_EXPORT LinuxUIShellDialog : public ui::LinuxUI {
 public:
  virtual ~LinuxUIShellDialog() {}

  // Returns a native file selection dialog.
  virtual SelectFileDialog* CreateSelectFileDialog(
      SelectFileDialog::Listener* listener,
      SelectFilePolicy* policy) const = 0;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_LINUX_UI_SHELL_DIALOG_H_

