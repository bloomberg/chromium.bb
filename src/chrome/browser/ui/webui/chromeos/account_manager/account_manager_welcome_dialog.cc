// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/account_manager/account_manager_welcome_dialog.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

AccountManagerWelcomeDialog* g_dialog = nullptr;
constexpr int kSigninDialogWidth = 768;
constexpr int kSigninDialogHeight = 640;

}  // namespace

AccountManagerWelcomeDialog::AccountManagerWelcomeDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAccountManagerWelcomeURL),
                              std::u16string() /* title */) {}

AccountManagerWelcomeDialog::~AccountManagerWelcomeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

// static
bool AccountManagerWelcomeDialog::ShowIfRequired() {
  // TODO(crbug.com/1259613): Remove the
  // `prefs::kAccountManagerNumTimesWelcomeScreenShown` pref. Remove the dialog
  // if `ShowIfRequiredForEduCoexistence` is also not needed.
  return false;
}

// static
bool AccountManagerWelcomeDialog::ShowIfRequiredForEduCoexistence() {
  return ShowIfRequiredInternal();
}

void AccountManagerWelcomeDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

void AccountManagerWelcomeDialog::OnDialogClosed(
    const std::string& json_retval) {
  // Opening Settings during shutdown or crash/restart leads to a crash.
  if (!chrome::IsAttemptingShutdown() && g_browser_process &&
      !g_browser_process->IsShuttingDown()) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        ProfileManager::GetActiveUserProfile(),
        chromeos::settings::mojom::kMyAccountsSubpagePath);
  }

  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

void AccountManagerWelcomeDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kSigninDialogWidth, kSigninDialogHeight);
}

std::string AccountManagerWelcomeDialog::GetDialogArgs() const {
  return std::string();
}

bool AccountManagerWelcomeDialog::ShouldShowDialogTitle() const {
  return false;
}

bool AccountManagerWelcomeDialog::ShouldShowCloseButton() const {
  return false;
}

// static
bool AccountManagerWelcomeDialog::ShowIfRequiredInternal() {
  if (g_dialog) {
    // If the dialog is already being displayed, bring it to focus instead of
    // creating another window.
    g_dialog->dialog_window()->Focus();
    return true;
  }

  // Check if the dialog should be shown.
  // It should not be shown in kiosk mode since there are no actual accounts to
  // manage, but the service account.
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral() ||
      user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  // Will be deleted by |SystemWebDialogDelegate::OnDialogClosed|.
  g_dialog = new AccountManagerWelcomeDialog();
  g_dialog->ShowSystemDialog();
  return true;
}

}  // namespace chromeos
