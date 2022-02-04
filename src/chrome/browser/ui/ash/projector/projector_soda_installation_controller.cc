// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_soda_installation_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

namespace {

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

inline bool IsLanguageSupported(const speech::LanguageCode languageCode) {
  auto* soda_installer = speech::SodaInstaller::GetInstance();
  for (auto const& language : soda_installer->GetAvailableLanguages()) {
    if (speech::GetLanguageCode(language) == languageCode)
      return true;
  }
  return false;
}

}  // namespace

ProjectorSodaInstallationController::ProjectorSodaInstallationController(
    ash::ProjectorAppClient* client,
    ash::ProjectorController* projector_controller)
    : app_client_(client), projector_controller_(projector_controller) {
  speech::SodaInstaller::GetInstance()->AddObserver(this);

  if (!IsLanguageSupported(speech::GetLanguageCode(GetLocale()))) {
    projector_controller_->OnSpeechRecognitionAvailabilityChanged(
        ash::SpeechRecognitionAvailability::kUserLanguageNotSupported);
    return;
  }

  if (!OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
          GetLocale())) {
    projector_controller_->OnSpeechRecognitionAvailabilityChanged(
        ash::SpeechRecognitionAvailability::kSodaNotInstalled);
    return;
  }

  projector_controller_->OnSpeechRecognitionAvailabilityChanged(
      ash::SpeechRecognitionAvailability::kAvailable);
}

ProjectorSodaInstallationController::~ProjectorSodaInstallationController() {
  auto* installer = speech::SodaInstaller::GetInstance();
  if (installer)
    installer->RemoveObserver(this);
}

void ProjectorSodaInstallationController::InstallSoda(
    const std::string& language) {
  auto languageCode = speech::GetLanguageCode(language);
  auto* soda_installer = speech::SodaInstaller::GetInstance();

  // Initialization will trigger the installation of SODA and the language.
  PrefService* pref_service =
      ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  pref_service->SetString(ash::prefs::kProjectorCreationFlowLanguage, language);
  soda_installer->Init(pref_service, g_browser_process->local_state());

  if (!soda_installer->IsSodaDownloading(languageCode))
    soda_installer->InstallLanguage(language, g_browser_process->local_state());
}

bool ProjectorSodaInstallationController::ShouldDownloadSoda(
    speech::LanguageCode language_code) {
  return base::FeatureList::IsEnabled(
             ash::features::kOnDeviceSpeechRecognition) &&
         IsLanguageSupported(language_code) && !IsSodaAvailable(language_code);
}

bool ProjectorSodaInstallationController::IsSodaAvailable(
    speech::LanguageCode language_code) {
  return speech::SodaInstaller::GetInstance()->IsSodaInstalled(language_code);
}

void ProjectorSodaInstallationController::OnSodaInstalled() {
  auto* soda_installer = speech::SodaInstaller::GetInstance();
  // Make sure that both SODA binary and the locale language are available
  // before notifying that on device speech recognition is available.
  if (!soda_installer->IsSodaInstalled(speech::GetLanguageCode(GetLocale())))
    return;

  projector_controller_->OnSpeechRecognitionAvailabilityChanged(
      ash::SpeechRecognitionAvailability::kAvailable);
  app_client_->OnSodaInstalled();
}

void ProjectorSodaInstallationController::OnSodaError() {
  projector_controller_->OnSpeechRecognitionAvailabilityChanged(
      ash::SpeechRecognitionAvailability::kSodaInstallationError);
  app_client_->OnSodaInstallError();
}

void ProjectorSodaInstallationController::OnSodaProgress(
    int combined_progress) {
  app_client_->OnSodaInstallProgress(combined_progress);
}
