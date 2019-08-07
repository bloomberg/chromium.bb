// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service_factory.h"
#include "chrome/browser/chromeos/kiosk_next_home/intent_config_helper.h"
#include "chrome/browser/chromeos/kiosk_next_home/metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/services/app_service/public/cpp/app_service_proxy.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/url_data_source.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace chromeos {
namespace kiosk_next_home {

namespace {

// Returns AppInstance from ArcBridgeService if available, or nullptr.
arc::mojom::AppInstance* GetArcAppInstanceForLaunchIntent() {
  return arc::ArcServiceManager::Get()
             ? ARC_GET_INSTANCE_FOR_METHOD(
                   arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                   LaunchIntent)
             : nullptr;
}

void RecordLaunchIntentResult(LaunchIntentResult result) {
  UMA_HISTOGRAM_ENUMERATION("KioskNextHome.Bridge.LaunchIntentResult", result);
}

}  // namespace

// static
AppControllerService* AppControllerService::Get(
    content::BrowserContext* context) {
  return AppControllerServiceFactory::GetForBrowserContext(context);
}

AppControllerService::AppControllerService(Profile* profile)
    : profile_(profile),
      app_service_proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)),
      intent_config_helper_(IntentConfigHelper::GetInstance()) {
  DCHECK(profile);
  Observe(&app_service_proxy_->AppRegistryCache());

  // Add the chrome://app-icon URL data source.
  // TODO(ltenorio): Move this to a more suitable location when we change
  // the Kiosk Next Home to WebUI.
  content::URLDataSource::Add(profile,
                              std::make_unique<apps::AppIconSource>(profile));
}

AppControllerService::~AppControllerService() = default;

void AppControllerService::BindRequest(mojom::AppControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppControllerService::GetApps(
    mojom::AppController::GetAppsCallback callback) {
  RecordBridgeAction(BridgeAction::kListApps);
  std::vector<chromeos::kiosk_next_home::mojom::AppPtr> app_list;
  // Using AppUpdate objects here since that's how the app list is intended to
  // be consumed. Refer to AppRegistryCache::ForEachApp for more information.
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [this, &app_list](const apps::AppUpdate& update) {
        // Only include apps that are both relevant and installed.
        if (AppIsRelevantForKioskNextHome(update) &&
            update.Readiness() != apps::mojom::Readiness::kUninstalledByUser) {
          app_list.push_back(CreateAppPtr(update));
        }
      });
  std::move(callback).Run(std::move(app_list));
}

void AppControllerService::SetClient(mojom::AppControllerClientPtr client) {
  client_ = std::move(client);
}

void AppControllerService::LaunchApp(const std::string& app_id) {
  RecordBridgeAction(BridgeAction::kLaunchApp);
  app_service_proxy_->Launch(app_id, ui::EventFlags::EF_NONE,
                             apps::mojom::LaunchSource::kFromKioskNextHome,
                             display::kDefaultDisplayId);
}

void AppControllerService::UninstallApp(const std::string& app_id) {
  RecordBridgeAction(BridgeAction::kUninstallApp);
  app_service_proxy_->Uninstall(app_id);
}

void AppControllerService::GetArcAndroidId(
    mojom::AppController::GetArcAndroidIdCallback callback) {
  RecordBridgeAction(BridgeAction::kGetAndroidId);
  arc::GetAndroidId(base::BindOnce(
      [](mojom::AppController::GetArcAndroidIdCallback callback, bool success,
         int64_t raw_android_id) {
        UMA_HISTOGRAM_BOOLEAN("KioskNextHome.Bridge.GetAndroidIdSuccess",
                              success);
        // The bridge expects the Android id as a hex string.
        std::stringstream android_id_stream;
        android_id_stream << std::hex << raw_android_id;
        std::move(callback).Run(success, android_id_stream.str());
      },
      std::move(callback)));
}

void AppControllerService::LaunchIntent(const std::string& intent,
                                        LaunchIntentCallback callback) {
  RecordBridgeAction(BridgeAction::kLaunchIntent);
  GURL intent_uri(intent);
  if (!intent_config_helper_->IsIntentAllowed(intent_uri)) {
    RecordLaunchIntentResult(LaunchIntentResult::kNotAllowed);
    std::move(callback).Run(false, "Intent not allowed.");
    return;
  }

  arc::mojom::AppInstance* app_instance = GetArcAppInstanceForLaunchIntent();
  if (!app_instance) {
    RecordLaunchIntentResult(LaunchIntentResult::kArcUnavailable);
    std::move(callback).Run(false, "ARC bridge not available.");
    return;
  }

  RecordLaunchIntentResult(LaunchIntentResult::kSuccess);
  app_instance->LaunchIntent(intent_uri.spec(), display::kDefaultDisplayId);
  std::move(callback).Run(true, base::nullopt);
}

void AppControllerService::OnAppUpdate(const apps::AppUpdate& update) {
  // Skip this event if there were no changes to the fields that we are
  // interested in.
  if (!update.StateIsNull() && !update.NameChanged() &&
      !update.ReadinessChanged() && !update.ShowInLauncherChanged()) {
    return;
  }

  // Skip this app if it's not relevant.
  if (!AppIsRelevantForKioskNextHome(update))
    return;

  if (client_) {
    RecordBridgeAction(BridgeAction::kNotifiedAppChange);
    client_->OnAppChanged(CreateAppPtr(update));
  }
}

void AppControllerService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void AppControllerService::SetIntentConfigHelperForTesting(
    std::unique_ptr<IntentConfigHelper> helper) {
  intent_config_helper_ = std::move(helper);
}

mojom::AppPtr AppControllerService::CreateAppPtr(
    const apps::AppUpdate& update) {
  auto app = chromeos::kiosk_next_home::mojom::App::New();
  app->app_id = update.AppId();
  app->type = update.AppType();
  app->display_name = update.Name();
  app->readiness = update.Readiness();

  if (app->type == apps::mojom::AppType::kArc)
    app->android_package_name = MaybeGetAndroidPackageName(app->app_id);

  return app;
}

bool AppControllerService::AppIsRelevantForKioskNextHome(
    const apps::AppUpdate& update) {
  // The Kiosk Next Home app should never be returned since it's considered an
  // implementation detail.
  if (update.AppId() == extension_misc::kKioskNextHomeAppId)
    return false;

  // We only consider relevant apps that can be shown in the launcher.
  // This skips hidden apps like Galery, Web store, Welcome app, etc.
  return update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue;
}

const std::string& AppControllerService::MaybeGetAndroidPackageName(
    const std::string& app_id) {
  // Try to find a cached package name for this app.
  const auto& package_name_it = android_package_map_.find(app_id);
  if (package_name_it != android_package_map_.end()) {
    return package_name_it->second;
  }

  // If we don't find it, try to get the package name from ARC prefs.
  std::string package_name = arc::AppIdToArcPackageName(app_id, profile_);
  if (package_name.empty()) {
    return base::EmptyString();
  }

  // Now that we have a valid package name, update our caches.
  android_package_map_[app_id] = package_name;
  return android_package_map_[app_id];
}

}  // namespace kiosk_next_home
}  // namespace chromeos
