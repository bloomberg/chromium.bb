// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/help_app_test_helper.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/extensions/signin_screen_policy_provider.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// HelpAppTestHelper::Waiter
///////////////////////////////////////////////////////////////////////////////

HelpAppTestHelper::Waiter::Waiter() {
  aura::Env::GetInstance()->AddObserver(this);
}

HelpAppTestHelper::Waiter::~Waiter() {
  aura::Env::GetInstance()->RemoveObserver(this);
  window_observer_.RemoveAll();
  // Explicitly close any help app dialogs still open to prevent flaky errors in
  // browser tests. Remove when crbug.com/951828 is fixed.
  for (aura::Window* dialog_window : dialog_windows_)
    views::Widget::GetWidgetForNativeView(dialog_window)->CloseNow();
}

void HelpAppTestHelper::Waiter::Wait() {
  if (!help_app_dialog_opened_)
    run_loop_.Run();
}

void HelpAppTestHelper::Waiter::OnWindowInitialized(aura::Window* window) {
  DCHECK(!window_observer_.IsObserving(window));
  window_observer_.Add(window);
}

void HelpAppTestHelper::Waiter::OnWindowDestroyed(aura::Window* window) {
  if (window_observer_.IsObserving(window))
    window_observer_.Remove(window);
  dialog_windows_.erase(window);
}

void HelpAppTestHelper::Waiter::OnWindowVisibilityChanged(aura::Window* window,
                                                          bool visible) {
  if (!IsHelpAppDialog(window))
    return;

  dialog_windows_.insert(window);
  if (visible) {
    help_app_dialog_opened_ = true;
    run_loop_.Quit();
  }
}

bool HelpAppTestHelper::Waiter::IsHelpAppDialog(aura::Window* window) {
  return window->GetTitle() ==
         l10n_util::GetStringUTF16(IDS_LOGIN_OOBE_HELP_DIALOG_TITLE);
}

///////////////////////////////////////////////////////////////////////////////
// HelpAppTestHelper
///////////////////////////////////////////////////////////////////////////////

HelpAppTestHelper::HelpAppTestHelper() {
  auto reset = GetScopedSigninScreenPolicyProviderDisablerForTesting();

  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  extensions::ChromeTestExtensionLoader loader(
      ProfileHelper::GetSigninProfile());
  loader.set_allow_incognito_access(true);

  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_data_dir.AppendASCII("extensions")
                               .AppendASCII("api_test")
                               .AppendASCII("help_app"));

  DCHECK(extension && !extension->id().empty());

  HelpAppLauncher::SetExtensionIdForTest(extension->id().c_str());
}

HelpAppTestHelper::~HelpAppTestHelper() {
  HelpAppLauncher::SetExtensionIdForTest(nullptr);
}

}  // namespace chromeos
