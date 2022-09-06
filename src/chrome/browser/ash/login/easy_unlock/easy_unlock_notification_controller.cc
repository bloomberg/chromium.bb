// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_notification_controller.h"

#include "ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "ash/components/proximity_auth/screenlock_bridge.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {
namespace {

const char kEasyUnlockChromebookAddedNotifierId[] =
    "easyunlock_notification_ids.chromebook_added";

const char kEasyUnlockPairingChangeNotifierId[] =
    "easyunlock_notification_ids.pairing_change";

const char kEasyUnlockPairingChangeAppliedNotifierId[] =
    "easyunlock_notification_ids.pairing_change_applied";

const char kSmartLockSignInRemovedNotifierId[] =
    "easyunlock_notification_ids.sign_in_removed";

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const ash::NotificationCatalogName& catalog_name,
    const std::u16string& title,
    const std::u16string& message,
    const ui::ImageModel& icon,
    const message_center::RichNotificationData& rich_notification_data,
    message_center::NotificationDelegate* delegate) {
  return std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, std::u16string() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id, catalog_name),
      rich_notification_data, delegate);
}

}  // namespace

EasyUnlockNotificationController::EasyUnlockNotificationController(
    Profile* profile)
    : profile_(profile) {}

EasyUnlockNotificationController::~EasyUnlockNotificationController() {}

// static
bool EasyUnlockNotificationController::ShouldShowSignInRemovedNotification(
    Profile* profile) {
  if (!profile->GetPrefs()->GetBoolean(
          proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled))
    return false;

  if (!base::FeatureList::IsEnabled(ash::features::kSmartLockSignInRemoved))
    return false;

  if (profile->GetPrefs()->GetBoolean(
          prefs::kHasSeenSmartLockSignInRemovedNotification))
    return false;

  return true;
}

void EasyUnlockNotificationController::ShowSignInRemovedNotification() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kHasSeenSmartLockSignInRemovedNotification,
                           true);

  ShowNotification(CreateNotification(
      kSmartLockSignInRemovedNotifierId,
      ash::NotificationCatalogName::kEasyUnlockSmartLockSignInRemoved,
      l10n_util::GetStringUTF16(
          IDS_SMART_LOCK_SIGN_IN_REMOVED_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_SMART_LOCK_SIGN_IN_REMOVED_NOTIFICATION_MESSAGE),
      ui::ImageModel(), {},
      new NotificationDelegate(kSmartLockSignInRemovedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowChromebookAddedNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockChromebookAddedNotifierId,
      ash::NotificationCatalogName::kEasyUnlockChromebookAdded,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE,
          ui::GetChromeOSDeviceName()),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_NOTIFICATION_EASYUNLOCK_ENABLED)),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockChromebookAddedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowPairingChangeNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_UPDATE_BUTTON)));
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeNotifierId,
      ash::NotificationCatalogName::kEasyUnlockPairingChange,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_MESSAGE,
          ui::GetChromeOSDeviceName()),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_NOTIFICATION_EASYUNLOCK_ENABLED)),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowPairingChangeAppliedNotification(
    const std::string& phone_name) {
  // Remove the pairing change notification if it is still being shown.
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEasyUnlockPairingChangeNotifierId);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeAppliedNotifierId,
      ash::NotificationCatalogName::kEasyUnlockPairingChangeApplied,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_MESSAGE,
          base::UTF8ToUTF16(phone_name), ui::GetChromeOSDeviceName()),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_NOTIFICATION_EASYUNLOCK_ENABLED)),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeAppliedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void EasyUnlockNotificationController::LaunchEasyUnlockSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kSmartLockSubpagePath);
}

void EasyUnlockNotificationController::LaunchMultiDeviceSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath);
}

void EasyUnlockNotificationController::LockScreen() {
  proximity_auth::ScreenlockBridge::Get()->Lock();
}

EasyUnlockNotificationController::NotificationDelegate::NotificationDelegate(
    const std::string& notification_id,
    const base::WeakPtr<EasyUnlockNotificationController>&
        notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

EasyUnlockNotificationController::NotificationDelegate::
    ~NotificationDelegate() {}

void EasyUnlockNotificationController::NotificationDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (!notification_controller_)
    return;

  if (notification_id_ == kEasyUnlockPairingChangeNotifierId) {
    if (!button_index)
      return;

    if (*button_index == 0) {
      notification_controller_->LockScreen();
      return;
    }

    DCHECK_EQ(1, *button_index);
  }

  // The kSmartLockSignInRemoved flag removes the easy unlock settings page, so
  // check flag to determine which route should be launched.
  if (base::FeatureList::IsEnabled(ash::features::kSmartLockSignInRemoved)) {
    notification_controller_->LaunchMultiDeviceSettings();
  } else {
    notification_controller_->LaunchEasyUnlockSettings();
  }
}

}  // namespace ash
