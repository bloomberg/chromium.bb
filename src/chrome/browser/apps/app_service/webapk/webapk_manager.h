// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_MANAGER_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class PrefChangeRegistrar;
class Profile;

namespace ash {
class ApkWebAppService;
}

namespace apps {

class WebApkInstallQueue;

class WebApkManager : public AppRegistryCache::Observer,
                      public ArcAppListPrefs::Observer,
                      public arc::ArcSessionManagerObserver {
 public:
  explicit WebApkManager(Profile* profile);
  ~WebApkManager() override;

  WebApkManager(const WebApkManager&) = delete;
  WebApkManager& operator=(const WebApkManager&) = delete;

  WebApkInstallQueue* GetInstallQueueForTest();

 private:
  // Checks whether WebAPKs should be enabled for the current profile and
  // starts/stops observing app changes as appropriate.
  void StartOrStopObserving();

  // Called to synchronize installed WebAPKs with the currently installed apps.
  void Synchronize();

  bool IsAppEligibleForWebApk(const AppUpdate& app);
  void QueueInstall(const std::string& app_id);
  void QueueUpdate(const std::string& app_id);
  void QueueUninstall(const std::string& app_id);
  void UninstallInternal(const std::string& app_id);

  // AppRegistryCache::Observer:
  void OnAppUpdate(const AppUpdate& update) override;
  void OnAppTypeInitialized(apps::mojom::AppType type) override;
  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override;

  // ArcAppListPrefs::Observer:
  void OnPackageListInitialRefreshed() override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;

  // ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  Profile* profile_;
  AppServiceProxy* proxy_;
  ash::ApkWebAppService* apk_service_;
  ArcAppListPrefs* app_list_prefs_;

  bool initialized_;

  std::unique_ptr<WebApkInstallQueue> install_queue_;
  std::vector<std::string> uninstall_queue_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_app_list_prefs_observer_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_MANAGER_H_
