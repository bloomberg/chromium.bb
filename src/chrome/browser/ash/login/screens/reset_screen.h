// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_RESET_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_RESET_SCREEN_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/tpm_firmware_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chromeos/dbus/update_engine/update_engine_client.h"

class PrefRegistrySimple;

namespace ash {
class ErrorScreen;
class ScopedGuestButtonBlocker;

// Representation independent class that controls screen showing reset to users.
// It run exit callback only if the user cancels the reset. Other user actions
// will end up in the device restart.
class ResetScreen : public BaseScreen, public UpdateEngineClient::Observer {
 public:
  ResetScreen(ResetView* view,
              ErrorScreen* error_screen,
              const base::RepeatingClosure& exit_callback);

  ResetScreen(const ResetScreen&) = delete;
  ResetScreen& operator=(const ResetScreen&) = delete;

  ~ResetScreen() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(ResetView* view);

  // Registers Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  using TpmFirmwareUpdateAvailabilityCallback = base::OnceCallback<void(
      const std::set<tpm_firmware_update::Mode>& modes)>;
  using TpmFirmwareUpdateAvailabilityChecker = base::RepeatingCallback<void(
      TpmFirmwareUpdateAvailabilityCallback callback,
      base::TimeDelta delay)>;
  // Overrides the method used to determine TPM firmware update availability.
  // It should be called before the ResetScreen is created, otherwise it will
  // have no effect.
  static void SetTpmFirmwareUpdateCheckerForTesting(
      TpmFirmwareUpdateAvailabilityChecker* checker);

  // Checks if powerwash is allowed and passes the result to `callback`. In case
  // TPM firmware update has to be installed, the mode of update will be passed
  // as second parameter to `callback`.
  static void CheckIfPowerwashAllowed(
      base::OnceCallback<void(bool, absl::optional<tpm_firmware_update::Mode>)>
          callback);

 private:
  // BaseScreen implementation:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  void OnRollbackCheck(bool can_rollback);
  void OnTPMFirmwareUpdateAvailableCheck(
      const std::set<tpm_firmware_update::Mode>& modes);

  void OnCancel();
  void OnPowerwash();
  void OnRestart();
  void OnToggleRollback();
  void OnShowConfirm();
  void OnConfirmationDismissed();

  void ShowHelpArticle(HelpAppLauncher::HelpTopic topic);

  ResetView* view_;
  ErrorScreen* error_screen_;
  base::RepeatingClosure exit_callback_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Callback used to check whether a TPM firnware update is available.
  TpmFirmwareUpdateAvailabilityChecker tpm_firmware_update_checker_;

  std::unique_ptr<ScopedGuestButtonBlocker> scoped_guest_button_blocker_;

  base::WeakPtrFactory<ResetScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::ResetScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_RESET_SCREEN_H_
