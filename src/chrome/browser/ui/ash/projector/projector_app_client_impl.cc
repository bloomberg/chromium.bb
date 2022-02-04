// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/soda/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace {

constexpr char kUsEnglishLocale[] = "en-US";

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

inline const speech::LanguageCode GetLocaleLanguageCode() {
  return speech::GetLanguageCode(GetLocale());
}

}  // namespace

// static
void ProjectorAppClientImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      ash::prefs::kProjectorCreationFlowEnabled, /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(
      ash::prefs::kProjectorCreationFlowLanguage,
      /*default_value=*/kUsEnglishLocale,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      ash::prefs::kProjectorGalleryOnboardingShowCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      ash::prefs::kProjectorViewerOnboardingShowCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(ash::prefs::kProjectorAllowByPolicy,
                                /*default_value=*/false);
}

ProjectorAppClientImpl::ProjectorAppClientImpl()
    : pending_screencast_manager_(base::BindRepeating(
          &ProjectorAppClientImpl::NotifyScreencastsPendingStatusChanged,
          base::Unretained(this))) {
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    ash::ProjectorController::Get()->OnSpeechRecognitionAvailabilityChanged(
        ash::SpeechRecognitionAvailability::
            kOnDeviceSpeechRecognitionNotSupported);
    return;
  }

  soda_installation_controller_ =
      std::make_unique<ProjectorSodaInstallationController>(
          this, ash::ProjectorController::Get());
}

ProjectorAppClientImpl::~ProjectorAppClientImpl() = default;

void ProjectorAppClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProjectorAppClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

signin::IdentityManager* ProjectorAppClientImpl::GetIdentityManager() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return IdentityManagerFactory::GetForProfile(profile);
}

network::mojom::URLLoaderFactory*
ProjectorAppClientImpl::GetUrlLoaderFactory() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

void ProjectorAppClientImpl::OnNewScreencastPreconditionChanged(
    const ash::NewScreencastPrecondition& precondition) {
  for (auto& observer : observers_)
    observer.OnNewScreencastPreconditionChanged(precondition);
}

const ash::PendingScreencastSet& ProjectorAppClientImpl::GetPendingScreencasts()
    const {
  return pending_screencast_manager_.GetPendingScreencasts();
}

void ProjectorAppClientImpl::NotifyScreencastsPendingStatusChanged(
    const ash::PendingScreencastSet& pending_screencast) {
  for (auto& observer : observers_)
    observer.OnScreencastsPendingStatusChanged(pending_screencast);
}

bool ProjectorAppClientImpl::ShouldDownloadSoda() {
  return soda_installation_controller_ &&
         soda_installation_controller_->ShouldDownloadSoda(
             GetLocaleLanguageCode());
}

void ProjectorAppClientImpl::InstallSoda() {
  DCHECK(soda_installation_controller_);

  soda_installation_controller_->InstallSoda(GetLocale());
}

void ProjectorAppClientImpl::OnSodaInstallProgress(int combined_progress) {
  for (auto& observer : observers_)
    observer.OnSodaProgress(combined_progress);
}

void ProjectorAppClientImpl::OnSodaInstallError() {
  for (auto& observer : observers_)
    observer.OnSodaError();
}

void ProjectorAppClientImpl::OnSodaInstalled() {
  for (auto& observer : observers_)
    observer.OnSodaInstalled();
}

bool ProjectorAppClientImpl::IsSpeechRecognitionAvailable() {
  return soda_installation_controller_ &&
         soda_installation_controller_->IsSodaAvailable(
             GetLocaleLanguageCode());
}

void ProjectorAppClientImpl::OpenFeedbackDialog() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  constexpr char kProjectorAppFeedbackCategoryTag[] = "FromProjectorApp";
  chrome::ShowFeedbackPage(GURL(ash::kChromeUITrustedProjectorUrl), profile,
                           chrome::kFeedbackSourceProjectorApp,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/std::string(),
                           kProjectorAppFeedbackCategoryTag,
                           /*extra_diagnostics=*/std::string());
  // TODO(crbug/1048368): Communicate the dialog failing to open by returning an
  // error string. For now, assume that the dialog has opened successfully.
}
