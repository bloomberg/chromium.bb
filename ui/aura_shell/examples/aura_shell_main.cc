// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/examples/toplevel_window.h"
#include "ui/aura_shell/launcher/launcher_types.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace {

class ShellDelegateImpl : public aura_shell::ShellDelegate {
 public:
  ShellDelegateImpl() {
  }

  virtual void CreateNewWindow() OVERRIDE {
    aura_shell::examples::ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    aura_shell::examples::ToplevelWindow::CreateToplevelWindow(create_params);
  }

  virtual void ShowApps() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void LauncherItemClicked(
      const aura_shell::LauncherItem& item) OVERRIDE {
    item.window->Activate();
  }

  virtual bool ConfigureLauncherItem(aura_shell::LauncherItem* item) OVERRIDE {
    return true;  // Makes the entry show up in the launcher.
  }
};

}  // namesapce

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

  // Create the message-loop here before creating the desktop.
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  aura_shell::Shell::GetInstance()->SetDelegate(new ShellDelegateImpl);;

  aura_shell::examples::InitWindowTypeLauncher();

  aura::Desktop::GetInstance()->Run();

  delete aura::Desktop::GetInstance();

  return 0;
}

