// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/kiosk_next_home/mojom/app_controller.mojom.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace apps {
class AppServiceProxy;
class AppUpdate;
}  // namespace apps

namespace chromeos {
namespace kiosk_next_home {

class IntentConfigHelper;

// Service implementation for the Kiosk Next AppController.
// This class is responsible for managing Chrome OS apps and returning useful
// information about them to the Kiosk Next Home.
class AppControllerService : public mojom::AppController,
                             public KeyedService,
                             public apps::AppRegistryCache::Observer {
 public:
  // Returns the AppControllerService singleton attached to this |context|.
  static AppControllerService* Get(content::BrowserContext* context);

  explicit AppControllerService(Profile* profile);
  ~AppControllerService() override;

  // Binds this service to an AppController request.
  void BindRequest(mojom::AppControllerRequest request);

  // mojom::AppController:
  void GetApps(mojom::AppController::GetAppsCallback callback) override;
  void SetClient(mojom::AppControllerClientPtr client) override;
  void LaunchApp(const std::string& app_id) override;
  void UninstallApp(const std::string& app_id) override;
  void GetArcAndroidId(
      mojom::AppController::GetArcAndroidIdCallback callback) override;
  void LaunchIntent(const std::string& intent,
                    LaunchIntentCallback callback) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Allows overriding the intent config helper for tests.
  void SetIntentConfigHelperForTesting(
      std::unique_ptr<IntentConfigHelper> helper);

 private:
  // Creates a new Kiosk Next App from a delta app update coming from
  // AppServiceProxy.
  mojom::AppPtr CreateAppPtr(const apps::AppUpdate& update);

  bool AppIsRelevantForKioskNextHome(const apps::AppUpdate& update);

  // Tries to get the Android package name for this app from ARC++.
  // If we can't find the package name or this is not an Android app we return
  // an empty string.
  const std::string& MaybeGetAndroidPackageName(const std::string& app_id);

  Profile* profile_;
  mojo::BindingSet<mojom::AppController> bindings_;
  mojom::AppControllerClientPtr client_;
  apps::AppServiceProxy* app_service_proxy_;
  std::unique_ptr<IntentConfigHelper> intent_config_helper_;

  // Map of app ids to android packages.
  //
  // Since app ids and packages are stable, we can store them here
  // and avoid queries to Android prefs.
  // This cache also takes care of the case when an Android app is uninstalled
  // and we need to notify the bridge.
  // In that case we only receive the notification from the AppService after
  // ARC++ already lost the information about app, so keeping a cache on our end
  // is necessary.
  std::map<std::string, std::string> android_package_map_;

  DISALLOW_COPY_AND_ASSIGN(AppControllerService);
};

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_SERVICE_H_
