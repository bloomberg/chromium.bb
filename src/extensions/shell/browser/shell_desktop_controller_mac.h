// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_MAC_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_MAC_H_

#include <memory>

#include "base/macros.h"
#include "extensions/shell/browser/desktop_controller.h"

namespace extensions {

class AppWindow;
class AppWindowClient;

// A simple implementation of the app_shell DesktopController for Mac Cocoa.
// Only currently supports one app window (unlike Aura).
class ShellDesktopControllerMac : public DesktopController {
 public:
  ShellDesktopControllerMac();
  ~ShellDesktopControllerMac() override;

  // DesktopController:
  void Run() override;
  void AddAppWindow(AppWindow* app_window, gfx::NativeWindow window) override;
  void CloseAppWindows() override;

 private:
  std::unique_ptr<AppWindowClient> app_window_client_;

  // The desktop only supports a single app window.
  // TODO(yoz): Support multiple app windows, as we do in Aura.
  AppWindow* app_window_;  // NativeAppWindow::Close() deletes this.

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerMac);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_MAC_H_
