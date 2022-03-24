// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_IMPL_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"
#include "components/soda/soda_installer.h"

class PrefService;

namespace update_client {
struct CrxUpdateItem;
}

namespace speech {

// Installer of SODA (Speech On-Device API) for the Live Caption feature on
// non-ChromeOS desktop versions of Chrome browser.
class SodaInstallerImpl : public SodaInstaller,
                          public component_updater::ServiceObserver {
 public:
  SodaInstallerImpl();
  ~SodaInstallerImpl() override;
  SodaInstallerImpl(const SodaInstallerImpl&) = delete;
  SodaInstallerImpl& operator=(const SodaInstallerImpl&) = delete;

  // Currently only implemented in the chromeos-specific subclass.
  base::FilePath GetSodaBinaryPath() const override;

  // Currently only implemented in the chromeos-specific subclass.
  base::FilePath GetLanguagePath(const std::string& language) const override;

  // SodaInstaller:
  void InstallLanguage(const std::string& language,
                       PrefService* global_prefs) override;
  std::vector<std::string> GetAvailableLanguages() const override;

 protected:
  // SodaInstaller:
  void InstallSoda(PrefService* global_prefs) override;
  void UninstallSoda(PrefService* global_prefs) override;

  // component_updater::ServiceObserver:
  void OnEvent(Events event, const std::string& id) override;

  void OnSodaBinaryInstalled();
  void OnSodaLanguagePackInstalled(speech::LanguageCode language_code);

 private:
  std::map<std::string, update_client::CrxUpdateItem> downloading_components_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};

  base::WeakPtrFactory<SodaInstallerImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_IMPL_H_
