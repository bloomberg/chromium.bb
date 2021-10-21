// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/tpm_firmware_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace settings {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Triggers a TPM firmware update using the least destructive mode from
// |available_modes|.
void TriggerTPMFirmwareUpdate(
    const std::set<chromeos::tpm_firmware_update::Mode>& available_modes) {
  using chromeos::tpm_firmware_update::Mode;

  // Decide which update mode to use.
  for (Mode mode :
       {Mode::kPreserveDeviceState, Mode::kPowerwash, Mode::kCleanup}) {
    if (available_modes.count(mode) == 0) {
      continue;
    }

    // Save a TPM firmware update request in local state, which
    // will trigger the reset screen to appear on reboot.
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
    prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                      static_cast<int>(mode));
    prefs->CommitPendingWrite();
    chrome::AttemptRelaunch();
    return;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

BrowserLifetimeHandler::BrowserLifetimeHandler() {}

BrowserLifetimeHandler::~BrowserLifetimeHandler() {}

void BrowserLifetimeHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "restart", base::BindRepeating(&BrowserLifetimeHandler::HandleRestart,
                                     base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "relaunch", base::BindRepeating(&BrowserLifetimeHandler::HandleRelaunch,
                                      base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterDeprecatedMessageCallback(
      "signOutAndRestart",
      base::BindRepeating(&BrowserLifetimeHandler::HandleSignOutAndRestart,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "factoryReset",
      base::BindRepeating(&BrowserLifetimeHandler::HandleFactoryReset,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void BrowserLifetimeHandler::HandleRestart(
    const base::ListValue* args) {
  chrome::AttemptRestart();
}

void BrowserLifetimeHandler::HandleRelaunch(
    const base::ListValue* args) {
  chrome::AttemptRelaunch();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BrowserLifetimeHandler::HandleSignOutAndRestart(
    const base::ListValue* args) {
  chrome::AttemptUserExit();
}

void BrowserLifetimeHandler::HandleFactoryReset(
    const base::ListValue* args) {
  base::Value::ConstListView args_list = args->GetList();
  CHECK_EQ(1U, args_list.size());
  bool tpm_firmware_update_requested = args_list[0].GetBool();

  if (tpm_firmware_update_requested) {
    chromeos::tpm_firmware_update::GetAvailableUpdateModes(
        base::BindOnce(&TriggerTPMFirmwareUpdate), base::TimeDelta());
    return;
  }

  // TODO(crbug.com/891905): Centralize powerwash restriction checks.
  bool allow_powerwash =
      !webui::IsEnterpriseManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      !user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  if (!allow_powerwash)
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chrome::AttemptRelaunch();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace settings
