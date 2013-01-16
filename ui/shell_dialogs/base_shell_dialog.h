// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DIALOGS_BASE_SHELL_DIALOG_H_
#define UI_BASE_DIALOGS_BASE_SHELL_DIALOG_H_

#include "ui/base/ui_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// A base class for shell dialogs.
class UI_EXPORT BaseShellDialog {
 public:
  // Returns true if a shell dialog box is currently being shown modally
  // to the specified owner.
  virtual bool IsRunning(gfx::NativeWindow owning_window) const = 0;

  // Notifies the dialog box that the listener has been destroyed and it should
  // no longer be sent notifications.
  virtual void ListenerDestroyed() = 0;

 protected:
  virtual ~BaseShellDialog();
};

}  // namespace ui

#endif  // UI_BASE_DIALOGS_BASE_SHELL_DIALOG_H_
