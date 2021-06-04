// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/cpp/update_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
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
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kNotifierId[] = "ash.update";

const char kNotificationId[] = "chrome://update";

bool CheckForSlowBoot(const base::FilePath& slow_boot_file_path) {
  if (base::PathExists(slow_boot_file_path)) {
    return true;
  }
  return false;
}

}  // namespace

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
    absl::optional<bool> slow_boot_file_path_exists) {
  if (!ShouldShowUpdate()) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
    return;
  }

  if (slow_boot_file_path_exists != absl::nullopt) {
    slow_boot_file_path_exists_ = slow_boot_file_path_exists.value();
  }

  const bool is_rollback = model_->rollback();
  const NotificationStyle notification_style = model_->notification_style();

  message_center::SystemNotificationWarningLevel warning_level =
      (is_rollback || notification_style == NotificationStyle::kAdminRequired)
          ? message_center::SystemNotificationWarningLevel::WARNING
          : message_center::SystemNotificationWarningLevel::NORMAL;
  const gfx::VectorIcon& notification_icon =
      is_rollback ? kSystemMenuRollbackIcon
                  : (notification_style == NotificationStyle::kDefault)
                        ? kSystemMenuUpdateIcon
                        : vector_icons::kBusinessIcon;
  std::unique_ptr<Notification> notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      GetNotificationTitle(), GetNotificationMessage(),
      std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &UpdateNotificationController::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr())),
      notification_icon, warning_level);
  notification->set_pinned(true);

  if (model_->notification_style() == NotificationStyle::kAdminRequired)
    notification->SetSystemPriority();

  if (model_->update_required()) {
    std::vector<message_center::ButtonInfo> notification_actions;
    if (is_rollback) {
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

std::u16string UpdateNotificationController::GetNotificationMessage() const {
  if (model_->update_type() == UpdateType::kLacros)
    return l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_LACROS);

  std::u16string system_app_name =
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME);
  if (model_->rollback()) {
    return l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK);
  }
  if (model_->factory_reset_required()) {
    return l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_POWERWASH,
                                      system_app_name);
  }

  const std::u16string notification_body = model_->notification_body();
  std::u16string update_text;
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

std::u16string UpdateNotificationController::GetNotificationTitle() const {
  if (model_->update_type() == UpdateType::kLacros)
    return l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_TITLE_LACROS);

  const std::u16string notification_title = model_->notification_title();
  if (!notification_title.empty())
    return notification_title;

  return model_->rollback()
             ? l10n_util::GetStringUTF16(IDS_ROLLBACK_NOTIFICATION_TITLE)
             : l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_TITLE);
}

void UpdateNotificationController::RestartForUpdate() {
  confirmation_dialog_ = nullptr;
  if (model_->update_type() == UpdateType::kLacros) {
    // Lacros only needs to restart the browser to cause the component updater
    // to use the new lacros component.
    Shell::Get()->session_controller()->AttemptRestartChrome();
    return;
  }
  // System updates require restarting the device.
  Shell::Get()->system_tray_model()->client()->RequestRestartForUpdate();
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_OS_UPDATE_DEFAULT_SELECTED);
}

void UpdateNotificationController::RestartCancelled() {
  confirmation_dialog_ = nullptr;
  // Put the notification back.
  GenerateUpdateNotification(absl::nullopt);
}

void UpdateNotificationController::HandleNotificationClick(
    absl::optional<int> button_index) {
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
