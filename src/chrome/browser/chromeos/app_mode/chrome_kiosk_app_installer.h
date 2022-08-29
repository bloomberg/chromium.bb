// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_INSTALLER_H_

#include <string>

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"

class Profile;

namespace ash {

class ChromeKioskAppInstaller : private extensions::InstallObserver {
 public:
  using InstallResult = crosapi::mojom::ChromeKioskInstallResult;
  using AppInstallParams = crosapi::mojom::AppInstallParams;
  using InstallCallback =
      crosapi::mojom::ChromeKioskLaunchController::InstallKioskAppCallback;

  ChromeKioskAppInstaller(Profile* profile,
                          const AppInstallParams& install_data);
  ChromeKioskAppInstaller(const ChromeKioskAppInstaller&) = delete;
  ChromeKioskAppInstaller& operator=(const ChromeKioskAppInstaller&) = delete;
  ~ChromeKioskAppInstaller() override;

  void BeginInstall(InstallCallback callback);

 private:
  void MaybeInstallSecondaryApps();
  void MaybeCheckExtensionUpdate();
  void OnExtensionUpdateCheckFinished(bool update_found);
  void FinalizeAppInstall();

  // extensions::InstallObserver overrides.
  void OnFinishCrxInstall(const std::string& extension_id,
                          bool success) override;

  void ReportInstallSuccess();
  void ReportInstallFailure(InstallResult result);
  void ObserveActiveInstallations();

  const extensions::Extension* GetPrimaryAppExtension() const;

  // Returns true if all secondary apps have been installed.
  bool AreSecondaryAppsInstalled() const;

  // Returns true if the app with id |id| is pending an install.
  bool IsAppInstallPending(const std::string& id) const;

  // Returns true if any secondary app is pending.
  bool IsAnySecondaryAppPending() const;

  // Returns true if the primary app has a pending update.
  bool PrimaryAppHasPendingUpdate() const;

  // Returns true if the app with |id| failed, and it is the primary or one of
  // the secondary apps.
  bool DidPrimaryOrSecondaryAppFailedToInstall(bool success,
                                               const std::string& id) const;

  Profile* const profile_;
  AppInstallParams primary_app_install_data_;

  InstallCallback on_ready_callback_;

  bool install_complete_ = false;
  bool secondary_apps_installing_ = false;

  // Used to run extension update checks for primary app's imports and
  // secondary extensions.
  std::unique_ptr<StartupAppLauncherUpdateChecker> update_checker_;

  base::ScopedObservation<extensions::InstallTracker,
                          extensions::InstallObserver>
      install_observation_{this};
  base::WeakPtrFactory<ChromeKioskAppInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_INSTALLER_H_
