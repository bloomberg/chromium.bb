// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager_observer.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ExtensionEnableFlow;
class PrefChangeRegistrar;
class Profile;

namespace extensions {
class ExtensionService;
}  // namespace extensions

namespace favicon_base {
struct FaviconImageResult;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {
class WebAppProvider;
}  // namespace web_app

// The handler for Javascript messages related to the "apps" view.
class AppLauncherHandler
    : public content::WebUIMessageHandler,
      public extensions::ExtensionUninstallDialog::Delegate,
      public ExtensionEnableFlowDelegate,
      public extensions::InstallObserver,
      public web_app::AppRegistrarObserver,
      public web_app::WebAppPolicyManagerObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  AppLauncherHandler(extensions::ExtensionService* extension_service,
                     web_app::WebAppProvider* web_app_provider);

  AppLauncherHandler(const AppLauncherHandler&) = delete;
  AppLauncherHandler& operator=(const AppLauncherHandler&) = delete;

  ~AppLauncherHandler() override;

  void CreateWebAppInfo(const web_app::AppId& app_id,
                        base::DictionaryValue* value);

  void CreateExtensionInfo(const extensions::Extension* extension,
                           base::DictionaryValue* value);

  // Registers values (strings etc.) for the page.
  static void RegisterLoadTimeData(Profile* profile,
                                   content::WebUIDataSource* source);

  // Register per-profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // extensions::InstallObserver
  void OnAppsReordered(
      const absl::optional<std::string>& extension_id) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppInstallTimeChanged(const web_app::AppId& app_id,
                                  const base::Time& time) override;
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppUninstalled(const web_app::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  // web_app::WebAppPolicyManagerObserver
  void OnPolicyChanged() override;

  // Populate the given dictionary with all installed app info.
  void FillAppDictionary(base::DictionaryValue* value);

  // Create a dictionary value for the given extension.
  std::unique_ptr<base::DictionaryValue> GetExtensionInfo(
      const extensions::Extension* extension);

  // Create a dictionary value for the given web app.
  std::unique_ptr<base::DictionaryValue> GetWebAppInfo(
      const web_app::AppId& app_id);

  // Populate the given dictionary with the web store promo content.
  void FillPromoDictionary(base::DictionaryValue* value);

  // Handles the "launchApp" message with unused |args|.
  void HandleGetApps(const base::ListValue* args);

  // Handles the "launchApp" message with |args| containing [extension_id,
  // source] with optional [url, disposition], |disposition| defaulting to
  // CURRENT_TAB.
  void HandleLaunchApp(const base::ListValue* args);

  // Handles the "setLaunchType" message with args containing [extension_id,
  // launch_type].
  void HandleSetLaunchType(const base::ListValue* args);

  // Handles the "uninstallApp" message with |args| containing [extension_id]
  // and an optional bool to not confirm the uninstall when true, defaults to
  // false.
  void HandleUninstallApp(const base::ListValue* args);

  // Handles the "createAppShortcut" message with |args| containing
  // [extension_id].
  void HandleCreateAppShortcut(const base::ListValue* args);

  // Handles the "installAppLocally" message with |args| containing
  // [extension_id].
  void HandleInstallAppLocally(const base::ListValue* args);

  // Handles the "showAppInfo" message with |args| containing [extension_id].
  void HandleShowAppInfo(const base::ListValue* args);

  // Handles the "reorderApps" message with |args| containing [dragged_app_id,
  // app_order].
  void HandleReorderApps(const base::ListValue* args);

  // Handles the "setPageIndex" message with |args| containing [extension_id,
  // page_index].
  void HandleSetPageIndex(const base::ListValue* args);

  // Handles "saveAppPageName" message with |args| containing [name,
  // page_index].
  void HandleSaveAppPageName(const base::ListValue* args);

  // Handles "generateAppForLink" message with |args| containing [url, title,
  // page_index].
  void HandleGenerateAppForLink(const base::ListValue* args);

  // Handles "pageSelected" message with |args| containing [page_index].
  void HandlePageSelected(const base::ListValue* args);

  // Handles "runOnOsLogin" message with |args| containing [app_id, mode]
  void HandleRunOnOsLogin(const base::ListValue* args);

 private:
  struct AppInstallInfo {
    AppInstallInfo();
    ~AppInstallInfo();

    std::u16string title;
    GURL app_url;
    syncer::StringOrdinal page_ordinal;
  };

  // Reset some instance flags we use to track the currently uninstalling app.
  void CleanupAfterUninstall();

  // Prompts the user to re-enable the app for |extension_id|.
  void PromptToEnableApp(const std::string& extension_id);

  // Records result to UMA after OS Hooks are installed.
  void OnOsHooksInstalled(const web_app::AppId& app_id,
                          const web_app::OsHooksErrors os_hooks_errors);

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override;

  // ExtensionEnableFlowDelegate:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  extensions::ExtensionUninstallDialog* CreateExtensionUninstallDialog();

  // Continuation for installing a bookmark app after favicon lookup.
  void OnFaviconForAppInstallFromLink(
      std::unique_ptr<AppInstallInfo> install_info,
      const favicon_base::FaviconImageResult& image_result);

  void OnExtensionPreferenceChanged();

  // Called when an extension is removed (unloaded or uninstalled). Updates the
  // UI.
  void ExtensionRemoved(const extensions::Extension* extension,
                        bool is_uninstall);

  // True if the extension should be displayed.
  bool ShouldShow(const extensions::Extension* extension);

  // Handle installing OS hooks for Web App installs from chrome://apps page.
  void InstallOsHooks(const web_app::AppId& app_id);

  // The apps are represented in the extensions model, which
  // outlives us since it's owned by our containing profile.
  const raw_ptr<extensions::ExtensionService> extension_service_;

  // The apps are represented in the web apps model, which outlives us since
  // it's owned by our containing profile.
  const raw_ptr<web_app::WebAppProvider> web_app_provider_;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::AppRegistrarObserver>
      web_apps_observation_{this};

  base::ScopedObservation<web_app::WebAppPolicyManager,
                          web_app::WebAppPolicyManagerObserver>
      web_apps_policy_manager_observation_{this};

  base::ScopedObservation<extensions::InstallTracker,
                          extensions::InstallObserver>
      install_tracker_observation_{this};

  // Monitor extension preference changes so that the Web UI can be notified.
  PrefChangeRegistrar extension_pref_change_registrar_;

  // Monitor the local state pref to control the app launcher promo.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // Used to show confirmation UI for uninstalling extensions in incognito mode.
  std::unique_ptr<extensions::ExtensionUninstallDialog>
      extension_uninstall_dialog_;

  // Used to show confirmation UI for enabling extensions.
  std::unique_ptr<ExtensionEnableFlow> extension_enable_flow_;

  // The ids of apps to show on the NTP.
  std::set<std::string> visible_apps_;

  // The ids of apps installed externally.
  std::map<web_app::AppId, GURL> policy_installed_apps_;

  // The id of the extension we are prompting the user about (either enable or
  // uninstall).
  std::string extension_id_prompting_;

  // When true, we ignore changes to the underlying data rather than immediately
  // refreshing. This is useful when making many batch updates to avoid flicker.
  bool ignore_changes_;

  // When populated, we have attempted to install a bookmark app, and are still
  // waiting to hear about success or failure from the extensions system.
  absl::optional<syncer::StringOrdinal>
      attempting_web_app_install_page_ordinal_;

  // True if we have executed HandleGetApps() at least once.
  bool has_loaded_apps_;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Used for passing callbacks.
  base::WeakPtrFactory<AppLauncherHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
