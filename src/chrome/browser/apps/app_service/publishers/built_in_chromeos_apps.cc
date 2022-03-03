// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/built_in_chromeos_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/keyboard_shortcut_viewer.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<apps::App> CreateApp(
    const app_list::InternalApp& internal_app) {
  if ((internal_app.app_id == nullptr) ||
      (internal_app.name_string_resource_id == 0) ||
      (internal_app.icon_resource_id <= 0)) {
    return nullptr;
  }

  std::unique_ptr<apps::App> app = apps::AppPublisher::MakeApp(
      apps::AppType::kBuiltIn, internal_app.app_id, apps::Readiness::kReady,
      l10n_util::GetStringUTF8(internal_app.name_string_resource_id));

  app->icon_key =
      apps::IconKey(apps::IconKey::kDoesNotChangeOverTime,
                    internal_app.icon_resource_id, apps::IconEffects::kNone);

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr Convert(const app_list::InternalApp& internal_app) {
  if ((internal_app.app_id == nullptr) ||
      (internal_app.name_string_resource_id == 0) ||
      (internal_app.icon_resource_id <= 0)) {
    return apps::mojom::AppPtr();
  }

  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kBuiltIn, internal_app.app_id,
      apps::mojom::Readiness::kReady,
      l10n_util::GetStringUTF8(internal_app.name_string_resource_id),
      apps::mojom::InstallReason::kSystem);

  app->install_source = apps::mojom::InstallSource::kSystem;

  if (internal_app.searchable_string_resource_id != 0) {
    app->additional_search_terms.push_back(
        l10n_util::GetStringUTF8(internal_app.searchable_string_resource_id));
  }

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      internal_app.icon_resource_id, apps::IconEffects::kNone);

  app->recommendable = internal_app.recommendable
                           ? apps::mojom::OptionalBool::kTrue
                           : apps::mojom::OptionalBool::kFalse;
  app->searchable = internal_app.searchable ? apps::mojom::OptionalBool::kTrue
                                            : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = internal_app.show_in_launcher
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
  app->show_in_shelf = app->show_in_search =
      internal_app.searchable ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
  app->show_in_management = apps::mojom::OptionalBool::kFalse;
  app->allow_uninstall = apps::mojom::OptionalBool::kFalse;
  app->handles_intents = app->show_in_launcher;

  return app;
}

}  // namespace

namespace apps {

BuiltInChromeOsApps::BuiltInChromeOsApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {}

BuiltInChromeOsApps::~BuiltInChromeOsApps() = default;

void BuiltInChromeOsApps::Initialize() {
  PublisherBase::Initialize(proxy()->AppService(),
                            apps::mojom::AppType::kBuiltIn);

  RegisterPublisher(AppType::kBuiltIn);

  std::vector<std::unique_ptr<App>> apps;
  for (const auto& internal_app : app_list::GetInternalAppList(profile_)) {
    std::unique_ptr<App> app = CreateApp(internal_app);
    if (app) {
      apps.push_back(std::move(app));
    }
  }
  AppPublisher::Publish(std::move(apps));
}

void BuiltInChromeOsApps::LoadIcon(const std::string& app_id,
                                   const IconKey& icon_key,
                                   IconType icon_type,
                                   int32_t size_hint_in_dip,
                                   bool allow_placeholder_icon,
                                   apps::LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  if (icon_key.resource_id != IconKey::kInvalidResourceId) {
    LoadIconFromResource(
        icon_type, size_hint_in_dip, icon_key.resource_id, is_placeholder_icon,
        static_cast<IconEffects>(icon_key.icon_effects), std::move(callback));
    return;
  }
  // On failure, we still run the callback, with an empty IconValue.
  std::move(callback).Run(std::make_unique<IconValue>());
}

void BuiltInChromeOsApps::LaunchAppWithParams(AppLaunchParams&& params,
                                              LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, apps::mojom::LaunchSource::kUnknown,
         nullptr);
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void BuiltInChromeOsApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  if (profile_) {
    // TODO(crbug.com/826982): move source of truth for built-in apps from
    // ui/app_list to here when the AppService feature is enabled by default.
    for (const auto& internal_app : app_list::GetInternalAppList(profile_)) {
      apps::mojom::AppPtr app = Convert(internal_app);
      if (!app.is_null()) {
        apps.push_back(std::move(app));
      }
    }
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kBuiltIn,
                     true /* should_notify_initialized */);

  // Unlike other apps::mojom::Publisher implementations, we don't need to
  // retain the subscriber (e.g. add it to a
  // mojo::RemoteSet<apps::mojom::Subscriber> subscribers_) after this
  // function returns. The list of built-in Chrome OS apps is fixed for the
  // lifetime of the Chrome OS session. There won't be any further updates.
}

void BuiltInChromeOsApps::LoadIcon(const std::string& app_id,
                                   apps::mojom::IconKeyPtr icon_key,
                                   apps::mojom::IconType icon_type,
                                   int32_t size_hint_in_dip,
                                   bool allow_placeholder_icon,
                                   LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  if (icon_key &&
      (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId)) {
    LoadIconFromResource(
        ConvertMojomIconTypeToIconType(icon_type), size_hint_in_dip,
        icon_key->resource_id, is_placeholder_icon,
        static_cast<IconEffects>(icon_key->icon_effects),
        IconValueToMojomIconValueCallback(std::move(callback)));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void BuiltInChromeOsApps::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::WindowInfoPtr window_info) {
  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    ash::ToggleKeyboardShortcutViewer();
  }
}

void BuiltInChromeOsApps::GetMenuModel(const std::string& app_id,
                                       apps::mojom::MenuType menu_type,
                                       int64_t display_id,
                                       GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   &menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

}  // namespace apps
