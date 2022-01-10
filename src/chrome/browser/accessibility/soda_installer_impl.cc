// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/soda_installer_impl.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/update_client/crx_update_item.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int GetDownloadProgress(
    const std::map<std::string, update_client::CrxUpdateItem>&
        downloading_components) {
  int total_bytes = 0;
  int downloaded_bytes = 0;

  for (auto component : downloading_components) {
    if (component.second.downloaded_bytes >= 0 &&
        component.second.total_bytes > 0) {
      downloaded_bytes += component.second.downloaded_bytes;
      total_bytes += component.second.total_bytes;
    }
  }

  if (total_bytes == 0)
    return -1;

  DCHECK_LE(downloaded_bytes, total_bytes);
  return 100 * base::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                           0.0, 1.0);
}

}  // namespace

namespace speech {

SodaInstallerImpl::SodaInstallerImpl() = default;

SodaInstallerImpl::~SodaInstallerImpl() {
  component_updater_observation_.Reset();
}

base::FilePath SodaInstallerImpl::GetSodaBinaryPath() const {
  DLOG(FATAL) << "GetSodaBinaryPath not supported on this platform";
  return base::FilePath();
}

base::FilePath SodaInstallerImpl::GetLanguagePath(
    const std::string& language) const {
  DLOG(FATAL) << "GetLanguagePath not supported on this platform";
  return base::FilePath();
}

void SodaInstallerImpl::InstallSoda(PrefService* global_prefs) {
  if (never_download_soda_for_testing_)
    return;
  soda_binary_installed_ = false;
  is_soda_downloading_ = true;
  component_updater::RegisterSodaComponent(
      g_browser_process->component_updater(), global_prefs,
      base::BindOnce(&SodaInstallerImpl::OnSodaBinaryInstalled,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&component_updater::SodaComponentInstallerPolicy::
                         UpdateSodaComponentOnDemand));

  if (!component_updater_observation_.IsObservingSource(
          g_browser_process->component_updater())) {
    component_updater_observation_.Observe(
        g_browser_process->component_updater());
  }
}

void SodaInstallerImpl::InstallLanguage(const std::string& language,
                                        PrefService* global_prefs) {
  if (never_download_soda_for_testing_)
    return;
  speech::LanguageCode locale = speech::GetLanguageCode(language);
  language_pack_progress_.insert({locale, 0.0});
  SodaInstaller::RegisterLanguage(language, global_prefs);
  component_updater::RegisterSodaLanguageComponent(
      g_browser_process->component_updater(), language, global_prefs,
      base::BindOnce(&SodaInstallerImpl::OnSodaLanguagePackInstalled,
                     weak_factory_.GetWeakPtr()));

  if (!component_updater_observation_.IsObservingSource(
          g_browser_process->component_updater())) {
    component_updater_observation_.Observe(
        g_browser_process->component_updater());
  }
}

std::vector<std::string> SodaInstallerImpl::GetAvailableLanguages() const {
  // TODO(crbug.com/1161569): SODA is only available for English right now.
  // Update this to check available languages.
  return {kUsEnglishLocale};
}

void SodaInstallerImpl::UninstallSoda(PrefService* global_prefs) {
  SodaInstaller::UnregisterLanguages(global_prefs);
  base::DeletePathRecursively(speech::GetSodaDirectory());
  base::DeletePathRecursively(speech::GetSodaLanguagePacksDirectory());
  global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());

  soda_binary_installed_ = false;
  is_soda_downloading_ = false;
  soda_installer_initialized_ = false;
  installed_languages_.clear();
  language_pack_progress_.clear();
}

void SodaInstallerImpl::OnEvent(Events event, const std::string& id) {
  if (!component_updater::SodaLanguagePackComponentInstallerPolicy::
           GetExtensionIds()
               .contains(id) &&
      id != component_updater::SodaComponentInstallerPolicy::GetExtensionId()) {
    return;
  }

  LanguageCode language_code = LanguageCode::kNone;
  if (id != component_updater::SodaComponentInstallerPolicy::GetExtensionId()) {
    language_code = GetLanguageCodeByComponentId(id);
    DCHECK_NE(language_code, LanguageCode::kNone);
  }

  switch (event) {
    case Events::COMPONENT_UPDATE_FOUND:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
    case Events::COMPONENT_UPDATE_UPDATING: {
      update_client::CrxUpdateItem item;
      g_browser_process->component_updater()->GetComponentDetails(id, &item);
      downloading_components_[id] = item;
      const int combined_progress =
          GetDownloadProgress(downloading_components_);

      // When GetDownloadProgress returns -1, do nothing. It returns -1 when the
      // downloaded or total bytes is unknown.
      if (combined_progress != -1) {
        NotifyOnSodaProgress(combined_progress);
      }

      if (language_code != LanguageCode::kNone) {
        const int language_progress = GetDownloadProgress(
            std::map<std::string, update_client::CrxUpdateItem>{{id, item}});
        if (language_progress != -1) {
          language_pack_progress_[language_code] = language_progress;
          NotifyOnSodaLanguagePackProgress(language_progress, language_code);
        }
      }

    } break;
    case Events::COMPONENT_UPDATE_ERROR:
      is_soda_downloading_ = false;

      if (language_code != LanguageCode::kNone) {
        language_pack_progress_.erase(language_code);
        NotifyOnSodaLanguagePackError(language_code);
      }

      NotifyOnSodaError();
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_NOT_UPDATED:
      // Do nothing.
      break;
  }
}

void SodaInstallerImpl::OnSodaBinaryInstalled() {
  soda_binary_installed_ = true;
  is_soda_downloading_ = false;
  if (IsAnyLanguagePackInstalled()) {
    NotifyOnSodaInstalled();
  }
}

void SodaInstallerImpl::OnSodaLanguagePackInstalled(
    speech::LanguageCode language_code) {
  installed_languages_.insert(language_code);
  language_pack_progress_.erase(language_code);
  NotifyOnSodaLanguagePackInstalled(language_code);

  if (soda_binary_installed_) {
    NotifyOnSodaInstalled();
  }
}

}  // namespace speech
