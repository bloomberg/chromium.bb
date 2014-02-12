// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download.h"

#include <string>

#include "sync/engine/syncer_proto_util.h"
#include "sync/sessions/sync_session.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/syncable_read_transaction.h"

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

}  // namespace

namespace download {

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

  return ProcessResponse(update_response.get_updates(),
                         request_types,
                         get_updates_processor,
                         status);
}

SyncerError ProcessResponse(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet request_types,
    GetUpdatesProcessor* get_updates_processor,
    StatusController* status) {
  status->increment_num_updates_downloaded_by(gu_response.entries_size());

  // The changes remaining field is used to prevent the client from looping.  If
  // that field is being set incorrectly, we're in big trouble.
  if (!gu_response.has_changes_remaining()) {
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }
  status->set_num_server_changes_remaining(gu_response.changes_remaining());


  if (!get_updates_processor->ProcessGetUpdatesResponse(request_types,
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
