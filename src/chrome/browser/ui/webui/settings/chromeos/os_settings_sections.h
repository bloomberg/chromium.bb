// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_SECTIONS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_SECTIONS_H_

#include <unordered_map>
#include <vector>

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

class ArcAppListPrefs;
class Profile;
class SupervisedUserService;

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

// Collection of all OsSettingsSection implementations.
class OsSettingsSections {
 public:
  OsSettingsSections(
      Profile* profile,
      SearchTagRegistry* search_tag_registry,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      syncer::SyncService* sync_service,
      SupervisedUserService* supervised_user_service,
      KerberosCredentialsManager* kerberos_credentials_manager,
      ArcAppListPrefs* arc_app_list_prefs,
      signin::IdentityManager* identity_manager,
      android_sms::AndroidSmsService* android_sms_service,
      CupsPrintersManager* printers_manager);
  OsSettingsSections(const OsSettingsSections& other) = delete;
  OsSettingsSections& operator=(const OsSettingsSections& other) = delete;
  ~OsSettingsSections();

  OsSettingsSection* GetSection(mojom::Section section);

  std::vector<std::unique_ptr<OsSettingsSection>>& sections() {
    return sections_;
  }

 private:
  std::unordered_map<mojom::Section, OsSettingsSection*> sections_map_;
  std::vector<std::unique_ptr<OsSettingsSection>> sections_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_SECTIONS_H_
