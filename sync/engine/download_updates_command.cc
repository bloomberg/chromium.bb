// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download_updates_command.h"

#include <string>

#include "base/command_line.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/syncable_read_transaction.h"

using sync_pb::DebugInfo;

namespace syncer {
using sessions::StatusController;
using sessions::SyncSession;
using std::string;

DownloadUpdatesCommand::DownloadUpdatesCommand(
    bool create_mobile_bookmarks_folder)
    : create_mobile_bookmarks_folder_(create_mobile_bookmarks_folder) {}

DownloadUpdatesCommand::~DownloadUpdatesCommand() {}

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

}  // namespace

SyncerError DownloadUpdatesCommand::ExecuteImpl(SyncSession* session) {
  sync_pb::ClientToServerMessage client_to_server_message;
  sync_pb::ClientToServerResponse update_response;

  client_to_server_message.set_share(session->context()->account_name());
  client_to_server_message.set_message_contents(
      sync_pb::ClientToServerMessage::GET_UPDATES);
  sync_pb::GetUpdatesMessage* get_updates =
      client_to_server_message.mutable_get_updates();
  get_updates->set_create_mobile_bookmarks_folder(
      create_mobile_bookmarks_folder_);

  syncable::Directory* dir = session->context()->directory();

  // Request updates for all enabled types.
  const ModelTypeSet enabled_types =
      GetRoutingInfoTypes(session->context()->routing_info());
  DVLOG(1) << "Getting updates for types "
           << ModelTypeSetToString(enabled_types);
  DCHECK(!enabled_types.Empty());

  const ModelTypeInvalidationMap& invalidation_map =
      session->source().types;
  for (ModelTypeSet::Iterator it = enabled_types.First();
       it.Good(); it.Inc()) {
    if (ProxyTypes().Has(it.Get()))
      continue;
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    dir->GetDownloadProgress(it.Get(), progress_marker);

    // Set notification hint if present.
    ModelTypeInvalidationMap::const_iterator find_it =
        invalidation_map.find(it.Get());
    if (find_it != invalidation_map.end()) {
      progress_marker->set_notification_hint(find_it->second.payload);
    }
  }

  bool need_encryption_key = false;
  if (session->context()->keystore_encryption_enabled()) {
    syncable::Directory* dir = session->context()->directory();
    syncable::ReadTransaction trans(FROM_HERE, dir);
    syncable::NigoriHandler* nigori_handler = dir->GetNigoriHandler();
    need_encryption_key = nigori_handler->NeedKeystoreKey(&trans);
    get_updates->set_need_encryption_key(need_encryption_key);

  }

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  // Set GetUpdatesMessage.GetUpdatesCallerInfo information.
  get_updates->mutable_caller_info()->set_source(
      session->source().updates_source);
  get_updates->mutable_caller_info()->set_notifications_enabled(
      session->context()->notifications_enabled());

  DebugInfo* debug_info = client_to_server_message.mutable_debug_info();

  AppendClientDebugInfoIfNeeded(session, debug_info);

  SyncerError result = SyncerProtoUtil::PostClientToServerMessage(
      &client_to_server_message,
      &update_response,
      session);

  DVLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  StatusController* status = session->mutable_status_controller();
  status->set_updates_request_types(enabled_types);
  if (result != SYNCER_OK) {
    status->mutable_updates_response()->Clear();
    LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates";
    return result;
  }

  status->mutable_updates_response()->CopyFrom(update_response);

  DVLOG(1) << "GetUpdates "
           << " returned " << update_response.get_updates().entries_size()
           << " updates and indicated "
           << update_response.get_updates().changes_remaining()
           << " updates left on server.";

  if (need_encryption_key ||
      update_response.get_updates().encryption_keys_size() > 0) {
    status->set_last_get_key_result(
        HandleGetEncryptionKeyResponse(update_response, dir));
  }

  return result;
}

void DownloadUpdatesCommand::AppendClientDebugInfoIfNeeded(
    sessions::SyncSession* session,
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
