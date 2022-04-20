// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

namespace {

// Returns true if app's launch info should be saved to full restore.
bool ShouldSaveToFullRestore(AppServiceProxy* proxy,
                             const std::string& app_id) {
  if (!::full_restore::features::IsFullRestoreForLacrosEnabled())
    return false;

  bool is_platform_app = true;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&is_platform_app](const apps::AppUpdate& update) {
        is_platform_app = update.IsPlatformApp().value_or(true);
      });
  // Note: Hosted apps will be restored by Lacros session restoration. No need
  // to save launch info to full restore. Only platform app's launch info should
  // be saved to full restore.
  return is_platform_app;
}

}  // namespace

StandaloneBrowserExtensionApps::StandaloneBrowserExtensionApps(
    AppServiceProxy* proxy,
    AppType app_type)
    : apps::AppPublisher(proxy), app_type_(app_type) {
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }
  PublisherBase::Initialize(app_service,
                            apps::ConvertAppTypeToMojomAppType(app_type_));
  login_observation_.Observe(chromeos::LoginState::Get());
  // Check now in case login has already happened.
  LoggedInStateChanged();
}

StandaloneBrowserExtensionApps::~StandaloneBrowserExtensionApps() = default;

void StandaloneBrowserExtensionApps::RegisterCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  RegisterPublisher(app_type_);

  // At the moment the app service publisher will only accept one browser client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&StandaloneBrowserExtensionApps::OnReceiverDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void StandaloneBrowserExtensionApps::LoadIcon(const std::string& app_id,
                                              const IconKey& icon_key,
                                              IconType icon_type,
                                              int32_t size_hint_in_dip,
                                              bool allow_placeholder_icon,
                                              apps::LoadIconCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  IconType crosapi_icon_type = icon_type;
  IconKeyPtr crosapi_icon_key = icon_key.Clone();
  if (crosapi_icon_type == apps::IconType::kCompressed) {
    // If the request is for a compressed icon, modify request so that
    // uncompressed icon is sent over crosapi.
    crosapi_icon_type = apps::IconType::kUncompressed;
    crosapi_icon_key->icon_effects = apps::IconEffects::kNone;

    // To compensate for the above, wrap |callback| icon recompression. This is
    // applied after OnLoadIcon() runs, which is appropriate since OnLoadIcon()
    // needs an uncompressed icon for ApplyIconEffects().
    callback = base::BindOnce(
        [](apps::LoadIconCallback wrapped_callback, IconValuePtr icon_value) {
          ConvertUncompressedIconToCompressedIcon(std::move(icon_value),
                                                  std::move(wrapped_callback));
        },
        std::move(callback));
  }

  const uint32_t icon_effects = icon_key.icon_effects;
  controller_->LoadIcon(
      app_id, std::move(crosapi_icon_key), crosapi_icon_type, size_hint_in_dip,
      base::BindOnce(&StandaloneBrowserExtensionApps::OnLoadIcon,
                     weak_factory_.GetWeakPtr(), icon_effects, size_hint_in_dip,
                     std::move(callback)));
}

void StandaloneBrowserExtensionApps::LaunchAppWithParams(
    AppLaunchParams&& params,
    LaunchCallback callback) {
  if (!controller_.is_bound()) {
    std::move(callback).Run(LaunchResult());
    return;
  }

  controller_->Launch(
      apps::ConvertLaunchParamsToCrosapi(
          params, ProfileManager::GetPrimaryUserProfile()),
      apps::LaunchResultToMojomLaunchResultCallback(std::move(callback)));

  if (ShouldSaveToFullRestore(proxy(), params.app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        params.app_id, params.container, params.disposition, params.display_id,
        std::move(params.launch_files), std::move(params.intent));
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));

  mojo::RemoteSetElementId id = subscribers_.Add(std::move(subscriber));

  std::vector<apps::mojom::AppPtr> apps;
  for (auto& it : app_mojom_cache_) {
    apps.push_back(it.second.Clone());
  }

  subscribers_.Get(id)->OnApps(std::move(apps),
                               apps::ConvertAppTypeToMojomAppType(app_type_),
                               true /* should_notify_initialized */);
}

void StandaloneBrowserExtensionApps::LoadIcon(const std::string& app_id,
                                              apps::mojom::IconKeyPtr icon_key,
                                              apps::mojom::IconType icon_type,
                                              int32_t size_hint_in_dip,
                                              bool allow_placeholder_icon,
                                              LoadIconCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  controller_->LoadIcon(app_id, ConvertMojomIconKeyToIconKey(icon_key),
                        ConvertMojomIconTypeToIconType(icon_type),
                        size_hint_in_dip,
                        IconValueToMojomIconValueCallback(std::move(callback)));
}

void StandaloneBrowserExtensionApps::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  // The following code assumes |app_type_| must be
  // AppType::kStandaloneBrowserChromeApp. Therefore, the app must be either
  // platform app or hosted app.
  // In the future, this class is possible to be instantiated with other
  // AppType, please make sure to modify the logic if necessary.
  controller_->Launch(
      CreateCrosapiLaunchParamsWithEventFlags(
          proxy(), app_id, event_flags, launch_source,
          window_info ? window_info->display_id : display::kInvalidDisplayId),
      /*callback=*/base::DoNothing());

  if (ShouldSaveToFullRestore(proxy(), app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
        std::vector<base::FilePath>{}, nullptr);
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent = apps_util::ConvertAppServiceToCrosapiIntent(
      intent, ProfileManager::GetPrimaryUserProfile());
  controller_->Launch(std::move(launch_params),
                      /*callback=*/base::DoNothing());
  std::move(callback).Run(/*success=*/true);

  if (ShouldSaveToFullRestore(proxy(), app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
        std::vector<base::FilePath>{}, std::move(intent));
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent =
      apps_util::CreateCrosapiIntentForViewFiles(file_paths);
  controller_->Launch(std::move(launch_params),
                      /*callback=*/base::DoNothing());

  if (ShouldSaveToFullRestore(proxy(), app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
        std::move(file_paths->file_paths), nullptr);
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    GetMenuModelCallback callback) {
  bool is_platform_app = true;
  bool can_use_uninstall = false;
  bool show_app_info = false;
  WindowMode display_mode = WindowMode::kUnknown;
  proxy()->AppRegistryCache().ForOneApp(
      app_id, [&is_platform_app, &can_use_uninstall, &show_app_info,
               &display_mode](const apps::AppUpdate& update) {
        is_platform_app = update.IsPlatformApp().value_or(true);
        can_use_uninstall = update.AllowUninstall().value_or(false);
        show_app_info = update.ShowInManagement().value_or(false);
        display_mode = update.WindowMode();
      });

  // This provides the context menu for hosted app in standalone browser.
  // Note: The context menu for platform app in standalone browser is provided
  // by StandaloneBrowserExtensionAppContextMenu.
  DCHECK(!is_platform_app);

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();
  apps::CreateOpenNewSubmenu(menu_type,
                             display_mode == WindowMode::kWindow
                                 ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
                                 : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
                             &menu_items);

  if (menu_type == apps::mojom::MenuType::kShelf) {
    if (proxy()->BrowserAppInstanceRegistry()->IsAppRunning(app_id)) {
      apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                           &menu_items);
    }
  }

  if (can_use_uninstall) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (show_app_info) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void StandaloneBrowserExtensionApps::StopApp(const std::string& app_id) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  controller_->StopApp(app_id);
}
void StandaloneBrowserExtensionApps::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  controller_->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);
}

void StandaloneBrowserExtensionApps::SetWindowMode(
    const std::string& app_id,
    apps::mojom::WindowMode window_mode) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  controller_->SetWindowMode(app_id,
                             ConvertMojomWindowModeToWindowMode(window_mode));
}

void StandaloneBrowserExtensionApps::OnApps(std::vector<AppPtr> deltas) {
  if (deltas.empty()) {
    return;
  }

  if (controller_.is_bound()) {
    for (const AppPtr& delta : deltas) {
      app_mojom_cache_[delta->app_id] = ConvertAppToMojomApp(delta);
      PublisherBase::Publish(ConvertAppToMojomApp(delta), subscribers_);
    }

    apps::AppPublisher::Publish(std::move(deltas), app_type_,
                                should_notify_initialized_);
    should_notify_initialized_ = false;
  } else {
    // If `controller_` is not bound, add `deltas` to `app_cache_` to wait for
    // registering the crosapi controller to publish all deltas saved in
    // `app_cache_`.
    for (AppPtr& delta : deltas) {
      app_cache_[delta->app_id] = std::move(delta);
    }
  }
}

void StandaloneBrowserExtensionApps::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  if (controller_.is_bound()) {
    return;
  }

  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(
      base::BindOnce(&StandaloneBrowserExtensionApps::OnControllerDisconnected,
                     weak_factory_.GetWeakPtr()));
  if (app_cache_.empty()) {
    // If there is no apps saved in `app_cache_`, still publish an empty app
    // list to initialize `app_type_`.
    apps::AppPublisher::Publish(std::vector<AppPtr>{}, app_type_,
                                should_notify_initialized_);
  } else {
    std::vector<AppPtr> deltas;
    for (auto& it : app_cache_) {
      app_mojom_cache_[it.first] = ConvertAppToMojomApp(it.second);
      PublisherBase::Publish(ConvertAppToMojomApp(it.second), subscribers_);
      deltas.push_back(std::move(it.second));
    }
    app_cache_.clear();
    apps::AppPublisher::Publish(std::move(deltas), app_type_,
                                should_notify_initialized_);
  }
  should_notify_initialized_ = false;
}

void StandaloneBrowserExtensionApps::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void StandaloneBrowserExtensionApps::LoggedInStateChanged() {
  if (chromeos::LoginState::Get()->IsUserLoggedIn()) {
    if (!keep_alive_) {
      if (app_type_ == AppType::kStandaloneBrowserChromeApp) {
        keep_alive_ = crosapi::BrowserManager::Get()->KeepAlive(
            crosapi::BrowserManager::Feature::kChromeApps);
      } else if (app_type_ == AppType::kStandaloneBrowserExtension) {
        keep_alive_ = crosapi::BrowserManager::Get()->KeepAlive(
            crosapi::BrowserManager::Feature::kExtensions);
      }
    }
  } else {
    keep_alive_.reset();
  }
}

void StandaloneBrowserExtensionApps::OnReceiverDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void StandaloneBrowserExtensionApps::OnControllerDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void StandaloneBrowserExtensionApps::OnLoadIcon(uint32_t icon_effects,
                                                int size_hint_in_dip,
                                                apps::LoadIconCallback callback,
                                                IconValuePtr icon_value) {
  // Apply masking effects here since masking is unimplemented in Lacros.
  ApplyIconEffects(static_cast<IconEffects>(icon_effects), size_hint_in_dip,
                   std::move(icon_value), std::move(callback));
}

}  // namespace apps
