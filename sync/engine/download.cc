// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download.h"

#include <string>

#include "base/command_line.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/syncable_read_transaction.h"

namespace syncer {

using sessions::StatusController;
using sessions::SyncSession;
using sessions::SyncSessionContext;
using std::string;

namespace download {

namespace {

typedef std::map<ModelType, size_t> TypeToIndexMap;

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

void InitDownloadUpdatesContext(
    SyncSession* session,
    bool create_mobile_bookmarks_folder,
    sync_pb::ClientToServerMessage* message) {
  message->set_share(session->context()->account_name());
  message->set_message_contents(sync_pb::ClientToServerMessage::GET_UPDATES);

  sync_pb::GetUpdatesMessage* get_updates = message->mutable_get_updates();

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  get_updates->set_create_mobile_bookmarks_folder(
      create_mobile_bookmarks_folder);
  bool need_encryption_key = ShouldRequestEncryptionKey(session->context());
  get_updates->set_need_encryption_key(need_encryption_key);

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_notifications_enabled(
      session->context()->notifications_enabled());
}

}  // namespace

void BuildNormalDownloadUpdates(
    SyncSession* session,
    GetUpdatesProcessor* get_updates_processor,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    const sessions::NudgeTracker& nudge_tracker,
    sync_pb::ClientToServerMessage* client_to_server_message) {
  // Request updates for all requested types.
  DVLOG(1) << "Getting updates for types "
           << ModelTypeSetToString(request_types);
  DCHECK(!request_types.Empty());

  InitDownloadUpdatesContext(
      session,
      create_mobile_bookmarks_folder,
      client_to_server_message);

  BuildNormalDownloadUpdatesImpl(
      Intersection(request_types, ProtocolTypes()),
      get_updates_processor,
      nudge_tracker,
      client_to_server_message->mutable_get_updates());
}

void BuildNormalDownloadUpdatesImpl(
    ModelTypeSet proto_request_types,
    GetUpdatesProcessor* get_updates_processor,
    const sessions::NudgeTracker& nudge_tracker,
    sync_pb::GetUpdatesMessage* get_updates) {
  DCHECK(!proto_request_types.Empty());

  // Get progress markers and other data for requested types.
  get_updates_processor->PrepareGetUpdates(proto_request_types, get_updates);

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      nudge_tracker.updates_source());

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::GU_TRIGGER);
  get_updates->set_is_retry(nudge_tracker.IsRetryRequired());

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
}

void BuildDownloadUpdatesForConfigure(
    SyncSession* session,
    GetUpdatesProcessor* get_updates_processor,
    bool create_mobile_bookmarks_folder,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    ModelTypeSet request_types,
    sync_pb::ClientToServerMessage* client_to_server_message) {
  // Request updates for all enabled types.
  DVLOG(1) << "Initial download for types "
           << ModelTypeSetToString(request_types);

  InitDownloadUpdatesContext(
      session,
      create_mobile_bookmarks_folder,
      client_to_server_message);
  BuildDownloadUpdatesForConfigureImpl(
      Intersection(request_types, ProtocolTypes()),
      get_updates_processor,
      source,
      client_to_server_message->mutable_get_updates());
}

void BuildDownloadUpdatesForConfigureImpl(
    ModelTypeSet proto_request_types,
    GetUpdatesProcessor* get_updates_processor,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sync_pb::GetUpdatesMessage* get_updates) {
  DCHECK(!proto_request_types.Empty());

  // Get progress markers and other data for requested types.
  get_updates_processor->PrepareGetUpdates(proto_request_types, get_updates);

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(source);

  // Set the new and improved version of source, too.
  sync_pb::SyncEnums::GetUpdatesOrigin origin =
      ConvertConfigureSourceToOrigin(source);
  get_updates->set_get_updates_origin(origin);
}

void BuildDownloadUpdatesForPoll(
    SyncSession* session,
    GetUpdatesProcessor* get_updates_processor,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    sync_pb::ClientToServerMessage* client_to_server_message) {
  DVLOG(1) << "Polling for types "
           << ModelTypeSetToString(request_types);

  InitDownloadUpdatesContext(
      session,
      create_mobile_bookmarks_folder,
      client_to_server_message);
  BuildDownloadUpdatesForPollImpl(
      Intersection(request_types, ProtocolTypes()),
      get_updates_processor,
      client_to_server_message->mutable_get_updates());
}

void BuildDownloadUpdatesForPollImpl(
    ModelTypeSet proto_request_types,
    GetUpdatesProcessor* get_updates_processor,
    sync_pb::GetUpdatesMessage* get_updates) {
  DCHECK(!proto_request_types.Empty());

  // Get progress markers and other data for requested types.
  get_updates_processor->PrepareGetUpdates(proto_request_types, get_updates);

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      sync_pb::GetUpdatesCallerInfo::PERIODIC);

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::PERIODIC);
}

void BuildDownloadUpdatesForRetry(
    SyncSession* session,
    GetUpdatesProcessor* get_updates_processor,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    sync_pb::ClientToServerMessage* client_to_server_message) {
  DVLOG(1) << "Retrying for types "
           << ModelTypeSetToString(request_types);

  InitDownloadUpdatesContext(
      session,
      create_mobile_bookmarks_folder,
      client_to_server_message);
  BuildDownloadUpdatesForRetryImpl(
      Intersection(request_types, ProtocolTypes()),
      get_updates_processor,
      client_to_server_message->mutable_get_updates());
}

void BuildDownloadUpdatesForRetryImpl(
    ModelTypeSet proto_request_types,
    GetUpdatesProcessor* get_updates_processor,
    sync_pb::GetUpdatesMessage* get_updates) {
  DCHECK(!proto_request_types.Empty());

  get_updates_processor->PrepareGetUpdates(proto_request_types, get_updates);

  // Set legacy GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      sync_pb::GetUpdatesCallerInfo::RETRY);

  // Set the new and improved version of source, too.
  get_updates->set_get_updates_origin(sync_pb::SyncEnums::RETRY);
  get_updates->set_is_retry(true);
}

SyncerError ExecuteDownloadUpdates(
    ModelTypeSet request_types,
    SyncSession* session,
    GetUpdatesProcessor* get_updates_processor,
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::ClientToServerResponse update_response;
  StatusController* status = session->mutable_status_controller();
  bool need_encryption_key = ShouldRequestEncryptionKey(session->context());

  if (session->context()->debug_info_getter()) {
    sync_pb::DebugInfo* debug_info = msg->mutable_debug_info();
    CopyClientDebugInfo(session->context()->debug_info_getter(), debug_info);
  }

  SyncerError result = SyncerProtoUtil::PostClientToServerMessage(
      msg,
      &update_response,
      session);

  DVLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  if (result != SYNCER_OK) {
    LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates";
    return result;
  }

  DVLOG(1) << "GetUpdates "
           << " returned " << update_response.get_updates().entries_size()
           << " updates and indicated "
           << update_response.get_updates().changes_remaining()
           << " updates left on server.";

  if (session->context()->debug_info_getter()) {
    // Clear debug info now that we have successfully sent it to the server.
    DVLOG(1) << "Clearing client debug info.";
    session->context()->debug_info_getter()->ClearDebugInfo();
  }

  if (need_encryption_key ||
      update_response.get_updates().encryption_keys_size() > 0) {
    syncable::Directory* dir = session->context()->directory();
    status->set_last_get_key_result(
        HandleGetEncryptionKeyResponse(update_response, dir));
  }

  const ModelTypeSet proto_request_types =
      Intersection(request_types, ProtocolTypes());

  return ProcessResponse(update_response.get_updates(),
                         proto_request_types,
                         get_updates_processor,
                         status);
}

SyncerError ProcessResponse(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet proto_request_types,
    GetUpdatesProcessor* get_updates_processor,
    StatusController* status) {
  status->increment_num_updates_downloaded_by(gu_response.entries_size());

  // The changes remaining field is used to prevent the client from looping.  If
  // that field is being set incorrectly, we're in big trouble.
  if (!gu_response.has_changes_remaining()) {
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }
  status->set_num_server_changes_remaining(gu_response.changes_remaining());


  if (!get_updates_processor->ProcessGetUpdatesResponse(proto_request_types,
                                                        gu_response,
                                                        status)) {
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }

  if (gu_response.changes_remaining() == 0) {
    return SYNCER_OK;
  } else {
    return SERVER_MORE_TO_DOWNLOAD;
  }
}

void CopyClientDebugInfo(
    sessions::DebugInfoGetter* debug_info_getter,
    sync_pb::DebugInfo* debug_info) {
  DVLOG(1) << "Copying client debug info to send.";
  debug_info_getter->GetDebugInfo(debug_info);
}

}  // namespace download

}  // namespace syncer
