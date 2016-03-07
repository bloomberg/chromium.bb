// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/shell_dialog_linux.h"

namespace {

ui::ShellDialogLinux* g_shell_dialog_linux = nullptr;

}  // namespace

namespace ui {

void ShellDialogLinux::SetInstance(ShellDialogLinux* instance) {
  g_shell_dialog_linux = instance;
}

const ShellDialogLinux* ShellDialogLinux::instance() {
  return g_shell_dialog_linux;
}

}  // namespace ui
