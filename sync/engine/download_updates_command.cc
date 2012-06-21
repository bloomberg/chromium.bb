// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download_updates_command.h"

#include <string>

#include "base/command_line.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/engine/syncproto.h"
#include "sync/internal_api/public/syncable/model_type_payload_map.h"
#include "sync/syncable/directory.h"

using sync_pb::DebugInfo;

namespace csync {
using sessions::StatusController;
using sessions::SyncSession;
using std::string;
using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;
using syncable::ModelTypeSet;
using syncable::ModelTypeSetToString;

DownloadUpdatesCommand::DownloadUpdatesCommand(
    bool create_mobile_bookmarks_folder)
    : create_mobile_bookmarks_folder_(create_mobile_bookmarks_folder) {}

DownloadUpdatesCommand::~DownloadUpdatesCommand() {}

SyncerError DownloadUpdatesCommand::ExecuteImpl(SyncSession* session) {
  ClientToServerMessage client_to_server_message;
  ClientToServerResponse update_response;

  client_to_server_message.set_share(session->context()->account_name());
  client_to_server_message.set_message_contents(
      ClientToServerMessage::GET_UPDATES);
  GetUpdatesMessage* get_updates =
      client_to_server_message.mutable_get_updates();
  get_updates->set_create_mobile_bookmarks_folder(
      create_mobile_bookmarks_folder_);

  syncable::Directory* dir = session->context()->directory();

  // Request updates for all enabled types.
  const ModelTypeSet enabled_types =
      GetRoutingInfoTypes(session->routing_info());
  DVLOG(1) << "Getting updates for types "
           << ModelTypeSetToString(enabled_types);
  DCHECK(!enabled_types.Empty());

  const syncable::ModelTypePayloadMap& type_payload_map =
      session->source().types;
  for (ModelTypeSet::Iterator it = enabled_types.First();
       it.Good(); it.Inc()) {
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    dir->GetDownloadProgress(it.Get(), progress_marker);

    // Set notification hint if present.
    syncable::ModelTypePayloadMap::const_iterator type_payload =
        type_payload_map.find(it.Get());
    if (type_payload != type_payload_map.end()) {
      progress_marker->set_notification_hint(type_payload->second);
    }
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

  SyncerProtoUtil::AddRequestBirthday(dir, &client_to_server_message);

  DebugInfo* debug_info = client_to_server_message.mutable_debug_info();

  AppendClientDebugInfoIfNeeded(session, debug_info);

  SyncerError result = SyncerProtoUtil::PostClientToServerMessage(
      client_to_server_message,
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


}  // namespace csync
