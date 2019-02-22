// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_package_installer_notification.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_package_installer_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniPackageInstaller[] =
    "crostini.package_installer";

}  // namespace

CrostiniPackageInstallerNotification::CrostiniPackageInstallerNotification(
    Profile* profile,
    const std::string& notification_id,
    CrostiniPackageInstallerService* installer_service)
    : installer_service_(installer_service),
      profile_(profile),
      weak_ptr_factory_(this) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &ash::kNotificationLinuxIcon;
  rich_notification_data.never_timeout = true;
  rich_notification_data.accent_color =
      message_center::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_IN_PROGRESS_TITLE),
      base::string16(),  // body
      gfx::Image(),      // icon
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kNotifierCrostiniPackageInstaller),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  UpdateDisplayedNotification();
}

CrostiniPackageInstallerNotification::~CrostiniPackageInstallerNotification() =
    default;

// TODO(timloh): This doesn't get called if the user shuts down Crostini, so
// the notification will be stuck at whatever percentage it is at.
void CrostiniPackageInstallerNotification::UpdateProgress(
    InstallLinuxPackageProgressStatus result,
    int progress_percent,
    const std::string& failure_reason) {
  if (result == InstallLinuxPackageProgressStatus::SUCCEEDED ||
      result == InstallLinuxPackageProgressStatus::FAILED) {
    // The package installer service will stop sending us updates after this.
    int title_id = IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_TITLE;
    int message_id =
        IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_MESSAGE;
    if (result != InstallLinuxPackageProgressStatus::SUCCEEDED) {
      title_id = IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_TITLE;
      message_id = IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_MESSAGE;
      notification_->set_accent_color(
          message_center::kSystemNotificationColorCriticalWarning);
    }
    notification_->set_title(l10n_util::GetStringUTF16(title_id));
    notification_->set_message(l10n_util::GetStringUTF16(message_id));
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    notification_->set_never_timeout(false);
  } else {
    int display_progress = progress_percent / 2;
    if (result == InstallLinuxPackageProgressStatus::INSTALLING)
      display_progress += 50;
    else
      DCHECK_EQ(InstallLinuxPackageProgressStatus::DOWNLOADING, result);

    notification_->set_progress(display_progress);
  }

  UpdateDisplayedNotification();
}

void CrostiniPackageInstallerNotification::Close(bool by_user) {
  // This call deletes us.
  installer_service_->NotificationClosed(this);
}

void CrostiniPackageInstallerNotification::UpdateDisplayedNotification() {
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  display_service->Display(NotificationHandler::Type::TRANSIENT,
                           *notification_);
}

}  // namespace crostini
