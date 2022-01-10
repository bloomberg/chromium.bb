// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_SODA_INSTALLATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_SODA_INSTALLATION_CONTROLLER_H_

#include <string>

#include "components/soda/soda_installer.h"

namespace ash {
class ProjectorAppClient;
class ProjectorController;
}  // namespace ash

namespace speech {
enum class LanguageCode;
}  // namespace speech

// Class owned by ProjectorAppClientImpl used to control the installation of
// SODA and the language pack requested by the user.
class ProjectorSodaInstallationController
    : public speech::SodaInstaller::Observer {
 public:
  ProjectorSodaInstallationController(ash::ProjectorAppClient* app_client,
                                      ash::ProjectorController* controller);
  ProjectorSodaInstallationController(
      const ProjectorSodaInstallationController&) = delete;
  ProjectorSodaInstallationController& operator=(
      const ProjectorSodaInstallationController&) = delete;

  ~ProjectorSodaInstallationController() override;

  // Installs the SODA binary and the the corresponding language if it is not
  // present.
  void InstallSoda(const std::string& language);

  // Checks if the device is eligible to install SODA and language pack for the
  // `language` provided.
  bool ShouldDownloadSoda(speech::LanguageCode language);

  // Checks if SODA binary and the requested `language` is downloaded and
  // available on device.
  bool IsSodaAvailable(speech::LanguageCode language);

 protected:
  // speech::SodaInstaller::Observer:
  void OnSodaInstalled() override;
  void OnSodaLanguagePackInstalled(
      speech::LanguageCode language_code) override {}
  void OnSodaError() override;
  void OnSodaLanguagePackError(speech::LanguageCode language_code) override {}
  void OnSodaProgress(int combined_progress) override;
  void OnSodaLanguagePackProgress(int language_progress,
                                  speech::LanguageCode language_code) override {
  }

  ash::ProjectorAppClient* const app_client_;
  ash::ProjectorController* const projector_controller_;
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_SODA_INSTALLATION_CONTROLLER_H_
