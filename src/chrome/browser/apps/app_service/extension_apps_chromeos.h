// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_CHROMEOS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_CHROMEOS_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/extension_apps_base.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_registry.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace extensions {
class AppWindow;
}  // namespace extensions

namespace message_center {
class Notification;
}

class NotificationDisplayService;

namespace apps {

// An app publisher (in the App Service sense) of extension-backed apps for
// ChromeOS, including Chrome Apps (platform apps and legacy packaged apps) and
// hosted apps (including desktop PWAs).
//
// In the future, desktop PWAs will be migrated to a new system.
//
// See chrome/services/app_service/README.md.
class ExtensionAppsChromeOs : public ExtensionAppsBase,
                              public extensions::AppWindowRegistry::Observer,
                              public ArcAppListPrefs::Observer,
                              public NotificationDisplayService::Observer {
 public:
  ExtensionAppsChromeOs(
      const mojo::Remote<apps::mojom::AppService>& app_service,
      Profile* profile,
      apps::mojom::AppType app_type,
      apps::InstanceRegistry* instance_registry);
  ~ExtensionAppsChromeOs() override;

  ExtensionAppsChromeOs(const ExtensionAppsChromeOs&) = delete;
  ExtensionAppsChromeOs& operator=(const ExtensionAppsChromeOs&) = delete;

  // Record uninstall dialog action for Web apps and Chrome apps.
  static void RecordUninstallCanceledAction(Profile* profile,
                                            const std::string& app_id);

  void Shutdown();

  void ObserveArc();

 private:
  void Initialize();

  // apps::mojom::Publisher overrides.
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void Uninstall(const std::string& app_id,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApps(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;

  // Overridden from AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;
  void OnAppWindowHidden(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // ArcAppListPrefs::Observer overrides.
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;
  void OnArcAppListPrefsDestroyed() override;

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override;
  void OnNotificationClosed(const std::string& notification_id) override;
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override;

  void MaybeAddNotification(const std::string& app_id,
                            const std::string& notification_id);
  void MaybeAddWebPageNotifications(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata);

  static bool IsBlacklisted(const std::string& app_id);

  void UpdateShowInFields(const std::string& app_id);

  // ExtensionAppsBase overrides.
  void OnHideWebStoreIconPrefChanged() override;
  void OnSystemFeaturesPrefChanged() override;
  bool Accepts(const extensions::Extension* extension) override;
  bool ShouldShownInLauncher(const extensions::Extension* extension) override;
  apps::mojom::AppPtr Convert(const extensions::Extension* extension,
                              apps::mojom::Readiness readiness) override;

  // Calculate the icon effects for the extension.
  IconEffects GetIconEffects(const extensions::Extension* extension,
                             bool paused);

  // Get the equivalent Chrome app from |arc_package_name| and set the Chrome
  // app badge on the icon effects for the equivalent Chrome apps. If the
  // equivalent ARC app is installed, add the Chrome app badge, otherwise,
  // remove the Chrome app badge.
  void ApplyChromeBadge(const std::string& arc_package_name);

  void SetIconEffect(const std::string& app_id);

  bool ShouldRecordAppWindowActivity(extensions::AppWindow* app_window);
  void RegisterInstance(extensions::AppWindow* app_window, InstanceState state);

  void GetMenuModelForChromeBrowserApp(apps::mojom::MenuType menu_type,
                                       GetMenuModelCallback callback);

  // web_app::AppRegistrarObserver overrides.
  void OnWebAppDisabledStateChanged(const std::string& app_id,
                                    bool is_disabled) override;

  apps::InstanceRegistry* instance_registry_;
  ScopedObserver<extensions::AppWindowRegistry,
                 extensions::AppWindowRegistry::Observer>
      app_window_registry_{this};

  PausedApps paused_apps_;

  std::set<std::string> disabled_apps_;

  std::map<extensions::AppWindow*, aura::Window*> app_window_to_aura_window_;

  ArcAppListPrefs* arc_prefs_ = nullptr;

  // Registrar used to monitor the profile prefs.
  PrefChangeRegistrar profile_pref_change_registrar_;

  // Registrar used to monitor the local state prefs.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  ScopedObserver<NotificationDisplayService,
                 NotificationDisplayService::Observer>
      notification_display_service_{this};

  AppNotifications app_notifications_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_CHROMEOS_H_
