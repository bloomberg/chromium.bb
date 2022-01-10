// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace eche_app {

const char kEcheAppScreenLockNotifierId[] =
    "eche_app_notification_ids.screen_lock";
// The notification type from WebUI is CONNECTION_FAILED or CONNECTION_LOST
// allow users to retry.
const char kEcheAppRetryConnectionNotifierId[] =
    "eche_app_notification_ids.retry_connection";
// The notification type from WebUI without any actions need to do.
const char kEcheAppFromWebWithoudButtonNotifierId[] =
    "eche_app_notification_ids.from_web_without_button";

const char kEcheAppDisabledByPhoneNotifierId[] =
    "eche_app_notification_ids.disabled_by_phone";

// TODO(crbug.com/1241352): This should probably have a ?p=<FEATURE_NAME> at
// some point.
const char kEcheAppLearnMoreUrl[] = "https://support.google.com/chromebook";

namespace {

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const gfx::Image& icon,
    const message_center::RichNotificationData& rich_notification_data,
    message_center::NotificationDelegate* delegate) {
  return std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, std::u16string() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id),
      rich_notification_data, delegate);
}

}  // namespace

EcheAppNotificationController::EcheAppNotificationController(Profile* profile)
    : profile_(profile) {}

EcheAppNotificationController::~EcheAppNotificationController() {}
void EcheAppNotificationController::LaunchSettings() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2);
}

void EcheAppNotificationController::LaunchLearnMore() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
  NewWindowDelegate::GetInstance()->OpenUrl(GURL(kEcheAppLearnMoreUrl),
                                            /* from_user_interaction= */ true);
}

void EcheAppNotificationController::LaunchTryAgain() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
}

void EcheAppNotificationController::LaunchHelp() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
}

void EcheAppNotificationController::ShowNotificationFromWebUI(
    const absl::optional<std::u16string>& title,
    const absl::optional<std::u16string>& message,
    absl::variant<LaunchAppHelper::NotificationInfo::NotificationType,
                  mojom::WebNotificationType> type) {
  auto web_type = absl::get<mojom::WebNotificationType>(type);
  if (title && message) {
    if (web_type == mojom::WebNotificationType::CONNECTION_FAILED ||
        web_type == mojom::WebNotificationType::CONNECTION_LOST) {
      // TODO(guanrulee): Show the buttons once they're completed.
      ShowNotification(CreateNotification(
          kEcheAppRetryConnectionNotifierId, title.value(), message.value(),
          gfx::Image(), message_center::RichNotificationData(),
          new NotificationDelegate(kEcheAppRetryConnectionNotifierId,
                                   weak_ptr_factory_.GetWeakPtr())));
    } else {
      // No need to take the action.
      ShowNotification(CreateNotification(
          kEcheAppFromWebWithoudButtonNotifierId, title.value(),
          message.value(), gfx::Image(), message_center::RichNotificationData(),
          new NotificationDelegate(kEcheAppFromWebWithoudButtonNotifierId,
                                   weak_ptr_factory_.GetWeakPtr())));
    }
  } else {
    PA_LOG(ERROR)
        << "Cannot find the title or message to show the notification.";
  }
}

void EcheAppNotificationController::ShowScreenLockNotification(
    const absl::optional<std::u16string>& title) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_SETTINGS_BUTTON)));
  rich_notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_LEARN_MORE)));

  ShowNotification(CreateNotification(
      kEcheAppScreenLockNotifierId,
      l10n_util::GetStringFUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_TITLE,
                                 title.value()),
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_MESSAGE),
      gfx::Image(), rich_notification_data,
      new NotificationDelegate(kEcheAppScreenLockNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EcheAppNotificationController::ShowDisabledByPhoneNotification(
    const absl::optional<std::u16string>& title) {
  // No need to take the action.
  ShowNotification(CreateNotification(
      kEcheAppDisabledByPhoneNotifierId,
      l10n_util::GetStringFUTF16(
          IDS_ECHE_APP_DISABLED_BY_PHONE_NOTIFICATION_TITLE, title.value()),
      l10n_util::GetStringUTF16(
          IDS_ECHE_APP_DISABLED_BY_PHONE_NOTIFICATION_MESSAGE),
      gfx::Image(), message_center::RichNotificationData(),
      new NotificationDelegate(kEcheAppDisabledByPhoneNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EcheAppNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

EcheAppNotificationController::NotificationDelegate::NotificationDelegate(
    const std::string& notification_id,
    const base::WeakPtr<EcheAppNotificationController>& notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

EcheAppNotificationController::NotificationDelegate::~NotificationDelegate() {}
void EcheAppNotificationController::NotificationDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (!button_index)
    return;

  if (notification_id_ == kEcheAppScreenLockNotifierId) {
    if (*button_index == 0) {
      notification_controller_->LaunchSettings();
    } else {
      DCHECK_EQ(1, *button_index);
      notification_controller_->LaunchLearnMore();
    }
  } else if (notification_id_ == kEcheAppRetryConnectionNotifierId) {
    if (*button_index == 0) {
      notification_controller_->LaunchTryAgain();
    } else {
      DCHECK_EQ(1, *button_index);
      notification_controller_->LaunchHelp();
    }
  }
}

}  // namespace eche_app
}  // namespace ash
