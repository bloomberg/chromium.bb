// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "chrome/common/apps/platform_apps/api/sync_file_system.h"
#include "components/drive/drive_notification_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/common/time_util.h"

using drive::EventLogger;
using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

SyncFileSystemInternalsHandler::SyncFileSystemInternalsHandler(Profile* profile)
    : profile_(profile),
      observing_task_log_(false) {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  if (sync_service)
    sync_service->AddSyncEventObserver(this);
}

SyncFileSystemInternalsHandler::~SyncFileSystemInternalsHandler() {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (!sync_service)
    return;
  sync_service->RemoveSyncEventObserver(this);
  if (observing_task_log_)
    sync_service->task_logger()->RemoveObserver(this);
}

void SyncFileSystemInternalsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getServiceStatus",
      base::BindRepeating(
          &SyncFileSystemInternalsHandler::HandleGetServiceStatus,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getLog",
      base::BindRepeating(&SyncFileSystemInternalsHandler::HandleGetLog,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "clearLogs",
      base::BindRepeating(&SyncFileSystemInternalsHandler::HandleClearLogs,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getNotificationSource",
      base::BindRepeating(
          &SyncFileSystemInternalsHandler::HandleGetNotificationSource,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "observeTaskLog",
      base::BindRepeating(&SyncFileSystemInternalsHandler::HandleObserveTaskLog,
                          base::Unretained(this)));
}

void SyncFileSystemInternalsHandler::OnSyncStateUpdated(
    const GURL& app_origin,
    sync_file_system::SyncServiceState state,
    const std::string& description) {
  if (!IsJavascriptAllowed()) {
    // Javascript is disallowed, either due to the page still loading, or in the
    // process of being unloaded. Skip this update.
    return;
  }
  std::string state_string = chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncServiceStateToExtensionEnum(state));
  if (!description.empty())
    state_string += " (" + description + ")";

  // TODO(calvinlo): OnSyncStateUpdated should be updated to also provide the
  // notification mechanism (XMPP or Polling).
  FireWebUIListener("service-status-changed", base::Value(state_string));
}

void SyncFileSystemInternalsHandler::OnFileSynced(
    const storage::FileSystemURL& url,
    sync_file_system::SyncFileType file_type,
    sync_file_system::SyncFileStatus status,
    sync_file_system::SyncAction action,
    sync_file_system::SyncDirection direction) {
}

void SyncFileSystemInternalsHandler::OnLogRecorded(
    const sync_file_system::TaskLogger::TaskLog& task_log) {
  base::DictionaryValue dict;
  int64_t duration = (task_log.end_time - task_log.start_time).InMilliseconds();
  dict.SetInteger("duration", duration);
  dict.SetString("task_description", task_log.task_description);
  dict.SetString("result_description", task_log.result_description);

  base::ListValue details;
  for (const std::string& detail : task_log.details) {
    details.Append(detail);
  }
  dict.SetKey("details", std::move(details));
  FireWebUIListener("task-log-recorded", dict);
}

void SyncFileSystemInternalsHandler::HandleGetServiceStatus(
    const base::ListValue* args) {
  AllowJavascript();
  SyncServiceState state_enum = sync_file_system::SYNC_SERVICE_DISABLED;
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (sync_service)
    state_enum = sync_service->GetSyncServiceState();
  const std::string state_string = chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncServiceStateToExtensionEnum(state_enum));
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            base::Value(state_string));
}

void SyncFileSystemInternalsHandler::HandleGetNotificationSource(
    const base::ListValue* args) {
  AllowJavascript();
  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::FindForBrowserContext(profile_);
  if (!drive_notification_manager)
    return;
  bool xmpp_enabled = drive_notification_manager->push_notification_enabled();
  std::string notification_source = xmpp_enabled ? "XMPP" : "Polling";
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            base::Value(notification_source));
}

void SyncFileSystemInternalsHandler::HandleGetLog(const base::ListValue* args) {
  AllowJavascript();
  const auto& args_list = args->GetList();
  DCHECK_GE(args_list.size(), 1u);
  const base::Value& callback_id = args_list[0];
  const std::vector<EventLogger::Event> log =
      sync_file_system::util::GetLogHistory();

  int last_log_id_sent = -1;
  if (args_list.size() >= 2 && args_list[1].is_int())
    last_log_id_sent = args_list[1].GetInt();

  // Collate events which haven't been sent to WebUI yet.
  base::Value list(base::Value::Type::LIST);
  for (auto log_entry = log.begin(); log_entry != log.end(); ++log_entry) {
    if (log_entry->id <= last_log_id_sent)
      continue;

    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetIntKey("id", log_entry->id);
    dict.SetStringKey("time", google_apis::util::FormatTimeAsStringLocaltime(
                                  log_entry->when));
    dict.SetStringKey("logEvent", log_entry->what);
    list.Append(std::move(dict));
    last_log_id_sent = log_entry->id;
  }
  if (list.GetList().empty())
    return;

  ResolveJavascriptCallback(callback_id, list);
}

void SyncFileSystemInternalsHandler::HandleClearLogs(
    const base::ListValue* args) {
  sync_file_system::util::ClearLog();
}

void SyncFileSystemInternalsHandler::HandleObserveTaskLog(
    const base::ListValue* args) {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (!sync_service)
    return;
  if (!observing_task_log_) {
    observing_task_log_ = true;
    sync_service->task_logger()->AddObserver(this);
  }

  DCHECK(sync_service->task_logger());
  const sync_file_system::TaskLogger::LogList& log =
      sync_service->task_logger()->GetLog();

  for (sync_file_system::TaskLogger::LogList::const_iterator itr = log.begin();
       itr != log.end(); ++itr)
    OnLogRecorded(**itr);
}

}  // namespace syncfs_internals
