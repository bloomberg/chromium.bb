// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/remote_apps.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

RemoteApps::RemoteApps(AppServiceProxy* proxy, Delegate* delegate)
    : AppPublisher(proxy), profile_(proxy->profile()), delegate_(delegate) {
  DCHECK(delegate);

  mojo::Remote<mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  PublisherBase::Initialize(app_service, mojom::AppType::kRemote);
}

RemoteApps::~RemoteApps() = default;

void RemoteApps::AddApp(const ash::RemoteAppsModel::AppInfo& info) {
  mojom::AppPtr mojom_app = Convert(info);
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app = CreateApp(info);
  AppPublisher::Publish(std::move(app));
}

void RemoteApps::UpdateAppIcon(const std::string& app_id) {
  mojom::AppPtr mojom_app = mojom::App::New();
  mojom_app->app_type = mojom::AppType::kRemote;
  mojom_app->app_id = app_id;
  mojom_app->icon_key = icon_key_factory_.MakeIconKey(IconEffects::kNone);
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app = std::make_unique<App>(AppType::kRemote, app_id);
  app->icon_key =
      std::move(*icon_key_factory_.CreateIconKey(IconEffects::kNone));
  AppPublisher::Publish(std::move(app));
}

void RemoteApps::DeleteApp(const std::string& app_id) {
  mojom::AppPtr mojom_app = mojom::App::New();
  mojom_app->app_type = mojom::AppType::kRemote;
  mojom_app->app_id = app_id;
  mojom_app->readiness = mojom::Readiness::kUninstalledByUser;
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app = std::make_unique<App>(AppType::kRemote, app_id);
  app->readiness = Readiness::kUninstalledByUser;
  AppPublisher::Publish(std::move(app));
}

std::unique_ptr<App> RemoteApps::CreateApp(
    const ash::RemoteAppsModel::AppInfo& info) {
  std::unique_ptr<App> app = AppPublisher::MakeApp(
      AppType::kRemote, info.id, Readiness::kReady, info.name);
  app->icon_key =
      std::move(*icon_key_factory_.CreateIconKey(IconEffects::kNone));
  return app;
}

apps::mojom::AppPtr RemoteApps::Convert(
    const ash::RemoteAppsModel::AppInfo& info) {
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      mojom::AppType::kRemote, info.id, mojom::Readiness::kReady, info.name,
      mojom::InstallReason::kUser);
  app->show_in_launcher = mojom::OptionalBool::kTrue;
  app->show_in_management = mojom::OptionalBool::kFalse;
  app->show_in_search = mojom::OptionalBool::kTrue;
  app->show_in_shelf = mojom::OptionalBool::kFalse;
  app->allow_uninstall = mojom::OptionalBool::kFalse;
  app->handles_intents = mojom::OptionalBool::kTrue;
  app->icon_key = icon_key_factory_.MakeIconKey(IconEffects::kNone);
  return app;
}

void RemoteApps::Initialize() {
  RegisterPublisher(AppType::kRemote);
}

void RemoteApps::LoadIcon(const std::string& app_id,
                          const IconKey& icon_key,
                          IconType icon_type,
                          int32_t size_hint_in_dip,
                          bool allow_placeholder_icon,
                          apps::LoadIconCallback callback) {
  DCHECK_NE(icon_type, IconType::kCompressed)
      << "Remote apps cannot provide uncompressed icons";

  bool is_placeholder_icon = false;
  gfx::ImageSkia icon_image = delegate_->GetIcon(app_id);
  if (icon_image.isNull() && allow_placeholder_icon) {
    is_placeholder_icon = true;
    icon_image = delegate_->GetPlaceholderIcon(app_id, size_hint_in_dip);
  }

  if (icon_image.isNull()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  auto icon = std::make_unique<IconValue>();
  icon->icon_type = icon_type;
  icon->uncompressed = icon_image;
  icon->is_placeholder_icon = is_placeholder_icon;
  IconEffects icon_effects = (icon_type == IconType::kStandard)
                                 ? IconEffects::kCrOsStandardIcon
                                 : IconEffects::kResizeAndPad;
  ApplyIconEffects(icon_effects, size_hint_in_dip, std::move(icon),
                   std::move(callback));
}

void RemoteApps::LaunchAppWithParams(AppLaunchParams&& params,
                                     LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, apps::mojom::LaunchSource::kUnknown,
         nullptr);
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void RemoteApps::Connect(
    mojo::PendingRemote<mojom::Subscriber> subscriber_remote,
    mojom::ConnectOptionsPtr opts) {
  mojo::Remote<mojom::Subscriber> subscriber(std::move(subscriber_remote));

  std::vector<mojom::AppPtr> apps;
  for (const auto& entry : delegate_->GetApps()) {
    apps.push_back(Convert(entry.second));
  }
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kRemote,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

void RemoteApps::LoadIcon(const std::string& app_id,
                          mojom::IconKeyPtr icon_key,
                          mojom::IconType icon_type,
                          int32_t size_hint_in_dip,
                          bool allow_placeholder_icon,
                          LoadIconCallback callback) {
  DCHECK(icon_type != mojom::IconType::kCompressed)
      << "Remote app should not be shown in management";

  bool is_placeholder_icon = false;
  gfx::ImageSkia icon_image = delegate_->GetIcon(app_id);
  if (icon_image.isNull() && allow_placeholder_icon) {
    is_placeholder_icon = true;
    icon_image = delegate_->GetPlaceholderIcon(app_id, size_hint_in_dip);
  }

  if (icon_image.isNull()) {
    std::move(callback).Run(mojom::IconValue::New());
    return;
  }

  auto icon = std::make_unique<IconValue>();
  icon->icon_type = ConvertMojomIconTypeToIconType(icon_type);
  icon->uncompressed = icon_image;
  icon->is_placeholder_icon = is_placeholder_icon;
  IconEffects icon_effects = (icon_type == mojom::IconType::kStandard)
                                 ? IconEffects::kCrOsStandardIcon
                                 : IconEffects::kResizeAndPad;
  apps::ApplyIconEffects(
      icon_effects, size_hint_in_dip, std::move(icon),
      IconValueToMojomIconValueCallback(std::move(callback)));
}

void RemoteApps::Launch(const std::string& app_id,
                        int32_t event_flags,
                        mojom::LaunchSource launch_source,
                        apps::mojom::WindowInfoPtr window_info) {
  delegate_->LaunchApp(app_id);
}

void RemoteApps::GetMenuModel(const std::string& app_id,
                              mojom::MenuType menu_type,
                              int64_t display_id,
                              GetMenuModelCallback callback) {
  std::move(callback).Run(delegate_->GetMenuModel(app_id));
}

}  // namespace apps
