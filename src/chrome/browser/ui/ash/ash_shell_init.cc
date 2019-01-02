// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_shell_init.h"

#include <utility>

#include "ash/content/content_gpu_interface_provider.h"
#include "ash/display/display_prefs.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/service_manager_connection.h"
#include "ui/aura/window_tree_host.h"

namespace {

void CreateClassicShell() {
  ash::ShellInitParams shell_init_params;
  shell_init_params.delegate = std::make_unique<ChromeShellDelegate>();
  shell_init_params.context_factory = content::GetContextFactory();
  shell_init_params.context_factory_private =
      content::GetContextFactoryPrivate();
  shell_init_params.gpu_interface_provider =
      std::make_unique<ash::ContentGpuInterfaceProvider>();
  shell_init_params.connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  DCHECK(shell_init_params.connector);
  // Pass the initial display prefs to ash::Shell as a Value dictionary.
  // This is done this way to avoid complexities with registering the display
  // prefs in multiple places (i.e. in g_browser_process->local_state() and
  // ash::Shell::local_state_). See https://crbug.com/834775 for details.
  shell_init_params.initial_display_prefs =
      ash::DisplayPrefs::GetInitialDisplayPrefsFromPrefService(
          g_browser_process->local_state());

  ash::Shell::CreateInstance(std::move(shell_init_params));
}

}  // namespace

// static
void AshShellInit::RegisterDisplayPrefs(PrefRegistrySimple* registry) {
  // Note: For CLASSIC/MUS, DisplayPrefs must be registered here so that
  // the initial display prefs can be passed synchronously to ash::Shell.
  ash::DisplayPrefs::RegisterLocalStatePrefs(registry);
}

AshShellInit::AshShellInit() {
  CreateClassicShell();
  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

AshShellInit::~AshShellInit() {
  ash::Shell::DeleteInstance();
}
