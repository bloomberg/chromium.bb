// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniExportImportOperation[] =
    "crostini.export_operation";

}  // namespace

CrostiniExportImportClickCloseDelegate::CrostiniExportImportClickCloseDelegate()
    : HandleNotificationClickDelegate(
          HandleNotificationClickDelegate::ButtonClickCallback()) {}

void CrostiniExportImportClickCloseDelegate::Close(bool by_user) {
  if (!close_closure_.is_null())
    close_closure_.Run();
}

CrostiniExportImportClickCloseDelegate::
    ~CrostiniExportImportClickCloseDelegate() = default;

CrostiniExportImportNotificationController::
    CrostiniExportImportNotificationController(
        Profile* profile,
        ExportImportType type,
        const std::string& notification_id,
        base::FilePath path,
        ContainerId container_id)
    : CrostiniExportImportStatusTracker(type, std::move(path)),
      profile_(profile),
      container_id_(std::move(container_id)),
      delegate_(
          base::MakeRefCounted<CrostiniExportImportClickCloseDelegate>()) {
  delegate_->SetCloseCallback(base::BindRepeating(
      &CrostiniExportImportNotificationController::on_notification_closed,
      weak_ptr_factory_.GetWeakPtr()));

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &kNotificationLinuxIcon;
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      base::string16(), base::string16(),
      gfx::Image(),  // icon
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_IMPORT_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCrostiniExportImportOperation),
      rich_notification_data, delegate_);
}

CrostiniExportImportNotificationController::
    ~CrostiniExportImportNotificationController() = default;

void CrostiniExportImportNotificationController::ForceRedisplay() {
  hidden_ = false;

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

void CrostiniExportImportNotificationController::SetStatusRunningUI(
    int progress_percent) {
  if (hidden_) {
    return;
  }

  delegate_->SetCallback(base::BindRepeating(
      [](Profile* profile, ExportImportType type, ContainerId container_id,
         base::Optional<int> button_index) {
        if (!button_index.has_value()) {
          return;
        }
        DCHECK(*button_index == 1);
        CrostiniExportImport::GetForProfile(profile)->CancelOperation(
            type, container_id);
      },
      profile_, type(), container_id_));

  notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification_->set_accent_color(ash::kSystemNotificationColorNormal);
  notification_->set_title(l10n_util::GetStringUTF16(
      type() == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_RUNNING
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_RUNNING));
  notification_->set_message(
      GetTimeRemainingMessage(started_, progress_percent));
  notification_->set_buttons({message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CANCEL))});
  notification_->set_never_timeout(true);
  notification_->set_progress(progress_percent);
  notification_->set_pinned(true);

  ForceRedisplay();
}

void CrostiniExportImportNotificationController::SetStatusCancellingUI() {
  if (hidden_) {
    return;
  }

  delegate_->SetCallback(
      CrostiniExportImportClickCloseDelegate::ButtonClickCallback());

  notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification_->set_accent_color(ash::kSystemNotificationColorNormal);
  notification_->set_title(l10n_util::GetStringUTF16(
      type() == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_CANCELLING
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_CANCELLING));
  notification_->set_message({});
  notification_->set_buttons({});
  notification_->set_never_timeout(true);
  notification_->set_progress(-1);  // Infinite progress bar
  notification_->set_pinned(false);

  ForceRedisplay();
}

void CrostiniExportImportNotificationController::SetStatusDoneUI() {
  if (type() == ExportImportType::EXPORT) {
    delegate_->SetCallback(base::BindRepeating(
        [](Profile* profile, base::FilePath path) {
          platform_util::ShowItemInFolder(profile, path);
        },
        profile_, path()));
  } else {
    delegate_->SetCallback(
        CrostiniExportImportClickCloseDelegate::ButtonClickCallback());
  }

  notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
  notification_->set_accent_color(ash::kSystemNotificationColorNormal);
  notification_->set_title(l10n_util::GetStringUTF16(
      type() == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_DONE
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_DONE));
  notification_->set_message(l10n_util::GetStringUTF16(
      type() == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_DONE
          : IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_DONE));
  notification_->set_buttons({});
  notification_->set_never_timeout(false);
  notification_->set_pinned(false);

  ForceRedisplay();
}

void CrostiniExportImportNotificationController::SetStatusCancelledUI() {
  delegate_->SetCallback(
      CrostiniExportImportClickCloseDelegate::ButtonClickCallback());

  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_->id());
}

void CrostiniExportImportNotificationController::SetStatusFailedWithMessageUI(
    Status status,
    const base::string16& message) {
  switch (status) {
    case Status::FAILED_UNKNOWN_REASON:
      delegate_->SetCallback(base::BindRepeating(
          [](Profile* profile) {
            chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
                profile, chromeos::settings::mojom::
                             kCrostiniBackupAndRestoreSubpagePath);
          },
          profile_));
      break;
    case Status::FAILED_ARCHITECTURE_MISMATCH:
      delegate_->SetCallback(base::BindRepeating(
          [](Profile* profile) {
            NavigateParams params(profile,
                                  GURL(chrome::kLinuxExportImportHelpURL),
                                  ui::PAGE_TRANSITION_LINK);
            params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
            params.window_action = NavigateParams::SHOW_WINDOW;
            Navigate(&params);
          },
          profile_));
      break;
    case Status::FAILED_INSUFFICIENT_SPACE:
      delegate_->SetCallback(base::BindRepeating(
          [](Profile* profile) {
            chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
                profile, chromeos::settings::mojom::kStorageSubpagePath);
          },
          profile_));
      break;
    default:
      LOG(ERROR) << "Unexpected failure status "
                 << static_cast<std::underlying_type_t<Status>>(status);
      delegate_->SetCallback(
          CrostiniExportImportClickCloseDelegate::ButtonClickCallback());
  }

  notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
  notification_->set_accent_color(ash::kSystemNotificationColorCriticalWarning);
  notification_->set_title(l10n_util::GetStringUTF16(
      type() == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_FAILED
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_FAILED));
  notification_->set_message(message);
  notification_->set_buttons({});
  notification_->set_never_timeout(false);
  notification_->set_pinned(false);

  ForceRedisplay();
}

}  // namespace crostini
