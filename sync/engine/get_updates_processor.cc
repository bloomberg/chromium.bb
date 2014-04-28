// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/get_updates_processor.h"

#include <map>

#include "base/debug/trace_event.h"
#include "sync/engine/get_updates_delegate.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/engine/update_handler.h"
#include "sync/internal_api/public/events/get_updates_response_event.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/syncable_read_transaction.h"

typedef std::vector<const sync_pb::SyncEntity*> SyncEntityList;
typedef std::map<syncer::ModelType, SyncEntityList> TypeSyncEntityMap;

namespace syncer {

typedef std::map<ModelType, size_t> TypeToIndexMap;

namespace {

bool ShouldRequestEncryptionKey(sessions::SyncSessionContext* context) {
  syncable::Directory* dir = context->directory();
  syncable::ReadTransaction trans(FROM_HERE, dir);
  syncable::NigoriHandler* nigori_handler = dir->GetNigoriHandler();
  return nigori_handler->NeedKeystoreKey(&trans);
}


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

// Given a GetUpdates response, iterates over all the returned items and
// divides them according to their type.  Outputs a map from model types to
// received SyncEntities.  The output map will have entries (possibly empty)
// for all types in |requested_types|.
void PartitionUpdatesByType(const sync_pb::GetUpdatesResponse& gu_response,
                            ModelTypeSet requested_types,
                            TypeSyncEntityMap* updates_by_type) {
  int update_count = gu_response.entries().size();
  for (ModelTypeSet::Iterator it = requested_types.First();
       it.Good(); it.Inc()) {
    updates_by_type->insert(std::make_pair(it.Get(), SyncEntityList()));
  }
  for (int i = 0; i < update_count; ++i) {
    const sync_pb::SyncEntity& update = gu_response.entries(i);
    ModelType type = GetModelType(update);
    if (!IsRealDataType(type)) {
      NOTREACHED() << "Received update with invalid type.";
      continue;
    }

    TypeSyncEntityMap::iterator it = updates_by_type->find(type);
    if (it == updates_by_type->end()) {
      NOTREACHED() << "Received update for unexpected type "
                   << ModelTypeToString(type);
      continue;
    }

    it->second.push_back(&update);
  }
}

// Builds a map of ModelTypes to indices to progress markers in the given
// |gu_response| message.  The map is returned in the |index_map| parameter.
void PartitionProgressMarkersByType(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet request_types,
    TypeToIndexMap* index_map) {
  for (int i = 0; i < gu_response.new_progress_marker_size(); ++i) {
    int field_number = gu_response.new_progress_marker(i).data_type_id();
    ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!IsRealDataType(model_type)) {
      DLOG(WARNING) << "Unknown field number " << field_number;
      continue;
    }
    if (!request_types.Has(model_type)) {
      DLOG(WARNING)
          << "Skipping unexpected progress marker for non-enabled type "
          << ModelTypeToString(model_type);
      continue;
    }
    index_map->insert(std::make_pair(model_type, i));
  }
}

void PartitionContextMutationsByType(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet request_types,
    TypeToIndexMap* index_map) {
  for (int i = 0; i < gu_response.context_mutations_size(); ++i) {
    int field_number = gu_response.context_mutations(i).data_type_id();
    ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!IsRealDataType(model_type)) {
      DLOG(WARNING) << "Unknown field number " << field_number;
      continue;
    }
    if (!request_types.Has(model_type)) {
      DLOG(WARNING)
          << "Skipping unexpected context mutation for non-enabled type "
          << ModelTypeToString(model_type);
      continue;
    }
    index_map->insert(std::make_pair(model_type, i));
  }
}

// Initializes the parts of the GetUpdatesMessage that depend on shared state,
// like the ShouldRequestEncryptionKey() status.  This is kept separate from the
// other of the message-building functions to make the rest of the code easier
// to test.
void InitDownloadUpdatesContext(
    sessions::SyncSession* session,
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

GetUpdatesProcessor::GetUpdatesProcessor(UpdateHandlerMap* update_handler_map,
                                         const GetUpdatesDelegate& delegate)
    : update_handler_map_(update_handler_map), delegate_(delegate) {}

GetUpdatesProcessor::~GetUpdatesProcessor() {}

SyncerError GetUpdatesProcessor::DownloadUpdates(
    ModelTypeSet request_types,
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder) {
  TRACE_EVENT0("sync", "DownloadUpdates");

  sync_pb::ClientToServerMessage message;
  InitDownloadUpdatesContext(session,
                             create_mobile_bookmarks_folder,
                             &message);
  PrepareGetUpdates(request_types, &message);

  SyncerError result = ExecuteDownloadUpdates(request_types, session, &message);
  session->mutable_status_controller()->set_last_download_updates_result(
      result);
  return result;
}

void GetUpdatesProcessor::PrepareGetUpdates(
    ModelTypeSet gu_types,
    sync_pb::ClientToServerMessage* message) {
  sync_pb::GetUpdatesMessage* get_updates = message->mutable_get_updates();

  for (ModelTypeSet::Iterator it = gu_types.First(); it.Good(); it.Inc()) {
    UpdateHandlerMap::iterator handler_it = update_handler_map_->find(it.Get());
    DCHECK(handler_it != update_handler_map_->end())
        << "Failed to look up handler for " << ModelTypeToString(it.Get());
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    handler_it->second->GetDownloadProgress(progress_marker);
    progress_marker->clear_gc_directive();

    sync_pb::DataTypeContext context;
    handler_it->second->GetDataTypeContext(&context);
    if (!context.context().empty())
      get_updates->add_client_contexts()->Swap(&context);
  }

  delegate_.HelpPopulateGuMessage(get_updates);
}

SyncerError GetUpdatesProcessor::ExecuteDownloadUpdates(
    ModelTypeSet request_types,
    sessions::SyncSession* session,
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::ClientToServerResponse update_response;
  sessions::StatusController* status = session->mutable_status_controller();
  bool need_encryption_key = ShouldRequestEncryptionKey(session->context());

  if (session->context()->debug_info_getter()) {
    sync_pb::DebugInfo* debug_info = msg->mutable_debug_info();
    CopyClientDebugInfo(session->context()->debug_info_getter(), debug_info);
  }

  session->SendProtocolEvent(
      *(delegate_.GetNetworkRequestEvent(base::Time::Now(), *msg)));

  SyncerError result = SyncerProtoUtil::PostClientToServerMessage(
      msg,
      &update_response,
      session);

  DVLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  if (result != SYNCER_OK) {
    GetUpdatesResponseEvent response_event(
        base::Time::Now(), update_response, result);
    session->SendProtocolEvent(response_event);

    LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates";
    return result;
  }

  DVLOG(1) << "GetUpdates returned "
           << update_response.get_updates().entries_size()
           << " updates.";


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

  SyncerError process_result = ProcessResponse(update_response.get_updates(),
                                              request_types,
                                              status);

  GetUpdatesResponseEvent response_event(
      base::Time::Now(), update_response, process_result);
  session->SendProtocolEvent(response_event);

  DVLOG(1) << "GetUpdates result: " << process_result;

  return process_result;
}

SyncerError GetUpdatesProcessor::ProcessResponse(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet request_types,
    sessions::StatusController* status) {
  status->increment_num_updates_downloaded_by(gu_response.entries_size());

  // The changes remaining field is used to prevent the client from looping.  If
  // that field is being set incorrectly, we're in big trouble.
  if (!gu_response.has_changes_remaining()) {
    return SERVER_RESPONSE_VALIDATION_FAILED;
  }

  syncer::SyncerError result =
      ProcessGetUpdatesResponse(request_types, gu_response, status);
  if (result != syncer::SYNCER_OK)
    return result;

  if (gu_response.changes_remaining() == 0) {
    return SYNCER_OK;
  } else {
    return SERVER_MORE_TO_DOWNLOAD;
  }
}

syncer::SyncerError GetUpdatesProcessor::ProcessGetUpdatesResponse(
    ModelTypeSet gu_types,
    const sync_pb::GetUpdatesResponse& gu_response,
    sessions::StatusController* status_controller) {
  TypeSyncEntityMap updates_by_type;
  PartitionUpdatesByType(gu_response, gu_types, &updates_by_type);
  DCHECK_EQ(gu_types.Size(), updates_by_type.size());

  TypeToIndexMap progress_index_by_type;
  PartitionProgressMarkersByType(gu_response,
                                 gu_types,
                                 &progress_index_by_type);
  if (gu_types.Size() != progress_index_by_type.size()) {
    NOTREACHED() << "Missing progress markers in GetUpdates response.";
    return syncer::SERVER_RESPONSE_VALIDATION_FAILED;
  }

  TypeToIndexMap context_by_type;
  PartitionContextMutationsByType(gu_response, gu_types, &context_by_type);

  // Iterate over these maps in parallel, processing updates for each type.
  TypeToIndexMap::iterator progress_marker_iter =
      progress_index_by_type.begin();
  TypeSyncEntityMap::iterator updates_iter = updates_by_type.begin();
  for (; (progress_marker_iter != progress_index_by_type.end()
           && updates_iter != updates_by_type.end());
       ++progress_marker_iter, ++updates_iter) {
    DCHECK_EQ(progress_marker_iter->first, updates_iter->first);
    ModelType type = progress_marker_iter->first;

    UpdateHandlerMap::iterator update_handler_iter =
        update_handler_map_->find(type);

    sync_pb::DataTypeContext context;
    TypeToIndexMap::iterator context_iter = context_by_type.find(type);
    if (context_iter != context_by_type.end())
      context.CopyFrom(gu_response.context_mutations(context_iter->second));

    if (update_handler_iter != update_handler_map_->end()) {
      syncer::SyncerError result =
          update_handler_iter->second->ProcessGetUpdatesResponse(
              gu_response.new_progress_marker(progress_marker_iter->second),
              context,
              updates_iter->second,
              status_controller);
      if (result != syncer::SYNCER_OK)
        return result;
    } else {
      DLOG(WARNING)
          << "Ignoring received updates of a type we can't handle.  "
          << "Type is: " << ModelTypeToString(type);
      continue;
    }
  }
  DCHECK(progress_marker_iter == progress_index_by_type.end() &&
         updates_iter == updates_by_type.end());

  return syncer::SYNCER_OK;
}

void GetUpdatesProcessor::ApplyUpdates(
    ModelTypeSet gu_types,
    sessions::StatusController* status_controller) {
  delegate_.ApplyUpdates(gu_types, status_controller, update_handler_map_);
}

void GetUpdatesProcessor::CopyClientDebugInfo(
    sessions::DebugInfoGetter* debug_info_getter,
    sync_pb::DebugInfo* debug_info) {
  DVLOG(1) << "Copying client debug info to send.";
  debug_info_getter->GetDebugInfo(debug_info);
}

}  // namespace syncer
