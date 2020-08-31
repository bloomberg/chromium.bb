// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/session/shutdown_confirmation_dialog.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kNotifierId[] = "ash.update";

bool CheckForSlowBoot(const base::FilePath& slow_boot_file_path) {
  if (base::PathExists(slow_boot_file_path)) {
    return true;
  }
  return false;
}

}  // namespace

// static
const char UpdateNotificationController::kNotificationId[] = "chrome://update";

UpdateNotificationController::UpdateNotificationController()
    : model_(Shell::Get()->system_tray_model()->update_model()),
      slow_boot_file_path_("/mnt/stateful_partition/etc/slow_boot_required") {
  model_->AddObserver(this);
  OnUpdateAvailable();
}

UpdateNotificationController::~UpdateNotificationController() {
  model_->RemoveObserver(this);
}

void UpdateNotificationController::GenerateUpdateNotification(
    base::Optional<bool> slow_boot_file_path_exists) {
  if (!ShouldShowUpdate()) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
    return;
  }

  if (slow_boot_file_path_exists != base::nullopt) {
    slow_boot_file_path_exists_ = slow_boot_file_path_exists.value();
  }

  message_center::SystemNotificationWarningLevel warning_level =
      (model_->rollback() ||
       model_->notification_style() == NotificationStyle::kAdminRequired)
          ? message_center::SystemNotificationWarningLevel::WARNING
          : message_center::SystemNotificationWarningLevel::NORMAL;
  std::unique_ptr<Notification> notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      GetNotificationTitle(), GetNotificationMessage(),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &UpdateNotificationController::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr())),
      model_->rollback() ? kSystemMenuRollbackIcon
                         : vector_icons::kBusinessIcon,
      warning_level);
  notification->set_pinned(true);

  if (model_->notification_style() == NotificationStyle::kAdminRequired)
    notification->SetSystemPriority();

  if (model_->update_required()) {
    std::vector<message_center::ButtonInfo> notification_actions;
    if (model_->rollback()) {
      notification_actions.push_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ROLLBACK_NOTIFICATION_RESTART_BUTTON)));
    } else {
      notification_actions.push_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON)));
    }
    notification->set_buttons(notification_actions);
  }

  MessageCenter::Get()->AddNotification(std::move(notification));
}

void UpdateNotificationController::OnUpdateAvailable() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CheckForSlowBoot, slow_boot_file_path_),
      base::BindOnce(&UpdateNotificationController::GenerateUpdateNotification,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool UpdateNotificationController::ShouldShowUpdate() const {
  return model_->update_required() || model_->update_over_cellular_available();
}

base::string16 UpdateNotificationController::GetNotificationMessage() const {
  base::string16 system_app_name =
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME);
  if (model_->rollback()) {
    return l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK);
  }
  if (model_->factory_reset_required()) {
    return l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_POWERWASH,
                                      system_app_name);
  }

  const base::string16 notification_body = model_->notification_body();
  base::string16 update_text;
  if (model_->update_type() == UpdateType::kSystem &&
      !notification_body.empty()) {
    update_text = notification_body;
  } else {
    update_text = l10n_util::GetStringFUTF16(
        IDS_UPDATE_NOTIFICATION_MESSAGE_LEARN_MORE, system_app_name);
  }

  if (slow_boot_file_path_exists_) {
    return l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_SLOW_BOOT,
                                      update_text);
  }
  return update_text;
}

base::string16 UpdateNotificationController::GetNotificationTitle() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (model_->update_type() == UpdateType::kFlash) {
    return l10n_util::GetStringUTF16(
        IDS_UPDATE_NOTIFICATION_TITLE_FLASH_PLAYER);
  }
#endif
  const base::string16 notification_title = model_->notification_title();
  if (!notification_title.empty())
    return notification_title;

  return model_->rollback()
             ? l10n_util::GetStringUTF16(IDS_ROLLBACK_NOTIFICATION_TITLE)
             : l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_TITLE);
}

void UpdateNotificationController::RestartForUpdate() {
  confirmation_dialog_ = nullptr;
  Shell::Get()->system_tray_model()->client()->RequestRestartForUpdate();
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED);
}

void UpdateNotificationController::RestartCancelled() {
  confirmation_dialog_ = nullptr;
  // Put the notification back.
  GenerateUpdateNotification(base::nullopt);
}

void UpdateNotificationController::HandleNotificationClick(
    base::Optional<int> button_index) {
  DCHECK(ShouldShowUpdate());

  if (!button_index) {
    // Notification message body clicked, which says "learn more".
    Shell::Get()->system_tray_model()->client()->ShowAboutChromeOS();
    return;
  }

  // Restart
  DCHECK(button_index.value() == 0);
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           false /* by_user */);

  if (model_->update_required()) {
    if (slow_boot_file_path_exists_) {
      // An active dialog exists already.
      if (confirmation_dialog_)
        return;

      confirmation_dialog_ = new ShutdownConfirmationDialog(
          IDS_DIALOG_TITLE_SLOW_BOOT, IDS_DIALOG_MESSAGE_SLOW_BOOT,
          base::BindOnce(&UpdateNotificationController::RestartForUpdate,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&UpdateNotificationController::RestartCancelled,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      RestartForUpdate();
    }
  } else {
    // Shows the about chrome OS page and checks for update after the page is
    // loaded.
    Shell::Get()->system_tray_model()->client()->ShowAboutChromeOS();
  }
}

}  // namespace ash
