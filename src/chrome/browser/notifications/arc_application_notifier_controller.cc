// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/arc_application_notifier_controller.h"

#include <set>

#include "ash/public/cpp/notifier_metadata.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/notifications/notifier_dataset.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/permission_utils.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace arc {

ArcApplicationNotifierController::ArcApplicationNotifierController(
    NotifierController::Observer* observer)
    : observer_(observer) {}

ArcApplicationNotifierController::~ArcApplicationNotifierController() = default;

std::vector<ash::NotifierMetadata>
ArcApplicationNotifierController::GetNotifierList(Profile* profile) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  // In Guest mode, it can be called but there's no ARC apps to return.
  if (profile->IsOffTheRecord())
    return std::vector<ash::NotifierMetadata>();

  last_used_profile_ = profile;
  apps::AppServiceProxy* service =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  Observe(&(service->AppRegistryCache()));

  package_to_app_ids_.clear();
  std::vector<NotifierDataset> notifier_dataset;

  service->AppRegistryCache().ForEachApp([&notifier_dataset](
                                             const apps::AppUpdate& update) {
    if (update.AppType() != apps::mojom::AppType::kArc)
      return;

    for (const auto& permission : update.Permissions()) {
      if (permission->permission_type !=
          apps::mojom::PermissionType::kNotifications) {
        continue;
      }
      DCHECK(permission->value->is_bool_value());
      // Do not include notifier metadata for system apps.
      if (update.InstallReason() == apps::mojom::InstallReason::kSystem) {
        return;
      }
      notifier_dataset.push_back(NotifierDataset{
          update.AppId() /*app_id*/, update.ShortName() /*app_name*/,
          update.PublisherId() /*publisher_id*/,
          permission->value->get_bool_value() /*enabled*/});
    }
  });

  std::vector<ash::NotifierMetadata> notifiers;
  for (auto& app_data : notifier_dataset) {
    // Handle packages having multiple launcher activities.
    if (package_to_app_ids_.count(app_data.publisher_id))
      continue;

    message_center::NotifierId notifier_id(
        message_center::NotifierType::ARC_APPLICATION, app_data.app_id);
    notifiers.emplace_back(notifier_id, base::UTF8ToUTF16(app_data.app_name),
                           app_data.enabled, false /* enforced */,
                           gfx::ImageSkia());
    package_to_app_ids_.insert(
        std::make_pair(app_data.publisher_id, app_data.app_id));
    CallLoadIcon(app_data.app_id, /*allow_placeholder_icon*/ true);
  }
  return notifiers;
}

void ArcApplicationNotifierController::SetNotifierEnabled(
    Profile* profile,
    const message_center::NotifierId& notifier_id,
    bool enabled) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  last_used_profile_ = profile;
  auto permission = apps::mojom::Permission::New();
  permission->permission_type = apps::mojom::PermissionType::kNotifications;
  permission->value = apps::mojom::PermissionValue::New();
  permission->value->set_bool_value(enabled);
  permission->is_managed = false;
  apps::AppServiceProxy* service =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  service->SetPermission(notifier_id.id, std::move(permission));
}

void ArcApplicationNotifierController::CallLoadIcon(
    const std::string& app_id,
    bool allow_placeholder_icon) {
  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      last_used_profile_));

  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    apps::AppServiceProxyFactory::GetForProfile(last_used_profile_)
        ->LoadIcon(apps::AppType::kArc, app_id, apps::IconType::kStandard,
                   message_center::kQuickSettingIconSizeInDp,
                   allow_placeholder_icon,
                   base::BindOnce(&ArcApplicationNotifierController::OnLoadIcon,
                                  weak_ptr_factory_.GetWeakPtr(), app_id));
  } else {
    apps::AppServiceProxyFactory::GetForProfile(last_used_profile_)
        ->LoadIcon(apps::mojom::AppType::kArc, app_id,
                   apps::mojom::IconType::kStandard,
                   message_center::kQuickSettingIconSizeInDp,
                   allow_placeholder_icon,
                   apps::MojomIconValueToIconValueCallback(base::BindOnce(
                       &ArcApplicationNotifierController::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), app_id)));
  }
}

void ArcApplicationNotifierController::OnLoadIcon(
    const std::string& app_id,
    apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  SetIcon(app_id, icon_value->uncompressed);
  if (icon_value->is_placeholder_icon)
    CallLoadIcon(app_id, /*allow_placeholder_icon*/ false);
}

void ArcApplicationNotifierController::SetIcon(const std::string& app_id,
                                               gfx::ImageSkia image) {
  observer_->OnIconImageUpdated(
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 app_id),
      image);
}

void ArcApplicationNotifierController::OnAppUpdate(
    const apps::AppUpdate& update) {
  if (!base::Contains(package_to_app_ids_, update.PublisherId()))
    return;

  if (update.PermissionsChanged()) {
    for (const auto& permission : update.Permissions()) {
      if (permission->permission_type ==
          apps::mojom::PermissionType::kNotifications) {
        message_center::NotifierId notifier_id(
            message_center::NotifierType::ARC_APPLICATION, update.AppId());
        observer_->OnNotifierEnabledChanged(
            notifier_id, apps_util::IsPermissionEnabled(permission->value));
      }
    }
  }

  if (update.IconKeyChanged())
    CallLoadIcon(update.AppId(), /*allow_placeholder_icon*/ true);
}

void ArcApplicationNotifierController::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

}  // namespace arc
