// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_recent_app_click_handler.h"

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"

namespace ash {
namespace eche_app {

EcheRecentAppClickHandler::EcheRecentAppClickHandler(
    phonehub::PhoneHubManager* phone_hub_manager,
    FeatureStatusProvider* feature_status_provider,
    LaunchAppHelper* launch_app_helper)
    : feature_status_provider_(feature_status_provider),
      launch_app_helper_(launch_app_helper) {
  notification_handler_ =
      phone_hub_manager->GetNotificationInteractionHandler();
  recent_apps_handler_ = phone_hub_manager->GetRecentAppsInteractionHandler();
  feature_status_provider_->AddObserver(this);

  if (notification_handler_ && recent_apps_handler_ &&
      IsClickable(feature_status_provider_->GetStatus())) {
    notification_handler_->AddNotificationClickHandler(this);
    recent_apps_handler_->AddRecentAppClickObserver(this);
    is_click_handler_set_ = true;
  }
}

EcheRecentAppClickHandler::~EcheRecentAppClickHandler() {
  feature_status_provider_->RemoveObserver(this);
  if (notification_handler_)
    notification_handler_->RemoveNotificationClickHandler(this);
  if (recent_apps_handler_)
    recent_apps_handler_->RemoveRecentAppClickObserver(this);
}

void EcheRecentAppClickHandler::HandleNotificationClick(
    int64_t notification_id,
    const phonehub::Notification::AppMetadata& app_metadata) {
  if (recent_apps_handler_ && launch_app_helper_->IsAppLaunchAllowed()) {
    recent_apps_handler_->NotifyRecentAppAddedOrUpdated(app_metadata,
                                                        base::Time::Now());
  }
}

void EcheRecentAppClickHandler::OnRecentAppClicked(
    const phonehub::Notification::AppMetadata& app_metadata) {
  if (launch_app_helper_->IsAppLaunchAllowed()) {
    launch_app_helper_->LaunchEcheApp(
        /*notification_id=*/absl::nullopt, app_metadata.package_name,
        app_metadata.visible_app_name, app_metadata.user_id);
  } else {
    launch_app_helper_->ShowNotification(
        /* title= */ absl::nullopt, /* message= */ absl::nullopt,
        std::make_unique<LaunchAppHelper::NotificationInfo>(
            LaunchAppHelper::NotificationInfo::Category::kNative,
            LaunchAppHelper::NotificationInfo::NotificationType::kScreenLock));
  }
}

void EcheRecentAppClickHandler::OnFeatureStatusChanged() {
  if (!notification_handler_ || !recent_apps_handler_) {
    return;
  }
  bool clickable = IsClickable(feature_status_provider_->GetStatus());
  if (!is_click_handler_set_ && clickable) {
    notification_handler_->AddNotificationClickHandler(this);
    recent_apps_handler_->AddRecentAppClickObserver(this);
    is_click_handler_set_ = true;
  } else if (is_click_handler_set_ && !clickable) {
    // This handler doesn't run |CloseEcheApp| since it possibly
    // closes twice on EcheNotificationClickHandler and here.
    notification_handler_->RemoveNotificationClickHandler(this);
    recent_apps_handler_->RemoveRecentAppClickObserver(this);
    is_click_handler_set_ = false;
  }
}

bool EcheRecentAppClickHandler::IsClickable(FeatureStatus status) {
  return status == FeatureStatus::kDisconnected ||
         status == FeatureStatus::kConnecting ||
         status == FeatureStatus::kConnected;
}

}  // namespace eche_app
}  // namespace ash
