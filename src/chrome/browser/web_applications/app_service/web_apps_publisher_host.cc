// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_publisher_host.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/one_shot_event.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

using apps::IconEffects;

namespace {

using LaunchResultCallback =
    base::OnceCallback<void(::crosapi::mojom::LaunchResultPtr)>;

void ReturnLaunchResult(Profile* profile,
                        content::WebContents* web_contents,
                        LaunchResultCallback callback) {
  // TODO(crbug.com/1144877): Run callback when the window is ready.
  auto* app_instance_tracker =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppInstanceTracker();
  auto launch_result = crosapi::mojom::LaunchResult::New();
  if (app_instance_tracker) {
    launch_result->instance_id =
        app_instance_tracker->GetAppInstance(web_contents)->id;
  } else {
    // TODO(crbug.com/1144877): This part of code should not be reached
    // after the instance tracker flag is turn on. Replaced with DCHECK when
    // the app instance tracker flag is turned on.
    launch_result->instance_id = base::UnguessableToken::Create();
  }
  std::move(callback).Run(std::move(launch_result));
}

}  // namespace

namespace web_app {

WebAppsPublisherHost::WebAppsPublisherHost(Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::GetForWebApps(profile)),
      publisher_helper_(profile,
                        provider_,
                        apps::mojom::AppType::kWeb,
                        this,
                        /*observe_media_requests=*/true) {
  DCHECK(provider_);
}

WebAppsPublisherHost::~WebAppsPublisherHost() = default;

void WebAppsPublisherHost::Init() {
  if (!remote_publisher_) {
    auto* service = chromeos::LacrosService::Get();
    if (!service) {
      return;
    }
    if (!service->IsAvailable<crosapi::mojom::AppPublisher>()) {
      return;
    }
    if (!service->init_params()->web_apps_enabled) {
      return;
    }

    remote_publisher_version_ =
        service->GetInterfaceVersion(crosapi::mojom::AppPublisher::Uuid_);

    if (remote_publisher_version_ <
        int{crosapi::mojom::AppPublisher::MethodMinVersions::
                kRegisterAppControllerMinVersion}) {
      LOG(WARNING) << "Ash AppPublisher version " << remote_publisher_version_
                   << " does not support RegisterAppController().";
      return;
    }

    service->GetRemote<crosapi::mojom::AppPublisher>()->RegisterAppController(
        receiver_.BindNewPipeAndPassRemoteWithVersion());
    remote_publisher_ =
        service->GetRemote<crosapi::mojom::AppPublisher>().get();
  }

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsPublisherHost::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

void WebAppsPublisherHost::Shutdown() {
  publisher_helper().Shutdown();
}

WebAppRegistrar& WebAppsPublisherHost::registrar() const {
  return provider_->registrar();
}

void WebAppsPublisherHost::SetPublisherForTesting(
    crosapi::mojom::AppPublisher* publisher) {
  remote_publisher_ = publisher;
  // Set the publisher version to the newest version for testing.
  remote_publisher_version_ = crosapi::mojom::AppPublisher::Version_;
}

void WebAppsPublisherHost::OnReady() {
  if (!remote_publisher_) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  for (const WebApp& web_app : registrar().GetApps()) {
    apps.push_back(publisher_helper().ConvertWebApp(&web_app));
  }
  PublishWebApps(std::move(apps));
}

void WebAppsPublisherHost::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

void WebAppsPublisherHost::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebAppsPublisherHost::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebAppsPublisherHost::LoadIcon(const std::string& app_id,
                                    apps::mojom::IconKeyPtr icon_key,
                                    apps::mojom::IconType icon_type,
                                    int32_t size_hint_in_dip,
                                    LoadIconCallback callback) {
  publisher_helper().LoadIcon(
      app_id, std::move(icon_key), std::move(icon_type), size_hint_in_dip,
      /*allow_placeholder_icon=*/false, std::move(callback));
}

content::WebContents* WebAppsPublisherHost::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  return publisher_helper().Launch(
      app_id, event_flags, std::move(launch_source), std::move(window_info));
}

content::WebContents* WebAppsPublisherHost::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  return publisher_helper().LaunchAppWithFiles(
      app_id, event_flags, std::move(launch_source), std::move(file_paths));
}

content::WebContents* WebAppsPublisherHost::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  return publisher_helper().LaunchAppWithIntent(
      app_id, event_flags, std::move(intent), std::move(launch_source),
      std::move(window_info));
}

void WebAppsPublisherHost::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

void WebAppsPublisherHost::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebAppsPublisherHost::SetWindowMode(const std::string& app_id,
                                         apps::mojom::WindowMode window_mode) {
  return publisher_helper().SetWindowMode(app_id, window_mode);
}

void WebAppsPublisherHost::GetMenuModel(const std::string& app_id,
                                        GetMenuModelCallback callback) {
  const WebApp* web_app = GetWebApp(app_id);
  auto menu_items = crosapi::mojom::MenuItems::New();
  if (!web_app) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (!web_app->shortcuts_menu_item_infos().empty()) {
    provider_->icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindOnce(&WebAppsPublisherHost::OnShortcutsMenuIconsRead,
                               weak_ptr_factory_.GetWeakPtr(), app_id,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebAppsPublisherHost::ExecuteContextMenuCommand(
    const std::string& app_id,
    const std::string& id,
    ExecuteContextMenuCommandCallback callback) {
  auto* web_contents = publisher_helper().ExecuteContextMenuCommand(
      app_id, id, display::kDefaultDisplayId);

  ReturnLaunchResult(profile_, web_contents, std::move(callback));
}

void WebAppsPublisherHost::StopApp(const std::string& app_id) {
  publisher_helper().StopApp(app_id);
}

// TODO(crbug.com/1144877): Clean up the multiple launch interfaces and remove
// duplicated code.
void WebAppsPublisherHost::Launch(crosapi::mojom::LaunchParamsPtr launch_params,
                                  LaunchCallback callback) {
  content::WebContents* web_contents = nullptr;
  if (launch_params->intent) {
    if (!profile_) {
      ReturnLaunchResult(profile_, nullptr, std::move(callback));
      return;
    }

    web_contents = publisher_helper().MaybeNavigateExistingWindow(
        launch_params->app_id, launch_params->intent->url);
    if (web_contents) {
      ReturnLaunchResult(profile_, web_contents, std::move(callback));
      return;
    }
    auto params = apps::CreateAppLaunchParamsForIntent(
        launch_params->app_id, ui::EF_NONE,
        apps::GetAppLaunchSource(launch_params->launch_source),
        display::kDefaultDisplayId,
        ConvertDisplayModeToAppLaunchContainer(
            registrar().GetAppEffectiveDisplayMode(launch_params->app_id)),
        apps_util::ConvertCrosapiToAppServiceIntent(launch_params->intent,
                                                    profile_));
    if (launch_params->intent->files.has_value()) {
      for (const auto& file : launch_params->intent->files.value()) {
        params.launch_files.push_back(file->file_path);
      }
    }
    web_contents = publisher_helper().LaunchAppWithParams(std::move(params));
  } else {
    web_contents =
        publisher_helper().Launch(launch_params->app_id, ui::EF_NONE,
                                  launch_params->launch_source, nullptr);
  }

  ReturnLaunchResult(profile_, web_contents, std::move(callback));
}

void WebAppsPublisherHost::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    crosapi::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(crosapi::mojom::MenuItems::New());
    return;
  }

  size_t menu_item_index = 0;

  for (const WebApplicationShortcutsMenuItemInfo& menu_item_info :
       web_app->shortcuts_menu_item_infos()) {
    const std::map<SquareSizePx, SkBitmap>* menu_item_icon_bitmaps = nullptr;
    if (menu_item_index < shortcuts_menu_icon_bitmaps.size()) {
      // We prefer |MASKABLE| icons, but fall back to icons with purpose |ANY|.
      menu_item_icon_bitmaps =
          &shortcuts_menu_icon_bitmaps[menu_item_index].maskable;
      if (menu_item_icon_bitmaps->empty()) {
        menu_item_icon_bitmaps =
            &shortcuts_menu_icon_bitmaps[menu_item_index].any;
      }
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      icon = apps::ConvertIconBitmapsToImageSkia(
          *menu_item_icon_bitmaps,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    auto menu_item = crosapi::mojom::MenuItem::New();
    menu_item->label = base::UTF16ToUTF8(menu_item_info.name);
    menu_item->image = icon;
    std::string shortcut_id = publisher_helper().GenerateShortcutId();
    publisher_helper().StoreShortcutId(shortcut_id, menu_item_info);
    menu_item->id = shortcut_id;
    menu_items->items.push_back(std::move(menu_item));

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

const WebApp* WebAppsPublisherHost::GetWebApp(const AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

void WebAppsPublisherHost::PublishWebApps(
    std::vector<apps::mojom::AppPtr> apps) {
  if (!remote_publisher_) {
    return;
  }

  if (remote_publisher_version_ <
      int{crosapi::mojom::AppPublisher::MethodMinVersions::kOnAppsMinVersion}) {
    LOG(WARNING) << "Ash AppPublisher version " << remote_publisher_version_
                 << " does not support OnApps().";
    return;
  }

  remote_publisher_->OnApps(std::move(apps));
}

void WebAppsPublisherHost::PublishWebApp(apps::mojom::AppPtr app) {
  if (!remote_publisher_) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(std::move(app));
  PublishWebApps(std::move(apps));
}

void WebAppsPublisherHost::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (!remote_publisher_) {
    return;
  }

  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<apps::mojom::CapabilityAccessPtr> capability_accesses;
  auto capability_access = apps::mojom::CapabilityAccess::New();
  capability_access->app_id = app_id;

  if (accessing_camera.has_value()) {
    capability_access->camera = accessing_camera.value()
                                    ? apps::mojom::OptionalBool::kTrue
                                    : apps::mojom::OptionalBool::kFalse;
  }

  if (accessing_microphone.has_value()) {
    capability_access->microphone = accessing_microphone.value()
                                        ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  }

  capability_accesses.push_back(std::move(capability_access));

  if (remote_publisher_version_ <
      int{crosapi::mojom::AppPublisher::MethodMinVersions::
              kOnCapabilityAccessesMinVersion}) {
    LOG(WARNING) << "Ash AppPublisher version " << remote_publisher_version_
                 << " does not support OnCapabilityAccesses().";
    return;
  }
  remote_publisher_->OnCapabilityAccesses(std::move(capability_accesses));
}

}  // namespace web_app
