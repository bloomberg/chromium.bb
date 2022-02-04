// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/drivefs_native_message_host.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace {

void CancelCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemOperationRunner::OperationID operation_id) {
  file_system_context->operation_runner()->Cancel(
      operation_id, base::BindOnce([](base::File::Error error) {
        DLOG_IF(WARNING, error != base::File::FILE_OK)
            << "Failed to cancel copy: " << error;
      }));
}

constexpr char kSwaFileOperationPrefix[] = "swa-file-operation-";

bool NotificationIdToOperationId(
    const std::string& notification_id,
    storage::FileSystemOperationRunner::OperationID* operation_id) {
  *operation_id = 0;
  std::string id_string;
  if (base::RemoveChars(notification_id, kSwaFileOperationPrefix, &id_string)) {
    if (base::StringToUint64(id_string, operation_id)) {
      return true;
    }
  }

  return false;
}

void RecordDeviceNotificationMetric(
    file_manager::DeviceNotificationUmaType type) {
  UMA_HISTOGRAM_ENUMERATION(file_manager::kNotificationShowHistogramName, type);
}

void RecordDeviceNotificationUserActionMetric(
    file_manager::DeviceNotificationUserActionUmaType type) {
  UMA_HISTOGRAM_ENUMERATION(file_manager::kNotificationUserActionHistogramName,
                            type);
}

using file_manager::io_task::OperationType;
using file_manager::io_task::ProgressStatus;
using file_manager::util::GetDisplayableFileName16;
using l10n_util::GetStringFUTF16;

std::u16string GetIOTaskMessage(const ProgressStatus& status) {
  switch (status.type) {
    case OperationType::kCopy:
      if (status.sources.size() > 1) {
        return GetStringFUTF16(IDS_FILE_BROWSER_COPY_ITEMS_REMAINING,
                               base::NumberToString16(status.sources.size()));
      }
      return GetStringFUTF16(
          IDS_FILE_BROWSER_COPY_FILE_NAME,
          GetDisplayableFileName16(status.sources.back().url));
    case OperationType::kMove:
      if (status.sources.size() > 1) {
        return GetStringFUTF16(IDS_FILE_BROWSER_MOVE_ITEMS_REMAINING,
                               base::NumberToString16(status.sources.size()));
      }
      return GetStringFUTF16(
          IDS_FILE_BROWSER_MOVE_FILE_NAME,
          GetDisplayableFileName16(status.sources.back().url));
    case OperationType::kDelete:
      if (status.sources.size() > 1) {
        return GetStringFUTF16(IDS_FILE_BROWSER_DELETE_ITEMS_REMAINING,
                               base::NumberToString16(status.sources.size()));
      }
      return GetStringFUTF16(
          IDS_FILE_BROWSER_DELETE_FILE_NAME,
          GetDisplayableFileName16(status.sources.back().url));

    case OperationType::kZip:
      if (status.sources.size() > 1) {
        return GetStringFUTF16(IDS_FILE_BROWSER_ZIP_ITEMS_REMAINING,
                               base::NumberToString16(status.sources.size()));
      }
      return GetStringFUTF16(
          IDS_FILE_BROWSER_ZIP_FILE_NAME,
          GetDisplayableFileName16(status.sources.back().url));
    default:
      NOTREACHED();
      return u"Unknown operation type";
  }
}

}  // namespace

namespace file_manager {

SystemNotificationManager::SystemNotificationManager(Profile* profile)
    : profile_(profile),
      app_name_(l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME)),
      swa_enabled_(ash::features::IsFileManagerSwaEnabled()) {}

SystemNotificationManager::~SystemNotificationManager() = default;

bool SystemNotificationManager::DoFilesSwaWindowsExist() {
  return FindSystemWebAppBrowser(profile_, web_app::SystemAppType::FILE_MANAGER,
                                 Browser::TYPE_APP) != nullptr;
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const scoped_refptr<message_center::NotificationDelegate>& delegate) {
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      app_name_, GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(), delegate, kProductIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const base::RepeatingClosure& click_callback) {
  return CreateNotification(
      notification_id, title, message,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          click_callback));
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message) {
  return CreateNotification(
      notification_id, title, message,
      base::BindRepeating(&SystemNotificationManager::Dismiss,
                          weak_ptr_factory_.GetWeakPtr(), notification_id));
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    int title_id,
    int message_id) {
  std::u16string title = l10n_util::GetStringUTF16(title_id);
  std::u16string message = l10n_util::GetStringUTF16(message_id);
  return CreateNotification(notification_id, title, message);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    const scoped_refptr<message_center::NotificationDelegate>& delegate) {
  std::u16string title = l10n_util::GetStringUTF16(title_id);
  std::u16string message = l10n_util::GetStringUTF16(message_id);
  return CreateNotification(notification_id, title, message, delegate);
}

void SystemNotificationManager::HandleProgressClick(
    const std::string& notification_id,
    absl::optional<int> button_index) {
  if (button_index) {
    // Cancel the copy operation.
    scoped_refptr<storage::FileSystemContext> file_system_context =
        util::GetFileManagerFileSystemContext(profile_);
    storage::FileSystemOperationRunner::OperationID operation_id;
    if (NotificationIdToOperationId(notification_id, &operation_id)) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CancelCopyOnIOThread, file_system_context,
                                    operation_id));
    }
  }
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateProgressNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    int progress) {
  message_center::RichNotificationData rich_data;
  rich_data.progress = progress;

  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id, title,
      message, app_name_, GURL(), message_center::NotifierId(), rich_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&SystemNotificationManager::HandleProgressClick,
                              weak_ptr_factory_.GetWeakPtr(), notification_id)),
      kProductIcon, message_center::SystemNotificationWarningLevel::NORMAL);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::CreateIOTaskProgressNotification(
    file_manager::io_task::IOTaskId task_id,
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    int progress) {
  message_center::RichNotificationData rich_data;
  rich_data.progress = progress;

  auto notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id, title,
      message, app_name_, GURL(), message_center::NotifierId(), rich_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&SystemNotificationManager::CancelTaskId,
                              weak_ptr_factory_.GetWeakPtr(), task_id,
                              notification_id)),
      kProductIcon, message_center::SystemNotificationWarningLevel::NORMAL);

  // Add the cancel button:
  notification->set_buttons({message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL))});
  return notification;
}

void SystemNotificationManager::CancelTaskId(
    file_manager::io_task::IOTaskId task_id,
    const std::string& notification_id,
    absl::optional<int> button_index) {
  if (button_index) {
    if (io_task_controller_) {
      io_task_controller_->Cancel(task_id);
    } else {
      LOG(ERROR) << "No TaskController, can't cancel task_id: " << task_id;
    }
  }
}

void SystemNotificationManager::Dismiss(const std::string& notification_id) {
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         notification_id);
}

constexpr char kDeviceFailNotificationId[] = "swa-device-fail-id";

void SystemNotificationManager::HandleDeviceEvent(
    const file_manager_private::DeviceEvent& event) {
  if (!swa_enabled_) {
    return;
  }
  std::unique_ptr<message_center::Notification> notification;

  std::u16string title;
  std::u16string message;
  const char* id = file_manager_private::ToString(event.type);
  switch (event.type) {
    case file_manager_private::DEVICE_EVENT_TYPE_DISABLED:
      notification =
          CreateNotification(id, IDS_REMOVABLE_DEVICE_DETECTION_TITLE,
                             IDS_EXTERNAL_STORAGE_DISABLED_MESSAGE);
      RecordDeviceNotificationMetric(
          DeviceNotificationUmaType::DEVICE_EXTERNAL_STORAGE_DISABLED);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_REMOVED:
      // Hide device fail & storage disabled notifications.
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, kDeviceFailNotificationId);
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT,
          file_manager_private::ToString(
              file_manager_private::DEVICE_EVENT_TYPE_DISABLED));
      // Remove the device from the mount status map.
      mount_status_.erase(event.device_path);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_HARD_UNPLUGGED:
      notification = CreateNotification(id, IDS_DEVICE_HARD_UNPLUGGED_TITLE,
                                        IDS_DEVICE_HARD_UNPLUGGED_MESSAGE);
      RecordDeviceNotificationMetric(
          DeviceNotificationUmaType::DEVICE_HARD_UNPLUGGED);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_FORMAT_START:
      title = l10n_util::GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                                         base::UTF8ToUTF16(event.device_label));
      message =
          l10n_util::GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_PROGRESS_MESSAGE,
                                     base::UTF8ToUTF16(event.device_label));
      notification = CreateNotification(id, title, message);
      RecordDeviceNotificationMetric(DeviceNotificationUmaType::FORMAT_START);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_FORMAT_SUCCESS:
    case file_manager_private::DEVICE_EVENT_TYPE_FORMAT_FAIL:
    case file_manager_private::DEVICE_EVENT_TYPE_PARTITION_FAIL:
      // Hide the formatting notification.
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT,
          file_manager_private::ToString(
              file_manager_private::DEVICE_EVENT_TYPE_FORMAT_START));
      title = l10n_util::GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_DIALOG_TITLE,
                                         base::UTF8ToUTF16(event.device_label));
      if (event.type ==
          file_manager_private::DEVICE_EVENT_TYPE_FORMAT_SUCCESS) {
        message =
            l10n_util::GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_SUCCESS_MESSAGE,
                                       base::UTF8ToUTF16(event.device_label));
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::FORMAT_SUCCESS);
      } else {
        message =
            l10n_util::GetStringFUTF16(IDS_FILE_BROWSER_FORMAT_FAILURE_MESSAGE,
                                       base::UTF8ToUTF16(event.device_label));
        RecordDeviceNotificationMetric(
            event.type == file_manager_private::DEVICE_EVENT_TYPE_FORMAT_FAIL
                ? DeviceNotificationUmaType::FORMAT_FAIL
                : DeviceNotificationUmaType::PARTITION_FAIL);
      }
      notification = CreateNotification(id, title, message);
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_PARTITION_START:
    case file_manager_private::DEVICE_EVENT_TYPE_PARTITION_SUCCESS:
      // No-op.
      break;
    case file_manager_private::DEVICE_EVENT_TYPE_RENAME_FAIL:
      notification =
          CreateNotification(id, IDS_RENAMING_OF_DEVICE_FAILED_TITLE,
                             IDS_RENAMING_OF_DEVICE_FINISHED_FAILURE_MESSAGE);
      RecordDeviceNotificationMetric(DeviceNotificationUmaType::RENAME_FAIL);
      break;
    default:
      DLOG(WARNING) << "Unable to generate notification for " << id;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

namespace file_manager_private = extensions::api::file_manager_private;

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeDriveSyncErrorNotification(
    const extensions::Event& event,
    base::Value::ListView& event_arguments) {
  std::unique_ptr<message_center::Notification> notification;
  file_manager_private::DriveSyncErrorEvent sync_error;
  const char* id;
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  std::u16string message;
  if (file_manager_private::DriveSyncErrorEvent::Populate(event_arguments[0],
                                                          &sync_error)) {
    id = file_manager_private::ToString(sync_error.type);
    GURL file_url(sync_error.file_url);
    switch (sync_error.type) {
      case file_manager_private::
          DRIVE_SYNC_ERROR_TYPE_DELETE_WITHOUT_PERMISSION:
        message = l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_SYNC_DELETE_WITHOUT_PERMISSION_ERROR,
            util::GetDisplayableFileName16(file_url));
        notification = CreateNotification(id, title, message);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_SERVICE_UNAVAILABLE:
        notification =
            CreateNotification(id, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
                               IDS_FILE_BROWSER_SYNC_SERVICE_UNAVAILABLE_ERROR);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE:
        message = l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_SYNC_NO_SERVER_SPACE,
            util::GetDisplayableFileName16(file_url));
        notification = CreateNotification(id, title, message);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_LOCAL_SPACE:
        notification =
            CreateNotification(id, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
                               IDS_FILE_BROWSER_DRIVE_OUT_OF_SPACE_HEADER);
        break;
      case file_manager_private::DRIVE_SYNC_ERROR_TYPE_MISC:
        message = l10n_util::GetStringFUTF16(
            IDS_FILE_BROWSER_SYNC_MISC_ERROR,
            util::GetDisplayableFileName16(file_url));
        notification = CreateNotification(id, title, message);
        break;
      default:
        DLOG(WARNING) << "Unknown Drive Sync error: " << sync_error.type;
        break;
    }
  }
  return notification;
}

const char* kDriveDialogId = "swa-drive-confirm-dialog";

void SystemNotificationManager::HandleDriveDialogClick(
    absl::optional<int> button_index) {
  drivefs::mojom::DialogResult result = drivefs::mojom::DialogResult::kDismiss;
  if (button_index) {
    if (button_index.value() == 1) {
      result = drivefs::mojom::DialogResult::kAccept;
    } else {
      result = drivefs::mojom::DialogResult::kReject;
    }
  }
  // Send the dialog result to the callback stored in DriveFS on dialog
  // creation.
  if (drivefs_event_router_) {
    drivefs_event_router_->OnDialogResult(result);
  }
  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kDriveDialogId);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeDriveConfirmDialogNotification(
    const extensions::Event& event,
    base::Value::ListView& event_arguments) {
  std::unique_ptr<message_center::Notification> notification;
  file_manager_private::DriveConfirmDialogEvent dialog_event;
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  std::u16string message;
  if (file_manager_private::DriveConfirmDialogEvent::Populate(
          event_arguments[0], &dialog_event)) {
    std::vector<message_center::ButtonInfo> notification_buttons;
    scoped_refptr<message_center::NotificationDelegate> delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(
                &SystemNotificationManager::HandleDriveDialogClick,
                weak_ptr_factory_.GetWeakPtr()));
    notification = CreateNotification(
        kDriveDialogId, IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL,
        IDS_FILE_BROWSER_OFFLINE_ENABLE_MESSAGE, delegate);

    notification_buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OFFLINE_ENABLE_REJECT)));
    notification_buttons.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OFFLINE_ENABLE_ACCEPT)));
    notification->set_buttons(notification_buttons);
  }
  return notification;
}

constexpr char kDriveSyncId[] = "swa-drive-sync";
constexpr char kDrivePinId[] = "swa-drive-pin";

std::unique_ptr<message_center::Notification>
SystemNotificationManager::UpdateDriveSyncNotification(
    const extensions::Event& event,
    base::Value::ListView& event_arguments) {
  std::unique_ptr<message_center::Notification> notification;
  file_manager_private::FileTransferStatus transfer_status;
  if (!file_manager_private::FileTransferStatus::Populate(event_arguments[0],
                                                          &transfer_status)) {
    LOG(ERROR) << "Invalid event argument or transfer status...";
    return notification;
  }

  // Work out if this is a sync or pin update.
  bool is_sync_operation =
      (event.histogram_value ==
       extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED);
  if (transfer_status.transfer_state ==
          file_manager_private::TRANSFER_STATE_COMPLETED ||
      transfer_status.transfer_state ==
          file_manager_private::TRANSFER_STATE_FAILED) {
    // We only close when there are no jobs left, we could have received
    // a TRANSFER_STATE_COMPLETED event when there are more jobs to run.
    if (transfer_status.num_total_jobs == 0) {
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT,
          is_sync_operation ? kDriveSyncId : kDrivePinId);
    }
    return notification;
  }
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_GRID_VIEW_FILES_TITLE);
  std::u16string message;
  int message_template;
  if (transfer_status.num_total_jobs == 1) {
    message_template = is_sync_operation
                           ? IDS_FILE_BROWSER_SYNC_FILE_NAME
                           : IDS_FILE_BROWSER_OFFLINE_PROGRESS_MESSAGE;
    message = l10n_util::GetStringFUTF16(
        message_template,
        util::GetDisplayableFileName16(GURL(transfer_status.file_url)));
  } else {
    message_template = is_sync_operation
                           ? IDS_FILE_BROWSER_SYNC_FILE_NUMBER
                           : IDS_FILE_BROWSER_OFFLINE_PROGRESS_MESSAGE_PLURAL;
    message = l10n_util::GetStringFUTF16(
        message_template,
        base::NumberToString16(transfer_status.num_total_jobs));
  }
  notification = CreateProgressNotification(
      is_sync_operation ? kDriveSyncId : kDrivePinId, title, message,
      static_cast<int>((transfer_status.processed / transfer_status.total) *
                       100.0));
  return notification;
}

void SystemNotificationManager::HandleEvent(const extensions::Event& event) {
  if (!swa_enabled_) {
    return;
  }
  base::Value::ListView event_arguments;

  event_arguments = event.event_args->GetList();
  if (event_arguments.size() < 1) {
    return;
  }
  // For some events we always display a system notification regardless of if
  // there are any SWA windows open.
  bool force_as_system_notification = false;
  std::unique_ptr<message_center::Notification> notification;
  switch (event.histogram_value) {
    case extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR:
      notification = MakeDriveSyncErrorNotification(event, event_arguments);
      break;
    case extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_CONFIRM_DIALOG:
      notification = MakeDriveConfirmDialogNotification(event, event_arguments);
      force_as_system_notification = true;
      break;
    case extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED:
    case extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED:
      notification = UpdateDriveSyncNotification(event, event_arguments);
      break;
    default:
      DLOG(WARNING) << "Unhandled event: " << event.event_name;
      break;
  }

  if (notification) {
    // Check if we need to remove any progress notification when there
    // are active SWA windows.
    if (!force_as_system_notification && DoFilesSwaWindowsExist()) {
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, notification->id());
      return;
    }
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

void SystemNotificationManager::HandleCopyStart(
    int copy_id,
    file_manager_private::CopyOrMoveProgressStatus& status) {
  if (!swa_enabled_) {
    return;
  }
  if (status.size) {
    required_copy_space_[copy_id] = *status.size;
  }
}

void SystemNotificationManager::HandleCopyEvent(
    int copy_id,
    file_manager_private::CopyOrMoveProgressStatus& status) {
  if (!swa_enabled_) {
    return;
  }
  std::unique_ptr<message_center::Notification> notification;
  int progress = 0;
  std::string id =
      base::StrCat({kSwaFileOperationPrefix, base::NumberToString(copy_id)});
  // Check if we need to remove any progress notification when there
  // are active SWA windows.
  if (DoFilesSwaWindowsExist()) {
    GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                           id);
    return;
  }
  std::u16string title = l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME);

  std::u16string message;
  if (status.source_url) {
    message = l10n_util::GetStringFUTF16(
        IDS_FILE_BROWSER_COPY_FILE_NAME,
        util::GetDisplayableFileName16(GURL(*status.source_url)));
  } else {
    message = l10n_util::GetStringUTF16(IDS_FILE_BROWSER_FILE_ERROR_GENERIC);
  }

  auto copy_operation = required_copy_space_.find(copy_id);
  switch (status.type) {
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_BEGIN:
      notification = CreateProgressNotification(id, title, message, 0);
      notification->set_buttons({message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL))});
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_PROGRESS:
      if (copy_operation != required_copy_space_.end()) {
        if (status.size) {
          progress =
              static_cast<int>((*status.size / copy_operation->second) * 100.0);
        }
      }
      notification = CreateProgressNotification(id, title, message, progress);
      notification->set_buttons({message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL))});
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_END_COPY:
      notification = CreateProgressNotification(id, title, message, 100);
      break;
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_SUCCESS:
    case file_manager_private::COPY_OR_MOVE_PROGRESS_STATUS_TYPE_ERROR:
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, id);
      required_copy_space_.erase(copy_id);
      break;
    default:
      DLOG(WARNING) << "Unhandled copy event for type " << status.type;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

void SystemNotificationManager::HandleIOTaskProgress(
    const file_manager::io_task::ProgressStatus& status) {
  if (!swa_enabled_) {
    return;
  }

  std::string id = base::StrCat(
      {kSwaFileOperationPrefix, base::NumberToString(status.task_id)});

  // If there are any SWA windows open, we remove the progress in system
  // notification.
  if (DoFilesSwaWindowsExist()) {
    GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                           id);
    return;
  }

  if (status.state == io_task::State::kError ||
      status.state == io_task::State::kCancelled ||
      status.state == io_task::State::kSuccess) {
    GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                           id);
    return;
  }

  // From here state is kQueued or kInProgress:
  std::u16string title = l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME);

  std::u16string message = GetIOTaskMessage(status);

  int progress = 0;
  if (status.total_bytes > 0) {
    progress = status.bytes_transferred * 100.0 / status.total_bytes;
  }

  std::unique_ptr<message_center::Notification> notification =
      CreateIOTaskProgressNotification(status.task_id, id, title, message,
                                       progress);

  GetNotificationDisplayService()->Display(NotificationHandler::Type::TRANSIENT,
                                           *notification,
                                           /*metadata=*/nullptr);
}

constexpr char kRemovableNotificationId[] = "swa-removable-device-id";

void SystemNotificationManager::HandleRemovableNotificationClick(
    const std::string& path,
    const std::vector<DeviceNotificationUserActionUmaType>&
        uma_types_for_buttons,
    absl::optional<int> button_index) {
  if (button_index) {
    if (button_index.value() == 0) {
      base::FilePath volume_root(path);
      platform_util::ShowItemInFolder(profile_, volume_root);
    } else {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kExternalStorageSubpagePath);
    }
    if (button_index.value() < uma_types_for_buttons.size()) {
      RecordDeviceNotificationUserActionMetric(
          uma_types_for_buttons.at(button_index.value()));
    }
  }

  GetNotificationDisplayService()->Close(NotificationHandler::Type::TRANSIENT,
                                         kRemovableNotificationId);
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeMountErrorNotification(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  std::unique_ptr<message_center::Notification> notification;
  std::vector<message_center::ButtonInfo> notification_buttons;
  std::vector<DeviceNotificationUserActionUmaType> uma_types_for_buttons;
  auto device_mount_status =
      mount_status_.find(volume.storage_device_path().value());
  if (device_mount_status != mount_status_.end()) {
    std::u16string title =
        l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_DETECTION_TITLE);
    std::u16string message;
    switch (device_mount_status->second) {
      // We have either an unsupported or unknown filesystem on the mount.
      case MOUNT_STATUS_ONLY_PARENT_ERROR:
      case MOUNT_STATUS_CHILD_ERROR:
        if (event.status ==
            file_manager_private::
                MOUNT_COMPLETED_STATUS_ERROR_UNSUPPORTED_FILESYSTEM) {
          if (volume.drive_label().empty()) {
            message = l10n_util::GetStringUTF16(
                IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE);
          } else {
            message = l10n_util::GetStringFUTF16(
                IDS_DEVICE_UNSUPPORTED_MESSAGE,
                base::UTF8ToUTF16(volume.drive_label()));
          }
          RecordDeviceNotificationMetric(
              DeviceNotificationUmaType::DEVICE_FAIL);
        } else {
          if (volume.drive_label().empty()) {
            message =
                l10n_util::GetStringUTF16(IDS_DEVICE_UNKNOWN_DEFAULT_MESSAGE);
          } else {
            message = l10n_util::GetStringFUTF16(
                IDS_DEVICE_UNKNOWN_MESSAGE,
                base::UTF8ToUTF16(volume.drive_label()));
          }
          if (!volume.is_read_only()) {
            // Give a format device button on the notification.
            notification_buttons.push_back(message_center::ButtonInfo(
                l10n_util::GetStringUTF16(IDS_DEVICE_UNKNOWN_BUTTON_LABEL)));
            uma_types_for_buttons.push_back(
                DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_FAIL);
            RecordDeviceNotificationMetric(
                DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN);
          } else {
            RecordDeviceNotificationMetric(
                DeviceNotificationUmaType::DEVICE_FAIL_UNKNOWN_READONLY);
          }
        }
        break;
      // We have a multi-partition device for which at least one mount
      // failed.
      case MOUNT_STATUS_MULTIPART_ERROR:
        if (volume.drive_label().empty()) {
          message = l10n_util::GetStringUTF16(
              IDS_MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE);
        } else {
          message = l10n_util::GetStringFUTF16(
              IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE,
              base::UTF8ToUTF16(volume.drive_label()));
        }
        RecordDeviceNotificationMetric(DeviceNotificationUmaType::DEVICE_FAIL);
        break;
      default:
        DLOG(WARNING) << "Unhandled mount status for "
                      << device_mount_status->second;
        return notification;
    }
    scoped_refptr<message_center::NotificationDelegate> delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(
                &SystemNotificationManager::HandleRemovableNotificationClick,
                weak_ptr_factory_.GetWeakPtr(), volume.mount_path().value(),
                uma_types_for_buttons));
    notification =
        CreateNotification(kDeviceFailNotificationId, title, message, delegate);
    DCHECK_EQ(notification_buttons.size(), uma_types_for_buttons.size());
    notification->set_buttons(notification_buttons);
  }
  return notification;
}

enum SystemNotificationManagerMountStatus
SystemNotificationManager::UpdateDeviceMountStatus(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  enum SystemNotificationManagerMountStatus status = MOUNT_STATUS_NO_RESULT;
  const std::string& device_path = volume.storage_device_path().value();
  auto device_mount_status = mount_status_.find(device_path);
  if (device_mount_status == mount_status_.end()) {
    status = MOUNT_STATUS_NO_RESULT;
  } else {
    status = device_mount_status->second;
  }
  switch (status) {
    case MOUNT_STATUS_MULTIPART_ERROR:
      // Do nothing, status has already been detected.
      break;
    case MOUNT_STATUS_ONLY_PARENT_ERROR:
      if (!volume.is_parent()) {
        // Hide Device Fail notification.
        GetNotificationDisplayService()->Close(
            NotificationHandler::Type::TRANSIENT, kDeviceFailNotificationId);
      }
      [[fallthrough]];
    case MOUNT_STATUS_NO_RESULT:
      if (event.status ==
          file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS) {
        status = MOUNT_STATUS_SUCCESS;
      } else if (event.volume_metadata.is_parent_device) {
        status = MOUNT_STATUS_ONLY_PARENT_ERROR;
      } else {
        status = MOUNT_STATUS_CHILD_ERROR;
      }
      break;
    case MOUNT_STATUS_SUCCESS:
    case MOUNT_STATUS_CHILD_ERROR:
      if (status == MOUNT_STATUS_SUCCESS &&
          event.status ==
              file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS) {
        status = MOUNT_STATUS_SUCCESS;
      } else {
        // Multi partition device with at least one partition in error.
        status = MOUNT_STATUS_MULTIPART_ERROR;
      }
      break;
  }
  mount_status_[device_path] = status;

  return status;
}

std::unique_ptr<message_center::Notification>
SystemNotificationManager::MakeRemovableNotification(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  std::unique_ptr<message_center::Notification> notification;
  if (event.status == file_manager_private::MOUNT_COMPLETED_STATUS_SUCCESS) {
    bool show_settings_button = false;
    std::u16string title =
        l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_DETECTION_TITLE);
    std::u16string message;
    std::vector<DeviceNotificationUserActionUmaType> uma_types_for_buttons;
    if (volume.is_read_only() && !volume.is_read_only_removable_device()) {
      message = l10n_util::GetStringUTF16(
          IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE_READONLY_POLICY);
      RecordDeviceNotificationMetric(
          DeviceNotificationUmaType::DEVICE_NAVIGATION_READONLY_POLICY);
      uma_types_for_buttons.push_back(
          DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION);
    } else {
      const PrefService* const service = profile_->GetPrefs();
      DCHECK(service);
      bool arc_enabled = service->GetBoolean(arc::prefs::kArcEnabled);
      bool arc_removable_media_access_enabled =
          service->GetBoolean(arc::prefs::kArcHasAccessToRemovableMedia);
      if (!arc_enabled) {
        message =
            l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE);
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::DEVICE_NAVIGATION);
        uma_types_for_buttons.push_back(
            DeviceNotificationUserActionUmaType::OPEN_MEDIA_DEVICE_NAVIGATION);
      } else if (arc_removable_media_access_enabled) {
        message = base::StrCat(
            {l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE),
             u" ",
             l10n_util::GetStringUTF16(
                 IDS_REMOVABLE_DEVICE_PLAY_STORE_APPS_HAVE_ACCESS_MESSAGE)});
        show_settings_button = true;
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::DEVICE_NAVIGATION_APPS_HAVE_ACCESS);
        uma_types_for_buttons.insert(uma_types_for_buttons.end(),
                                     {DeviceNotificationUserActionUmaType::
                                          OPEN_MEDIA_DEVICE_NAVIGATION_ARC,
                                      DeviceNotificationUserActionUmaType::
                                          OPEN_SETTINGS_FOR_ARC_STORAGE});
      } else {
        message = base::StrCat(
            {l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_NAVIGATION_MESSAGE),
             u" ",
             l10n_util::GetStringUTF16(
                 IDS_REMOVABLE_DEVICE_ALLOW_PLAY_STORE_ACCESS_MESSAGE)});
        show_settings_button = true;
        RecordDeviceNotificationMetric(
            DeviceNotificationUmaType::DEVICE_NAVIGATION_ALLOW_APP_ACCESS);
        uma_types_for_buttons.insert(uma_types_for_buttons.end(),
                                     {DeviceNotificationUserActionUmaType::
                                          OPEN_MEDIA_DEVICE_NAVIGATION_ARC,
                                      DeviceNotificationUserActionUmaType::
                                          OPEN_SETTINGS_FOR_ARC_STORAGE});
      }
    }
    scoped_refptr<message_center::NotificationDelegate> delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(
                &SystemNotificationManager::HandleRemovableNotificationClick,
                weak_ptr_factory_.GetWeakPtr(), volume.mount_path().value(),
                uma_types_for_buttons));
    notification =
        CreateNotification(kRemovableNotificationId, title, message, delegate);
    std::vector<message_center::ButtonInfo> notification_buttons;
    notification_buttons.push_back(
        message_center::ButtonInfo(l10n_util::GetStringUTF16(
            IDS_REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL)));
    if (show_settings_button) {
      notification_buttons.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_REMOVABLE_DEVICE_OPEN_SETTTINGS_BUTTON_LABEL)));
    }
    DCHECK_EQ(notification_buttons.size(), uma_types_for_buttons.size());
    notification->set_buttons(notification_buttons);
  }
  if (volume.device_type() != chromeos::DEVICE_TYPE_UNKNOWN &&
      !volume.storage_device_path().empty()) {
    if (UpdateDeviceMountStatus(event, volume) != MOUNT_STATUS_SUCCESS) {
      notification = MakeMountErrorNotification(event, volume);
    }
  }

  return notification;
}

void SystemNotificationManager::HandleMountCompletedEvent(
    file_manager_private::MountCompletedEvent& event,
    const Volume& volume) {
  if (!swa_enabled_) {
    return;
  }
  std::unique_ptr<message_center::Notification> notification;

  switch (event.event_type) {
    case file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT:
      if (event.should_notify) {
        notification = MakeRemovableNotification(event, volume);
      }
      break;
    case file_manager_private::MOUNT_COMPLETED_EVENT_TYPE_UNMOUNT:
      GetNotificationDisplayService()->Close(
          NotificationHandler::Type::TRANSIENT, kRemovableNotificationId);

      if (volume.device_type() != chromeos::DEVICE_TYPE_UNKNOWN &&
          !volume.storage_device_path().empty()) {
        UpdateDeviceMountStatus(event, volume);
      }
      break;
    default:
      DLOG(WARNING) << "Unhandled mount event for type " << event.event_type;
      break;
  }

  if (notification) {
    GetNotificationDisplayService()->Display(
        NotificationHandler::Type::TRANSIENT, *notification,
        /*metadata=*/nullptr);
  }
}

NotificationDisplayService*
SystemNotificationManager::GetNotificationDisplayService() {
  return NotificationDisplayServiceFactory::GetForProfile(profile_);
}

void SystemNotificationManager::SetDriveFSEventRouter(
    DriveFsEventRouter* drivefs_event_router) {
  drivefs_event_router_ = drivefs_event_router;
}

void SystemNotificationManager::SetIOTaskController(
    file_manager::io_task::IOTaskController* io_task_controller) {
  io_task_controller_ = io_task_controller;
}

}  // namespace file_manager
