// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/crosapi_utils.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

namespace apps {

WebAppsCrosapi::WebAppsCrosapi(AppServiceProxy* proxy)
    : apps::AppPublisher(proxy), proxy_(proxy) {
  // This object may be created when the flag is on or off, but only register
  // the publisher if the flag is on.
  if (web_app::IsWebAppsCrosapiEnabled()) {
    mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
    if (!app_service.is_bound()) {
      return;
    }
    PublisherBase::Initialize(app_service, apps::mojom::AppType::kWeb);
  }
}

WebAppsCrosapi::~WebAppsCrosapi() = default;

void WebAppsCrosapi::RegisterWebAppsCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    RegisterPublisher(AppType::kWeb);
  }

  // At the moment the app service publisher will only accept one client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::LoadIcon(const std::string& app_id,
                              const IconKey& icon_key,
                              IconType icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              apps::LoadIconCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  const uint32_t icon_effects = icon_key.icon_effects;

  IconType crosapi_icon_type = icon_type;
  apps::mojom::IconKeyPtr crosapi_icon_key =
      ConvertIconKeyToMojomIconKey(icon_key);
  if (crosapi_icon_type == apps::IconType::kCompressed) {
    // The effects are applied here in Ash.
    crosapi_icon_type = apps::IconType::kUncompressed;
    crosapi_icon_key->icon_effects = apps::IconEffects::kNone;
  }

  controller_->LoadIcon(
      app_id, std::move(crosapi_icon_key), crosapi_icon_type, size_hint_in_dip,
      base::BindOnce(&WebAppsCrosapi::OnLoadIcon, weak_factory_.GetWeakPtr(),
                     icon_type, size_hint_in_dip,
                     static_cast<apps::IconEffects>(icon_effects),
                     std::move(callback)));
}

void WebAppsCrosapi::LaunchAppWithParams(AppLaunchParams&& params,
                                         LaunchCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(LaunchResult());
    return;
  }
  controller_->Launch(
      apps::ConvertLaunchParamsToCrosapi(params, proxy_->profile()),
      apps::LaunchResultToMojomLaunchResultCallback(std::move(callback)));
}

void WebAppsCrosapi::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscribers_.Add(std::move(subscriber));
}

void WebAppsCrosapi::LoadIcon(const std::string& app_id,
                              apps::mojom::IconKeyPtr icon_key,
                              apps::mojom::IconType mojom_icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              LoadIconCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  if (!icon_key) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  const IconType icon_type = ConvertMojomIconTypeToIconType(mojom_icon_type);
  const apps::IconEffects icon_effects =
      static_cast<apps::IconEffects>(icon_key->icon_effects);
  controller_->LoadIcon(
      app_id, std::move(icon_key), icon_type, size_hint_in_dip,
      base::BindOnce(&WebAppsCrosapi::OnLoadIcon, weak_factory_.GetWeakPtr(),
                     icon_type, size_hint_in_dip, icon_effects,
                     IconValueToMojomIconValueCallback(std::move(callback))));
}

void WebAppsCrosapi::Launch(const std::string& app_id,
                            int32_t event_flags,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::WindowInfoPtr window_info) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->Launch(
      CreateCrosapiLaunchParamsWithEventFlags(
          proxy_, app_id, event_flags, launch_source,
          window_info ? window_info->display_id : display::kInvalidDisplayId),
      base::DoNothing());
}

void WebAppsCrosapi::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      proxy_, app_id, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);

  params->intent =
      apps_util::ConvertAppServiceToCrosapiIntent(intent, proxy_->profile());
  controller_->Launch(std::move(params), base::DoNothing());
  // TODO(crbug/1261263): handle the case where launch fails.
  std::move(callback).Run(/*success=*/true);
}

void WebAppsCrosapi::LaunchAppWithFiles(const std::string& app_id,
                                        int32_t event_flags,
                                        apps::mojom::LaunchSource launch_source,
                                        apps::mojom::FilePathsPtr file_paths) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      proxy_, app_id, event_flags, launch_source, display::kInvalidDisplayId);
  params->intent = apps_util::CreateCrosapiIntentForViewFiles(file_paths);
  controller_->Launch(std::move(params), base::DoNothing());
}

void WebAppsCrosapi::Uninstall(const std::string& app_id,
                               apps::mojom::UninstallSource uninstall_source,
                               bool clear_site_data,
                               bool report_abuse) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);
}

void WebAppsCrosapi::GetMenuModel(const std::string& app_id,
                                  apps::mojom::MenuType menu_type,
                                  int64_t display_id,
                                  GetMenuModelCallback callback) {
  bool is_system_web_app = false;
  bool can_use_uninstall = false;
  apps::mojom::WindowMode display_mode = apps::mojom::WindowMode::kUnknown;

  proxy_->AppRegistryCache().ForOneApp(
      app_id, [&is_system_web_app, &can_use_uninstall,
               &display_mode](const apps::AppUpdate& update) {
        is_system_web_app =
            update.InstallReason() == apps::mojom::InstallReason::kSystem;
        can_use_uninstall =
            update.AllowUninstall() == apps::mojom::OptionalBool::kTrue;
        display_mode = update.WindowMode();
      });

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (display_mode != apps::mojom::WindowMode::kUnknown && !is_system_web_app) {
    apps::CreateOpenNewSubmenu(menu_type,
                               display_mode == apps::mojom::WindowMode::kBrowser
                                   ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                                   : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                               &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf) {
    // TODO(crbug.com/1203992): We cannot use InstanceRegistry with lacros yet,
    // because InstanceRegistry updates for lacros isn't implemented yet, so we
    // need to check BrowserAppInstanceRegistry directly. Remove this when
    // InstanceRegistry updates are implemented.
    bool app_running =
        web_app::IsWebAppsCrosapiEnabled()
            ? proxy_->BrowserAppInstanceRegistry()->IsAppRunning(app_id)
            : proxy_->InstanceRegistry().ContainsAppId(app_id);
    if (app_running) {
      apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                           &menu_items);
    }
  }

  if (can_use_uninstall) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (!is_system_web_app) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI)) {
    if (!LogIfNotConnected(FROM_HERE)) {
      std::move(callback).Run(std::move(menu_items));
      return;
    }

    controller_->GetMenuModel(
        app_id, base::BindOnce(&WebAppsCrosapi::OnGetMenuModelFromCrosapi,
                               weak_factory_.GetWeakPtr(), app_id, menu_type,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebAppsCrosapi::OnGetMenuModelFromCrosapi(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    crosapi::mojom::MenuItemsPtr crosapi_menu_items) {
  if (crosapi_menu_items->items.empty()) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  auto separator_type = ui::DOUBLE_SEPARATOR;
  const int crosapi_menu_items_size = crosapi_menu_items->items.size();

  for (int item_index = 0; item_index < crosapi_menu_items_size; item_index++) {
    const auto& crosapi_menu_item = crosapi_menu_items->items[item_index];
    apps::AddSeparator(std::exchange(separator_type, ui::PADDED_SEPARATOR),
                       &menu_items);

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + item_index;

    auto& icon_image = crosapi_menu_item->image;

    icon_image = apps::ApplyBackgroundAndMask(icon_image);

    apps::AddShortcutCommandItem(command_id, crosapi_menu_item->id.value_or(""),
                                 crosapi_menu_item->label, icon_image,
                                 &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebAppsCrosapi::PauseApp(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->PauseApp(app_id);
}

void WebAppsCrosapi::UnpauseApp(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->UnpauseApp(app_id);
}

void WebAppsCrosapi::StopApp(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->StopApp(app_id);
}

void WebAppsCrosapi::OpenNativeSettings(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->OpenNativeSettings(app_id);
}

void WebAppsCrosapi::SetWindowMode(const std::string& app_id,
                                   apps::mojom::WindowMode window_mode) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->SetWindowMode(app_id, window_mode);
}

void WebAppsCrosapi::ExecuteContextMenuCommand(const std::string& app_id,
                                               int command_id,
                                               const std::string& shortcut_id,
                                               int64_t display_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->ExecuteContextMenuCommand(app_id, shortcut_id,
                                         base::DoNothing());
}

void WebAppsCrosapi::SetPermission(const std::string& app_id,
                                   apps::mojom::PermissionPtr permission) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->SetPermission(app_id, std::move(permission));
}

void WebAppsCrosapi::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  if (!web_app::IsWebAppsCrosapiEnabled())
    return;

  std::vector<std::unique_ptr<App>> apps;
  for (apps::mojom::AppPtr& delta : deltas) {
    apps.push_back(ConvertMojomAppToApp(delta));
  }
  apps::AppPublisher::Publish(std::move(apps));

  for (auto& subscriber : subscribers_) {
    subscriber->OnApps(apps_util::CloneStructPtrVector(deltas),
                       apps::mojom::AppType::kWeb, should_notify_initialized_);
  }
  should_notify_initialized_ = false;
}

void WebAppsCrosapi::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  if (controller_.is_bound()) {
    return;
  }
  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnControllerDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  if (!web_app::IsWebAppsCrosapiEnabled())
    return;
  for (auto& subscriber : subscribers_) {
    subscriber->OnCapabilityAccesses(apps_util::CloneStructPtrVector(deltas));
  }
}

bool WebAppsCrosapi::LogIfNotConnected(const base::Location& from_here) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.

  if (controller_.is_bound()) {
    return true;
  }
  LOG(WARNING) << "Controller not connected: " << from_here.ToString();
  return false;
}

void WebAppsCrosapi::OnCrosapiDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void WebAppsCrosapi::OnControllerDisconnected() {
  controller_.reset();
}

void WebAppsCrosapi::OnLoadIcon(IconType icon_type,
                                int size_hint_in_dip,
                                apps::IconEffects icon_effects,
                                apps::LoadIconCallback callback,
                                IconValuePtr icon_value) {
  if (!icon_value) {
    std::move(callback).Run(IconValuePtr());
    return;
  }
  // We apply the masking effect here, as masking is not implemented in Lacros.
  // (There is no resource file in the Lacros side to apply the icon effects.)
  ApplyIconEffects(icon_effects, size_hint_in_dip, std::move(icon_value),
                   base::BindOnce(&WebAppsCrosapi::OnApplyIconEffects,
                                  weak_factory_.GetWeakPtr(), icon_type,
                                  std::move(callback)));
}

void WebAppsCrosapi::OnApplyIconEffects(IconType icon_type,
                                        apps::LoadIconCallback callback,
                                        IconValuePtr icon_value) {
  if (icon_type == apps::IconType::kCompressed) {
    ConvertUncompressedIconToCompressedIcon(std::move(icon_value),
                                            std::move(callback));
    return;
  }

  std::move(callback).Run(std::move(icon_value));
}

}  // namespace apps
