// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_MANAGER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "components/keyed_service/core/keyed_service.h"

class ArcAppListPrefs;
class Profile;
class SupervisedUserService;

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace local_search_service {
class LocalSearchService;
}  // namespace local_search_service

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace chromeos {

class CupsPrintersManager;
class KerberosCredentialsManager;

namespace android_sms {
class AndroidSmsService;
}  // namespace android_sms

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace settings {

class OsSettingsSections;
class SearchHandler;
class SearchTagRegistry;

// Manager for the Chrome OS settings page. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the settings app is not opened.
//
// Main responsibilities:
//
// (1) Support search queries for settings content. OsSettingsManager is
//     responsible for updating the kCroSettings index of the LocalSearchService
//     with search tags corresponding to all settings which are available.
//
//     The availability of settings depends on the user's account (e.g.,
//     Personalization settings are not available for guest accounts), the state
//     of the device (e.g., devices without an external monitor hide some
//     display settings), Enterprise settings (e.g., ARC++ is prohibited by some
//     policies), and the state of various flags and switches.
//
//     Whenever settings becomes available or unavailable, OsSettingsManager
//     updates the search index accordingly.
//
// (2) Provide static data to the settings app via the loadTimeData framework.
//     This includes localized strings required by the settings UI as well as
//     flags passed as booleans.
//
// (3) Add logic supporting message-passing between the browser process (C++)
//     and the settings app (JS), via SettingsPageUIHandler objects.
class OsSettingsManager : public KeyedService {
 public:
  OsSettingsManager(
      Profile* profile,
      local_search_service::LocalSearchService* local_search_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      syncer::SyncService* sync_service,
      SupervisedUserService* supervised_user_service,
      KerberosCredentialsManager* kerberos_credentials_manager,
      ArcAppListPrefs* arc_app_list_prefs,
      signin::IdentityManager* identity_manager,
      android_sms::AndroidSmsService* android_sms_service,
      CupsPrintersManager* printers_manager);
  OsSettingsManager(const OsSettingsManager& other) = delete;
  OsSettingsManager& operator=(const OsSettingsManager& other) = delete;
  ~OsSettingsManager() override;

  // Provides static data (i.e., localized strings and flag values) to an OS
  // settings instance. This function causes |html_source| to export a
  // strings.js file which contains a key-value map of the data added by this
  // function.
  void AddLoadTimeData(content::WebUIDataSource* html_source);

  // Adds SettingsPageUIHandlers to an OS settings instance.
  void AddHandlers(content::WebUI* web_ui);

  // Note: Returns null when the kNewOsSettingsSearch flag is disabled.
  SearchHandler* search_handler() { return search_handler_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(OsSettingsManagerTest, Sections);

  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<SearchTagRegistry> search_tag_registry_;
  std::unique_ptr<OsSettingsSections> sections_;
  std::unique_ptr<SearchHandler> search_handler_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_MANAGER_H_
