// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download.h"

#include <string>

#include "base/command_line.h"
#include "sync/engine/process_updates_command.h"
#include "sync/engine/store_timestamps_command.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/syncable_read_transaction.h"

using sync_pb::DebugInfo;

namespace syncer {

using sessions::StatusController;
using sessions::SyncSession;
using sessions::SyncSessionContext;
using std::string;

namespace {

SyncerError HandleGetEncryptionKeyResponse(
    const sync_pb::ClientToServerResponse& update_response,
    syncable::Directory* dir) {
  bool success = false;
  if (update_response.get_updates().encryption_keys_size() == 0) {
    LOG(ERROR) << "Failed to receive encryption key from server.";
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }
  syncable::ReadTransaction trans(FROM_HERE, dir);
  syncable::NigoriHandler* nigori_handler = dir->GetNigoriHandler();
  success = nigori_handler->SetKeystoreKeys(
      update_response.get_updates().encryption_keys(),
      &trans);

  DVLOG(1) << "GetUpdates returned "
           << update_response.get_updates().encryption_keys_size()
           << "encryption keys. Nigori keystore key "
           << (success ? "" : "not ") << "updated.";
  return (success ? SYNCER_OK : SERVER_RESPONSE_VALIDATION_FAILED);
}

sync_pb::SyncEnums::GetUpdatesOrigin ConvertConfigureSourceToOrigin(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  switch (source) {
    // Configurations:
    case sync_pb::GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE:
      return sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE;
    case sync_pb::GetUpdatesCallerInfo::MIGRATION:
      return sync_pb::SyncEnums::MIGRATION;
    case sync_pb::GetUpdatesCallerInfo::RECONFIGURATION:
      return sync_pb::SyncEnums::RECONFIGURATION;
    case sync_pb::GetUpdatesCallerInfo::NEW_CLIENT:
      return sync_pb::SyncEnums::NEW_CLIENT;
    default:
      NOTREACHED();
      return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
  }
}

bool ShouldRequestEncryptionKey(
    SyncSessionContext* context) {
  bool need_encryption_key = false;
  if (context->keystore_encryption_enabled()) {
    syncable::Directory* dir = context->directory();
    syncable::ReadTransaction trans(FROM_HERE, dir);
    syncable::NigoriHandler* nigori_handler = dir->GetNigoriHandler();
    need_encryption_key = nigori_handler->NeedKeystoreKey(&trans);
  }
  return need_encryption_key;
}

SyncerError ExecuteDownloadUpdates(
    SyncSession* session,
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::ClientToServerResponse update_response;
  StatusController* status = session->mutable_status_controller();
  bool need_encryption_key = ShouldRequestEncryptionKey(session->context());

  SyncerError result = SyncerProtoUtil::PostClientToServerMessage(
      msg,
      &update_response,
      session);

  DVLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  if (result != SYNCER_OK) {
    status->mutable_updates_response()->Clear();
    LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates";
  } else {
    status->mutable_updates_response()->CopyFrom(update_response);

    DVLOG(1) << "GetUpdates "
             << " returned " << update_response.get_updates().entries_size()
             << " updates and indicated "
             << update_response.get_updates().changes_remaining()
             << " updates left on server.";

    if (need_encryption_key ||
        update_response.get_updates().encryption_keys_size() > 0) {
      syncable::Directory* dir = session->context()->directory();
      status->set_last_get_key_result(
          HandleGetEncryptionKeyResponse(update_response, dir));
    }
  }

  ProcessUpdatesCommand process_updates;
  process_updates.Execute(session);

  StoreTimestampsCommand store_timestamps;
  store_timestamps.Execute(session);

  return result;
}

void InitDownloadUpdatesRequest(
    SyncSession* session,
    bool create_mobile_bookmarks_folder,
    sync_pb::ClientToServerMessage* message,
    ModelTypeSet request_types) {
  message->set_share(session->context()->account_name());
  message->set_message_contents(sync_pb::ClientToServerMessage::GET_UPDATES);

  sync_pb::GetUpdatesMessage* get_updates = message->mutable_get_updates();

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  DebugInfo* debug_info = message->mutable_debug_info();
  AppendClientDebugInfoIfNeeded(session, debug_info);

  get_updates->set_create_mobile_bookmarks_folder(
      create_mobile_bookmarks_folder);
  bool need_encryption_key = ShouldRequestEncryptionKey(session->context());
  get_updates->set_need_encryption_key(need_encryption_key);

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_notifications_enabled(
      session->context()->notifications_enabled());

  StatusController* status = session->mutable_status_controller();
  status->set_updates_request_types(request_types);

  syncable::Directory* dir = session->context()->directory();
  for (ModelTypeSet::Iterator it = request_types.First();
       it.Good(); it.Inc()) {
    if (ProxyTypes().Has(it.Get()))
      continue;
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    dir->GetDownloadProgress(it.Get(), progress_marker);
  }
}

}  // namespace

SyncerError NormalDownloadUpdates(
    SyncSession* session,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    const sessions::NudgeTracker& nudge_tracker) {
  sync_pb::ClientToServerMessage client_to_server_message;
  InitDownloadUpdatesRequest(
      session,
      create_mobile_bookmarks_folder,
      &client_to_server_message,
      request_types);
  sync_pb::GetUpdatesMessage* get_updates =
      client_to_server_message.mutable_get_updates();

  // Request updates for all requested types.
  DVLOG(1) << "Getting updates for types "
           << ModelTypeSetToString(request_types);
  DCHECK(!request_types.Empty());

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      nudge_tracker.updates_source());

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::GU_TRIGGER);

  // Fill in the notification hints.
  for (int i = 0; i < get_updates->from_progress_marker_size(); ++i) {
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->mutable_from_progress_marker(i);
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        progress_marker->data_type_id());

    DCHECK(!nudge_tracker.IsTypeThrottled(type))
        << "Throttled types should have been removed from the request_types.";

    nudge_tracker.SetLegacyNotificationHint(type, progress_marker);
    nudge_tracker.FillProtoMessage(
        type,
        progress_marker->mutable_get_update_triggers());
  }

  return ExecuteDownloadUpdates(session, &client_to_server_message);
}

SyncerError DownloadUpdatesForConfigure(
    SyncSession* session,
    bool create_mobile_bookmarks_folder,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    ModelTypeSet request_types) {
  sync_pb::ClientToServerMessage client_to_server_message;
  InitDownloadUpdatesRequest(
      session,
      create_mobile_bookmarks_folder,
      &client_to_server_message,
      request_types);
  sync_pb::GetUpdatesMessage* get_updates =
      client_to_server_message.mutable_get_updates();

  // Request updates for all enabled types.
  DVLOG(1) << "Initial download for types "
           << ModelTypeSetToString(request_types);
  DCHECK(!request_types.Empty());

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(source);

  // Set the new and improved version of source, too.
  sync_pb::SyncEnums::GetUpdatesOrigin origin =
      ConvertConfigureSourceToOrigin(source);
  get_updates->set_get_updates_origin(origin);

  return ExecuteDownloadUpdates(session, &client_to_server_message);
}

SyncerError DownloadUpdatesForPoll(
    SyncSession* session,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types) {
  sync_pb::ClientToServerMessage client_to_server_message;
  InitDownloadUpdatesRequest(
      session,
      create_mobile_bookmarks_folder,
      &client_to_server_message,
      request_types);
  sync_pb::GetUpdatesMessage* get_updates =
      client_to_server_message.mutable_get_updates();

  DVLOG(1) << "Polling for types "
           << ModelTypeSetToString(request_types);
  DCHECK(!request_types.Empty());

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      sync_pb::GetUpdatesCallerInfo::PERIODIC);

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::PERIODIC);

  return ExecuteDownloadUpdates(session, &client_to_server_message);
}

void AppendClientDebugInfoIfNeeded(
    SyncSession* session,
    DebugInfo* debug_info) {
  // We want to send the debug info only once per sync cycle. Check if it has
  // already been sent.
  if (!session->status_controller().debug_info_sent()) {
    DVLOG(1) << "Sending client debug info ...";
    // could be null in some unit tests.
    if (session->context()->debug_info_getter()) {
      session->context()->debug_info_getter()->GetAndClearDebugInfo(
          debug_info);
    }
    session->mutable_status_controller()->set_debug_info_sent();
  }
}

}  // namespace syncer
