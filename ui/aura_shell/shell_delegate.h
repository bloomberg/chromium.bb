// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_DELEGATE_H_
#define UI_AURA_SHELL_SHELL_DELEGATE_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace views {
class Widget;
}

namespace aura_shell {

struct LauncherItem;

// Delegate of the Shell.
class AURA_SHELL_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Invoked when the user clicks on button in the launcher to create a new
  // window.
  virtual void CreateNewWindow() = 0;

  // Invoked to create a new status area. Can return NULL.
  virtual views::Widget* CreateStatusArea() = 0;

  // Invoked when the user clicks the app list button on the launcher.
  virtual void ShowApps() = 0;

  // Invoked when the user clicks on a window entry in the launcher.
  virtual void LauncherItemClicked(const LauncherItem& item) = 0;

  // Invoked when a window is added. If the delegate wants the launcher to show
  // an entry for |item->window| it should configure |item| appropriately and
  // return true.
  virtual bool ConfigureLauncherItem(LauncherItem* item) = 0;
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_DELEGATE_H_
