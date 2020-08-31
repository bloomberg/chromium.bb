// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {

OsSettingsManager::OsSettingsManager(
    Profile* profile,
    local_search_service::LocalSearchService* local_search_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    syncer::SyncService* sync_service,
    SupervisedUserService* supervised_user_service,
    KerberosCredentialsManager* kerberos_credentials_manager,
    ArcAppListPrefs* arc_app_list_prefs,
    signin::IdentityManager* identity_manager,
    android_sms::AndroidSmsService* android_sms_service,
    CupsPrintersManager* printers_manager)
    : search_tag_registry_(
          std::make_unique<SearchTagRegistry>(local_search_service)),
      sections_(
          std::make_unique<OsSettingsSections>(profile,
                                               search_tag_registry_.get(),
                                               multidevice_setup_client,
                                               sync_service,
                                               supervised_user_service,
                                               kerberos_credentials_manager,
                                               arc_app_list_prefs,
                                               identity_manager,
                                               android_sms_service,
                                               printers_manager)) {
  if (base::FeatureList::IsEnabled(features::kNewOsSettingsSearch)) {
    search_handler_ = std::make_unique<SearchHandler>(
        search_tag_registry_.get(), local_search_service);
  }
}

OsSettingsManager::~OsSettingsManager() = default;

void OsSettingsManager::AddLoadTimeData(content::WebUIDataSource* html_source) {
  for (const auto& section : sections_->sections())
    section->AddLoadTimeData(html_source);
  html_source->UseStringsJs();
}

void OsSettingsManager::AddHandlers(content::WebUI* web_ui) {
  for (const auto& section : sections_->sections())
    section->AddHandlers(web_ui);
}

void OsSettingsManager::Shutdown() {
  // Note: These must be deleted in the opposite order of their creation to
  // prevent against UAF violations.
  search_handler_.reset();
  sections_.reset();
  search_tag_registry_.reset();
}

}  // namespace settings
}  // namespace chromeos
