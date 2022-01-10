// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"  // nogncheck
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/menu_item_constants.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#endif

using apps::IconEffects;

namespace web_app {

namespace {

apps::mojom::AppType GetWebAppType() {
// After moving the ordinary Web Apps to Lacros chrome, the remaining web
// apps in ash Chrome will be only System Web Apps. Change the app type
// to kSystemWeb for this case and the kWeb app type will be published from
// the publisher for Lacros web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled() && IsWebAppsCrosapiEnabled()) {
    return apps::mojom::AppType::kSystemWeb;
  }
#endif

  return apps::mojom::AppType::kWeb;
}

bool ShouldObserveMediaRequests() {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Make |app_id| the preferred app for handling |url|, without needing the user
// to choose the app through an intent picker. This function must be called
// after the corresponding intent filter has already been registered.
void AddDefaultPreferredApp(const std::string& app_id,
                            const GURL& url,
                            apps::mojom::AppService* app_service) {
  auto intent_filter = apps_util::CreateIntentFilterForUrlScope(url);
  app_service->AddPreferredApp(GetWebAppType(), app_id,
                               std::move(intent_filter),
                               /*intent=*/nullptr, /*from_publisher=*/true);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

WebApps::WebApps(apps::AppServiceProxy* proxy)
    : apps::AppPublisher(proxy),
      profile_(proxy->profile()),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_)),
      app_service_(proxy->AppService().get()),
      app_type_(GetWebAppType()),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      instance_registry_(&proxy->InstanceRegistry()),
#endif
      publisher_helper_(profile_,
                        provider_,
                        app_type_,
                        this,
                        ShouldObserveMediaRequests()) {
  Initialize(proxy->AppService());
}

WebApps::~WebApps() = default;

void WebApps::Shutdown() {
  if (provider_) {
    publisher_helper().Shutdown();
  }
}

const WebApp* WebApps::GetWebApp(const AppId& app_id) const {
  DCHECK(provider_);
  return provider_->registrar().GetAppById(app_id);
}

bool WebApps::Accepts(const std::string& app_id) const {
  return WebAppPublisherHelper::Accepts(app_id);
}

void WebApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  DCHECK(provider_);

  PublisherBase::Initialize(app_service, app_type_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::InitWebApps, AsWeakPtr()));
}

void WebApps::LoadIcon(const std::string& app_id,
                       const apps::IconKey& icon_key,
                       apps::IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       apps::LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                              std::move(callback));
}

void WebApps::LaunchAppWithParams(apps::AppLaunchParams&& params,
                                  apps::LaunchCallback callback) {
  publisher_helper().LaunchAppWithParams(std::move(params));
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(apps::LaunchResult());
}

void WebApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::StartPublishingWebApps, AsWeakPtr(),
                                std::move(subscriber_remote)));
}

void WebApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  if (!icon_key) {
    // On failure, we still run the callback, with an empty IconValue.
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  std::unique_ptr<apps::IconKey> key =
      apps::ConvertMojomIconKeyToIconKey(icon_key);
  publisher_helper().LoadIcon(
      app_id, *key, apps::ConvertMojomIconTypeToIconType(icon_type),
      size_hint_in_dip,
      apps::IconValueToMojomIconValueCallback(std::move(callback)));
}

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().Launch(app_id, event_flags, launch_source,
                            std::move(window_info));
}

void WebApps::LaunchAppWithFiles(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::FilePathsPtr file_paths) {
  publisher_helper().LaunchAppWithFiles(app_id, event_flags, launch_source,
                                        std::move(file_paths));
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  apps::mojom::WindowInfoPtr window_info,
                                  LaunchAppWithIntentCallback callback) {
  publisher_helper().LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                         launch_source, std::move(window_info),
                                         std::move(callback));
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

void WebApps::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebApps::PublishWebApps(std::vector<apps::mojom::AppPtr> mojom_apps) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const WebApp* web_app = GetWebApp(ash::kChromeUITrustedProjectorSwaAppId);
  if (web_app) {
    AddDefaultPreferredApp(ash::kChromeUITrustedProjectorSwaAppId,
                           GURL(ash::kChromeUIUntrustedProjectorPwaUrl),
                           app_service_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (mojom_apps.empty()) {
    return;
  }

  std::vector<std::unique_ptr<apps::App>> apps;
  for (apps::mojom::AppPtr& app : mojom_apps) {
    apps.push_back(apps::ConvertMojomAppToApp(app));
  }

  apps::AppPublisher::Publish(std::move(apps));

  const bool should_notify_initialized = false;
  if (subscribers_.size() == 1) {
    auto& subscriber = *subscribers_.begin();
    subscriber->OnApps(std::move(mojom_apps), app_type(),
                       should_notify_initialized);
    return;
  }
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> cloned_apps;
    for (const auto& app : mojom_apps)
      cloned_apps.push_back(app.Clone());
    subscriber->OnApps(std::move(cloned_apps), app_type(),
                       should_notify_initialized);
  }
}

void WebApps::PublishWebApp(apps::mojom::AppPtr app) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app->app_id == ash::kChromeUITrustedProjectorSwaAppId) {
    // After OOBE, PublishWebApps() above could execute before the intent filter
    // has been registered. Since we need to call AddDefaultPreferredApp() after
    // the intent filter has been registered, we need this call for the OOBE
    // case.
    AddDefaultPreferredApp(ash::kChromeUITrustedProjectorSwaAppId,
                           GURL(ash::kChromeUIUntrustedProjectorPwaUrl),
                           app_service_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  apps::AppPublisher::Publish(apps::ConvertMojomAppToApp(app));
  PublisherBase::Publish(std::move(app), subscribers_);
}

void WebApps::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  ModifyCapabilityAccess(subscribers_, app_id, std::move(accessing_camera),
                         std::move(accessing_microphone));
}

std::vector<std::unique_ptr<apps::App>> WebApps::CreateWebApps() {
  DCHECK(provider_);

  std::vector<std::unique_ptr<apps::App>> apps;
  for (const WebApp& web_app : provider_->registrar().GetApps()) {
    if (Accepts(web_app.app_id())) {
      apps.push_back(publisher_helper().CreateWebApp(&web_app));
    }
  }
  return apps;
}

void WebApps::ConvertWebApps(std::vector<apps::mojom::AppPtr>* apps_out) {
  DCHECK(provider_);
  if (publisher_helper().IsShuttingDown()) {
    return;
  }

  for (const WebApp& web_app : provider_->registrar().GetApps()) {
    if (Accepts(web_app.app_id())) {
      apps_out->push_back(publisher_helper().ConvertWebApp(&web_app));
    }
  }
}

void WebApps::InitWebApps() {
  RegisterPublisher(apps::ConvertMojomAppTypToAppType(app_type_));

  std::vector<std::unique_ptr<apps::App>> apps = CreateWebApps();
  if (apps.empty()) {
    return;
  }
  apps::AppPublisher::Publish(std::move(apps));
}

void WebApps::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(&apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), app_type_,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApps::Uninstall(const std::string& app_id,
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

void WebApps::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebApps::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebApps::StopApp(const std::string& app_id) {
  publisher_helper().StopApp(app_id);
}

void WebApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  const auto* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();
  if (web_app->IsSystemApp()) {
    DCHECK(web_app->client_data().system_web_app_data.has_value());
    SystemAppType swa_type =
        web_app->client_data().system_web_app_data->system_app_type;

    auto* system_app = WebAppProvider::GetForSystemWebApps(profile())
                           ->system_web_app_manager()
                           .GetSystemApp(swa_type);
    if (system_app && system_app->ShouldShowNewWindowMenuOption()) {
      apps::AddCommandItem(ash::MENU_OPEN_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, &menu_items);
    }
  } else {
    apps::CreateOpenNewSubmenu(menu_type,
                               publisher_helper().GetWindowMode(app_id) ==
                                       apps::mojom::WindowMode::kBrowser
                                   ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                                   : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                               &menu_items);
  }

  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    DCHECK(base::FeatureList::IsEnabled(chromeos::features::kTerminalSSH));
    crostini::AddTerminalMenuItems(profile_, &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      instance_registry_->ContainsAppId(app_id)) {
    apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                         &menu_items);
  }

  if (web_app->CanUserUninstallWebApp()) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (!web_app->IsSystemApp()) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }

  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    DCHECK(base::FeatureList::IsEnabled(chromeos::features::kTerminalSSH));
    crostini::AddTerminalMenuShortcuts(profile_, ash::LAUNCH_APP_SHORTCUT_FIRST,
                                       std::move(menu_items),
                                       std::move(callback));
  } else {
    GetAppShortcutMenuModel(app_id, std::move(menu_items), std::move(callback));
  }
}

void WebApps::GetAppShortcutMenuModel(const std::string& app_id,
                                      apps::mojom::MenuItemsPtr menu_items,
                                      GetMenuModelCallback callback) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  // Read shortcuts menu item icons from disk, if any.
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI) &&
      !web_app->shortcuts_menu_item_infos().empty()) {
    provider()->icon_manager().ReadAllShortcutsMenuIcons(
        app_id, base::BindOnce(&WebApps::OnShortcutsMenuIconsRead,
                               base::AsWeakPtr<WebApps>(this), app_id,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebApps::OnShortcutsMenuIconsRead(
    const std::string& app_id,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);

  size_t menu_item_index = 0;

  for (const WebAppShortcutsMenuItemInfo& menu_item_info :
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

    if (menu_item_index != 0) {
      apps::AddSeparator(ui::PADDED_SEPARATOR, &menu_items);
    }

    gfx::ImageSkia icon;
    if (menu_item_icon_bitmaps) {
      IconEffects icon_effects = IconEffects::kNone;

      // We apply masking to each shortcut icon, regardless if the purpose is
      // |MASKABLE| or |ANY|.
      icon_effects = apps::kCrOsStandardBackground | apps::kCrOsStandardMask;

      icon = ConvertSquareBitmapsToImageSkia(
          *menu_item_icon_bitmaps, icon_effects,
          /*size_hint_in_dip=*/apps::kAppShortcutIconSizeDip);
    }

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + menu_item_index;

    const std::string label = base::UTF16ToUTF8(menu_item_info.name);
    std::string shortcut_id = publisher_helper().GenerateShortcutId();
    publisher_helper().StoreShortcutId(shortcut_id, menu_item_info);

    apps::AddShortcutCommandItem(command_id, shortcut_id, label, icon,
                                 &menu_items);

    ++menu_item_index;
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebApps::ExecuteContextMenuCommand(const std::string& app_id,
                                        int command_id,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    DCHECK(base::FeatureList::IsEnabled(chromeos::features::kTerminalSSH));
    if (crostini::ExecuteTerminalMenuShortcutCommand(profile_, shortcut_id,
                                                     display_id)) {
      return;
    }
  }
  publisher_helper().ExecuteContextMenuCommand(app_id, shortcut_id, display_id);
}

void WebApps::SetWindowMode(const std::string& app_id,
                            apps::mojom::WindowMode window_mode) {
  publisher_helper().SetWindowMode(app_id, window_mode);
}
#endif

}  // namespace web_app
