// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_ASH_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_ASH_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/apps/app_service/publisher_host.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/native_widget_types.h"

// Avoid including this header file directly or referring directly to
// AppServiceProxyAsh as a type. Instead:
//  - for forward declarations, use app_service_proxy_forward.h
//  - for the full header, use app_service_proxy.h, which aliases correctly
//    based on the platform

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace apps {

class AppPlatformMetrics;
class AppPlatformMetricsService;
class InstanceRegistryUpdater;
class BrowserAppInstanceRegistry;
class BrowserAppInstanceTracker;
class UninstallDialog;

struct PauseData {
  int hours = 0;
  int minutes = 0;
  bool should_show_pause_dialog = false;
};

// Singleton (per Profile) proxy and cache of an App Service's apps on Chrome
// OS.
//
// See components/services/app_service/README.md.
class AppServiceProxyAsh : public AppServiceProxyBase,
                           public apps::AppRegistryCache::Observer,
                           public apps::InstanceRegistry::Observer {
 public:
  using OnPauseDialogClosedCallback = base::OnceCallback<void()>;

  explicit AppServiceProxyAsh(Profile* profile);
  AppServiceProxyAsh(const AppServiceProxyAsh&) = delete;
  AppServiceProxyAsh& operator=(const AppServiceProxyAsh&) = delete;
  ~AppServiceProxyAsh() override;

  apps::InstanceRegistry& InstanceRegistry();
  apps::AppPlatformMetrics* AppPlatformMetrics();

  apps::BrowserAppInstanceTracker* BrowserAppInstanceTracker();
  apps::BrowserAppInstanceRegistry* BrowserAppInstanceRegistry();

  // apps::AppServiceProxyBase overrides:
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 gfx::NativeWindow parent_window) override;

  // Pauses apps. |pause_data|'s key is the app_id. |pause_data|'s PauseData
  // is the time limit setting for the app, which is shown in the pause app
  // dialog. AppService sets the paused status directly. If the app is running,
  // AppService shows the pause app dialog. Otherwise, AppService applies the
  // paused app icon effect directly.
  void PauseApps(const std::map<std::string, PauseData>& pause_data);

  // Unpauses the apps from the paused status. AppService sets the paused status
  // as false directly and removes the paused app icon effect.
  void UnpauseApps(const std::set<std::string>& app_ids);

  // Set whether resize lock is enabled for the app identified by |app_id|.
  void SetResizeLocked(const std::string& app_id,
                       apps::mojom::OptionalBool locked);

  // Sets |extension_apps_| and |web_apps_| to observe the ARC apps to set the
  // badge on the equivalent Chrome app's icon, when ARC is available.
  void SetArcIsRegistered();

  // apps::AppServiceProxyBase overrides:
  void FlushMojoCallsForTesting() override;

  void ReInitializeCrostiniForTesting();
  void SetDialogCreatedCallbackForTesting(base::OnceClosure callback);
  void UninstallForTesting(const std::string& app_id,
                           gfx::NativeWindow parent_window,
                           base::OnceClosure callback);
  void SetAppPlatformMetricsServiceForTesting(
      std::unique_ptr<apps::AppPlatformMetricsService>
          app_platform_metrics_service);

 private:
  // For access to Initialize.
  friend class AppServiceProxyFactory;
  FRIEND_TEST_ALL_PREFIXES(AppServiceProxyTest, LaunchCallback);

  using UninstallDialogs = std::set<std::unique_ptr<apps::UninstallDialog>,
                                    base::UniquePtrComparator>;

  void Initialize() override;

  // KeyedService overrides.
  void Shutdown() override;

  static void CreateBlockDialog(const std::string& app_name,
                                const gfx::ImageSkia& image,
                                Profile* profile);

  static void CreatePauseDialog(apps::mojom::AppType app_type,
                                const std::string& app_name,
                                const gfx::ImageSkia& image,
                                const PauseData& pause_data,
                                OnPauseDialogClosedCallback pause_callback);

  void UninstallImpl(const std::string& app_id,
                     apps::mojom::UninstallSource uninstall_source,
                     gfx::NativeWindow parent_window,
                     base::OnceClosure callback);

  // Invoked when the uninstall dialog is closed. The app for the given
  // |app_type| and |app_id| will be uninstalled directly if |uninstall| is
  // true. |clear_site_data| is available for bookmark apps only. If true, any
  // site data associated with the app will be removed. |report_abuse| is
  // available for Chrome Apps only. If true, the app will be reported for abuse
  // to the Web Store. |uninstall_dialog| will be removed from
  // |uninstall_dialogs_|.
  void OnUninstallDialogClosed(apps::mojom::AppType app_type,
                               const std::string& app_id,
                               apps::mojom::UninstallSource uninstall_source,
                               bool uninstall,
                               bool clear_site_data,
                               bool report_abuse,
                               UninstallDialog* uninstall_dialog);

  // apps::AppServiceProxyBase overrides:
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update) override;
  void OnLaunched(LaunchCallback callback,
                  LaunchResult&& launch_result) override;

  // Loads the icon for the app block dialog or the app pause dialog.
  void LoadIconForDialog(const apps::AppUpdate& update,
                         apps::LoadIconCallback callback);

  // Callback invoked when the icon is loaded for the block app dialog.
  void OnLoadIconForBlockDialog(const std::string& app_name,
                                IconValuePtr icon_value);

  // Callback invoked when the icon is loaded for the pause app dialog.
  void OnLoadIconForPauseDialog(apps::mojom::AppType app_type,
                                const std::string& app_id,
                                const std::string& app_name,
                                const PauseData& pause_data,
                                IconValuePtr icon_value);

  // Invoked when the user clicks the 'OK' button of the pause app dialog.
  // AppService stops the running app and applies the paused app icon effect.
  void OnPauseDialogClosed(apps::mojom::AppType app_type,
                           const std::string& app_id);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void PerformPostLaunchTasks(apps::mojom::LaunchSource launch_source) override;

  void RecordAppPlatformMetrics(
      Profile* profile,
      const apps::AppUpdate& update,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::LaunchContainer container) override;

  void InitAppPlatformMetrics();

  void PerformPostUninstallTasks(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::UninstallSource uninstall_source) override;

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  std::unique_ptr<PublisherHost> publisher_host_;

  bool arc_is_registered_ = false;

  apps::InstanceRegistry instance_registry_;

  std::unique_ptr<apps::BrowserAppInstanceTracker>
      browser_app_instance_tracker_;
  std::unique_ptr<apps::BrowserAppInstanceRegistry>
      browser_app_instance_registry_;
  std::unique_ptr<apps::InstanceRegistryUpdater>
      browser_app_instance_app_service_updater_;

  // When PauseApps is called, the app is added to |pending_pause_requests|.
  // When the user clicks the OK from the pause app dialog, the pause status is
  // updated in AppRegistryCache by the publisher, then the app is removed from
  // |pending_pause_requests|. If the app status is paused in AppRegistryCache
  // or pending_pause_requests, the app can't be launched.
  PausedApps pending_pause_requests_;

  UninstallDialogs uninstall_dialogs_;

  std::unique_ptr<apps::AppPlatformMetricsService>
      app_platform_metrics_service_;

  // App service require the Lacros Browser to keep alive for web apps.
  // TODO(crbug.com/1174246): Support Lacros not keeping alive.
  std::unique_ptr<crosapi::BrowserManager::ScopedKeepAlive> keep_alive_;

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observer_{this};

  // A map to record the launch callbacks for an app instance. Note it is
  // possible to have multiple launches to launch the same app instance for
  // web apps, so we record a list of launch callbacks for each instance.
  std::map<base::UnguessableToken, std::vector<LaunchCallback>> callback_list_;
  base::WeakPtrFactory<AppServiceProxyAsh> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_ASH_H_
