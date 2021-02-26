// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_

#include "chrome/browser/accessibility/soda_installer.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

class PrefService;

namespace speech {

// Installer of SODA (Speech On-Device API) for the Live Caption feature on
// ChromeOS.
class SODAInstallerImplChromeOS : public SODAInstaller {
 public:
  SODAInstallerImplChromeOS();
  ~SODAInstallerImplChromeOS();
  SODAInstallerImplChromeOS(const SODAInstallerImplChromeOS&) = delete;
  SODAInstallerImplChromeOS& operator=(const SODAInstallerImplChromeOS&) =
      delete;

  // SODAInstaller:
  void InstallSODA(PrefService* prefs) override;
  void InstallLanguage(PrefService* prefs) override;

 private:
  // This function is the InstallCallback for DlcserviceClient::Install().
  void OnSODAInstalled(
      const chromeos::DlcserviceClient::InstallResult& install_result);

  // This function is the ProgressCallback for DlcserviceClient::Install().
  void OnSODAProgress(double progress);
};

}  // namespace speech

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_
