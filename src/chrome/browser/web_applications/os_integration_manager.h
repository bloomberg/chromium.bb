// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MANAGER_H_

#include <bitset>
#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/web_applications/url_handler_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppRegistrar;
class WebAppIconManager;
class FakeOsIntegrationManager;
class WebAppUiManager;

// OsHooksErrors contains the result of all Os hook deployments.
// If a bit is set to `true`, then an error did occur.
using OsHooksErrors = std::bitset<OsHookType::kMaxValue + 1>;

// OsHooksOptions contains the (install/uninstall) options of all Os hook
// deployments.
using OsHooksOptions = std::bitset<OsHookType::kMaxValue + 1>;

using content::ProtocolHandler;

// Used to pass install options configured from upstream caller.
// All options are disabled by default.
struct InstallOsHooksOptions {
  InstallOsHooksOptions();
  InstallOsHooksOptions(const InstallOsHooksOptions& other);
  InstallOsHooksOptions& operator=(const InstallOsHooksOptions& other);

  OsHooksOptions os_hooks;
  bool add_to_desktop = false;
  bool add_to_quick_launch_bar = false;
};

// Callback made after InstallOsHooks is finished.
using InstallOsHooksCallback =
    base::OnceCallback<void(OsHooksErrors os_hooks_errors)>;

// Callback made after UninstallOsHooks is finished.
using UninstallOsHooksCallback =
    base::OnceCallback<void(OsHooksErrors os_hooks_errors)>;

// Used to suppress OS hooks within this object's lifetime.
using ScopedOsHooksSuppress = std::unique_ptr<base::AutoReset<bool>>;

using BarrierCallback =
    base::RepeatingCallback<void(OsHookType::Type os_hook, bool completed)>;

// OsIntegrationManager is responsible of creating/updating/deleting
// all OS hooks during Web App lifecycle.
// It contains individual OS integration managers and takes
// care of inter-dependencies among them.
class OsIntegrationManager {
 public:
  explicit OsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppShortcutManager> shortcut_manager,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
      std::unique_ptr<UrlHandlerManager> url_handler_manager);
  virtual ~OsIntegrationManager();

  void SetSubsystems(WebAppRegistrar* registrar,
                     WebAppUiManager* ui_manager,
                     WebAppIconManager* icon_manager);

  void Start();

  // Install all needed OS hooks for the web app.
  // If provided |web_app_info| is a nullptr, it will read icons data from disk,
  // otherwise it will use (SkBitmaps) from |web_app_info|.
  // virtual for testing
  virtual void InstallOsHooks(const AppId& app_id,
                              InstallOsHooksCallback callback,
                              std::unique_ptr<WebApplicationInfo> web_app_info,
                              InstallOsHooksOptions options);

  // Uninstall specific OS hooks for the web app.
  // Used when removing specific hooks resulting from an app setting change.
  // Example: Running on OS login.
  // TODO(https://crbug.com/1108109) we should record uninstall result and allow
  // callback. virtual for testing
  virtual void UninstallOsHooks(const AppId& app_id,
                                const OsHooksOptions& os_hooks,
                                UninstallOsHooksCallback callback);

  // Uninstall all OS hooks for the web app.
  // Used when uninstalling a web app.
  // virtual for testing
  virtual void UninstallAllOsHooks(const AppId& app_id,
                                   UninstallOsHooksCallback callback);

  // Update all needed OS hooks for the web app.
  // virtual for testing
  virtual void UpdateOsHooks(
      const AppId& app_id,
      base::StringPiece old_name,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebApplicationInfo& web_app_info);

  // Proxy calls for WebAppShortcutManager.
  // virtual for testing
  virtual void GetAppExistingShortCutLocation(
      ShortcutLocationCallback callback,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  // Proxy calls for WebAppShortcutManager.
  void GetShortcutInfoForApp(
      const AppId& app_id,
      WebAppShortcutManager::GetShortcutInfoCallback callback);

  // Proxy calls for WebAppFileHandlerManager.
  bool IsFileHandlingAPIAvailable(const AppId& app_id);
  const apps::FileHandlers* GetEnabledFileHandlers(const AppId& app_id);
  const absl::optional<GURL> GetMatchingFileHandlerURL(
      const AppId& app_id,
      const std::vector<base::FilePath>& launch_files);
  void MaybeUpdateFileHandlingOriginTrialExpiry(
      content::WebContents* web_contents,
      const AppId& app_id);
  void ForceEnableFileHandlingOriginTrial(const AppId& app_id);
  void DisableForceEnabledFileHandlingOriginTrial(const AppId& app_id);

  // Proxy calls for WebAppProtocolHandlerManager.
  virtual absl::optional<GURL> TranslateProtocolUrl(const AppId& app_id,
                                                    const GURL& protocol_url);
  virtual std::vector<ProtocolHandler> GetHandlersForProtocol(
      const std::string& protocol);
  virtual std::vector<ProtocolHandler> GetAppProtocolHandlers(
      const AppId& app_id);
  virtual std::vector<ProtocolHandler> GetAllowedHandlersForProtocol(
      const std::string& protocol);
  virtual std::vector<ProtocolHandler> GetDisallowedHandlersForProtocol(
      const std::string& protocol);

  // Getter for testing WebAppFileHandlerManager
  WebAppFileHandlerManager& file_handler_manager_for_testing();

  UrlHandlerManager& url_handler_manager_for_testing();

  WebAppProtocolHandlerManager& protocol_handler_manager_for_testing();

  static ScopedOsHooksSuppress ScopedSuppressOsHooksForTesting();

  virtual FakeOsIntegrationManager* AsTestOsIntegrationManager();

  void set_url_handler_manager(
      std::unique_ptr<UrlHandlerManager> url_handler_manager) {
    url_handler_manager_ = std::move(url_handler_manager);
  }

  virtual void UpdateUrlHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback);

  virtual void UpdateFileHandlers(
      const AppId& app_id,
      FileHandlerUpdateAction file_handlers_need_os_update);

  // Updates protocol handler registrations with the OS.
  // If `force_shortcut_updates_if_needed` is true, then also update the
  // application's shortcuts.
  virtual void UpdateProtocolHandlers(const AppId& app_id,
                                      bool force_shortcut_updates_if_needed,
                                      base::OnceClosure callback);

 protected:
  WebAppShortcutManager* shortcut_manager() { return shortcut_manager_.get(); }
  WebAppFileHandlerManager* file_handler_manager() {
    return file_handler_manager_.get();
  }
  WebAppProtocolHandlerManager* protocol_handler_manager() {
    return protocol_handler_manager_.get();
  }
  UrlHandlerManager* url_handler_manager() {
    return url_handler_manager_.get();
  }
  void set_shortcut_manager(
      std::unique_ptr<WebAppShortcutManager> shortcut_manager) {
    shortcut_manager_ = std::move(shortcut_manager);
  }
  void set_file_handler_manager(
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager) {
    file_handler_manager_ = std::move(file_handler_manager);
  }
  void set_protocol_handler_manager(
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager) {
    protocol_handler_manager_ = std::move(protocol_handler_manager);
  }

  virtual void CreateShortcuts(const AppId& app_id,
                               bool add_to_desktop,
                               CreateShortcutsCallback callback);

  // Installation:
  virtual void RegisterFileHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback);
  virtual void RegisterProtocolHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback);
  virtual void RegisterUrlHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback);
  virtual void RegisterShortcutsMenu(
      const AppId& app_id,
      const std::vector<WebApplicationShortcutsMenuItemInfo>&
          shortcuts_menu_item_infos,
      const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
      base::OnceCallback<void(bool success)> callback);
  virtual void ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      const AppId& app_id,
      base::OnceCallback<void(bool success)> callback);
  virtual void RegisterRunOnOsLogin(const AppId& app_id,
                                    RegisterRunOnOsLoginCallback callback);
  virtual void MacAppShimOnAppInstalledForProfile(const AppId& app_id);
  virtual void AddAppToQuickLaunchBar(const AppId& app_id);
  virtual void RegisterWebAppOsUninstallation(const AppId& app_id,
                                              const std::string& name);

  // Uninstallation:
  virtual bool UnregisterShortcutsMenu(const AppId& app_id);
  virtual void UnregisterRunOnOsLogin(const AppId& app_id,
                                      const base::FilePath& profile_path,
                                      const std::u16string& shortcut_title,
                                      UnregisterRunOnOsLoginCallback callback);
  virtual void DeleteShortcuts(const AppId& app_id,
                               const base::FilePath& shortcuts_data_dir,
                               std::unique_ptr<ShortcutInfo> shortcut_info,
                               DeleteShortcutsCallback callback);
  virtual void UnregisterFileHandlers(const AppId& app_id,
                                      base::OnceCallback<void(bool)> callback);
  virtual void UnregisterProtocolHandlers(
      const AppId& app_id,
      base::OnceCallback<void(bool)> callback);
  virtual void UnregisterUrlHandlers(const AppId& app_id);
  virtual void UnregisterWebAppOsUninstallation(const AppId& app_id);

  // Update:
  virtual void UpdateShortcuts(const AppId& app_id,
                               base::StringPiece old_name,
                               base::OnceClosure callback);
  virtual void UpdateShortcutsMenu(const AppId& app_id,
                                   const WebApplicationInfo& web_app_info);

  // Utility methods:
  virtual std::unique_ptr<ShortcutInfo> BuildShortcutInfo(const AppId& app_id);

 private:
  class OsHooksBarrier;

  void OnShortcutsCreated(const AppId& app_id,
                          std::unique_ptr<WebApplicationInfo> web_app_info,
                          InstallOsHooksOptions options,
                          scoped_refptr<OsHooksBarrier> barrier,
                          bool shortcuts_created);

  void OnShortcutsDeleted(const AppId& app_id,
                          DeleteShortcutsCallback callback,
                          bool shortcuts_deleted);

  void OnShortcutInfoRetrievedRegisterRunOnOsLogin(
      RegisterRunOnOsLoginCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  // Called after the shortcuts for an app are updated in response
  // to protocol handler changes.
  // `update_finished_callback` is the callback provided in
  // `UpdateProtocolHandlers`.
  void OnShortcutsUpdatedForProtocolHandlers(
      const AppId& app_id,
      base::OnceClosure update_finished_callback);

  Profile* const profile_;
  WebAppRegistrar* registrar_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;

  std::unique_ptr<WebAppShortcutManager> shortcut_manager_;
  std::unique_ptr<WebAppFileHandlerManager> file_handler_manager_;
  std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager_;
  std::unique_ptr<UrlHandlerManager> url_handler_manager_;

  base::WeakPtrFactory<OsIntegrationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MANAGER_H_
