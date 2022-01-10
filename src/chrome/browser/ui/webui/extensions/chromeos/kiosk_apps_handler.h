// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_CHROMEOS_KIOSK_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_CHROMEOS_KIOSK_APPS_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
// TODO(https://crbug.com/1164001): move OwnerSettingsServiceAsh to forward
// declaration when moved to chrome/browser/ash/.
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace chromeos {

class KioskAppsHandler : public content::WebUIMessageHandler,
                         public KioskAppManagerObserver {
 public:
  explicit KioskAppsHandler(OwnerSettingsServiceAsh* service);

  KioskAppsHandler(const KioskAppsHandler&) = delete;
  KioskAppsHandler& operator=(const KioskAppsHandler&) = delete;

  ~KioskAppsHandler() override;

  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // KioskAppManagerObserver overrides:
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  void OnKioskAppsSettingsChanged() override;

 private:
  // Get all kiosk apps and settings.
  std::unique_ptr<base::DictionaryValue> GetSettingsDictionary();

  // JS callbacks.
  void HandleInitializeKioskAppSettings(const base::ListValue* args);
  void HandleGetKioskAppSettings(const base::ListValue* args);
  void HandleAddKioskApp(const base::ListValue* args);
  void HandleRemoveKioskApp(const base::ListValue* args);
  void HandleEnableKioskAutoLaunch(const base::ListValue* args);
  void HandleDisableKioskAutoLaunch(const base::ListValue* args);
  void HandleSetDisableBailoutShortcut(const base::ListValue* args);

  void UpdateApp(const std::string& app_id);
  void ShowError(const std::string& app_id);

  // Callback for KioskAppManager::GetConsumerKioskModeStatus().
  void OnGetConsumerKioskAutoLaunchStatus(
      const std::string& callback_id,
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  KioskAppManager* kiosk_app_manager_;  // not owned.
  bool initialized_;
  bool is_kiosk_enabled_;
  bool is_auto_launch_enabled_;
  OwnerSettingsServiceAsh* const owner_settings_service_;  // not owned
  base::WeakPtrFactory<KioskAppsHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_CHROMEOS_KIOSK_APPS_HANDLER_H_
