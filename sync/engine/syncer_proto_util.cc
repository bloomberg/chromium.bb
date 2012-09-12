// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer_proto_util.h"

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/engine/traffic_logger.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/sync_enums.pb.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/syncable-inl.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/util/time.h"

using std::string;
using std::stringstream;
using sync_pb::ClientToServerMessage;
using sync_pb::ClientToServerResponse;

namespace syncer {

using sessions::SyncSession;
using syncable::BASE_VERSION;
using syncable::CTIME;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNSYNCED;
using syncable::MTIME;
using syncable::PARENT_ID;

namespace {

// Time to backoff syncing after receiving a throttled response.
const int kSyncDelayAfterThrottled = 2 * 60 * 60;  // 2 hours

void LogResponseProfilingData(const ClientToServerResponse& response) {
  if (response.has_profiling_data()) {
    stringstream response_trace;
    response_trace << "Server response trace:";

    if (response.profiling_data().has_user_lookup_time()) {
      response_trace << " user lookup: "
                     << response.profiling_data().user_lookup_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_write_time()) {
      response_trace << " meta write: "
                     << response.profiling_data().meta_data_write_time()
                     << "ms";
    }

    if (response.profiling_data().has_meta_data_read_time()) {
      response_trace << " meta read: "
                     << response.profiling_data().meta_data_read_time() << "ms";
    }

    if (response.profiling_data().has_file_data_write_time()) {
      response_trace << " file write: "
                     << response.profiling_data().file_data_write_time()
                     << "ms";
    }

    if (response.profiling_data().has_file_data_read_time()) {
      response_trace << " file read: "
                     << response.profiling_data().file_data_read_time() << "ms";
    }

    if (response.profiling_data().has_total_request_time()) {
      response_trace << " total time: "
                     << response.profiling_data().total_request_time() << "ms";
    }
    DVLOG(1) << response_trace.str();
  }
}

SyncerError ServerConnectionErrorAsSyncerError(
    const HttpResponse::ServerConnectionCode server_status) {
  switch (server_status) {
    case HttpResponse::CONNECTION_UNAVAILABLE:
      return NETWORK_CONNECTION_UNAVAILABLE;
    case HttpResponse::IO_ERROR:
      return NETWORK_IO_ERROR;
    case HttpResponse::SYNC_SERVER_ERROR:
      // FIXME what does this mean?
      return SYNC_SERVER_ERROR;
    case HttpResponse::SYNC_AUTH_ERROR:
      return SYNC_AUTH_ERROR;
    case HttpResponse::RETRY:
      return SERVER_RETURN_TRANSIENT_ERROR;
    case HttpResponse::SERVER_CONNECTION_OK:
    case HttpResponse::NONE:
    default:
      NOTREACHED();
      return UNSET;
  }
}

}  // namespace

// static
void SyncerProtoUtil::HandleMigrationDoneResponse(
  const ClientToServerResponse* response,
  sessions::SyncSession* session) {
  LOG_IF(ERROR, 0 >= response->migrated_data_type_id_size())
      << "MIGRATION_DONE but no types specified.";
  ModelTypeSet to_migrate;
  for (int i = 0; i < response->migrated_data_type_id_size(); i++) {
    to_migrate.Put(GetModelTypeFromSpecificsFieldNumber(
        response->migrated_data_type_id(i)));
  }
  // TODO(akalin): This should be a set union.
  session->mutable_status_controller()->
      set_types_needing_local_migration(to_migrate);
}

// static
bool SyncerProtoUtil::VerifyResponseBirthday(syncable::Directory* dir,
    const ClientToServerResponse* response) {

  std::string local_birthday = dir->store_birthday();

  if (local_birthday.empty()) {
    if (!response->has_store_birthday()) {
      LOG(WARNING) << "Expected a birthday on first sync.";
      return false;
    }

    DVLOG(1) << "New store birthday: " << response->store_birthday();
    dir->set_store_birthday(response->store_birthday());
    return true;
  }

  // Error situation, but we're not stuck.
  if (!response->has_store_birthday()) {
    LOG(WARNING) << "No birthday in server response?";
    return true;
  }

  if (response->store_birthday() != local_birthday) {
    LOG(WARNING) << "Birthday changed, showing syncer stuck";
    return false;
  }

  return true;
}

// static
void SyncerProtoUtil::AddRequestBirthday(syncable::Directory* dir,
                                         ClientToServerMessage* msg) {
  if (!dir->store_birthday().empty())
    msg->set_store_birthday(dir->store_birthday());
}

// static
void SyncerProtoUtil::AddBagOfChips(syncable::Directory* dir,
                                    ClientToServerMessage* msg) {
  msg->mutable_bag_of_chips()->ParseFromString(dir->bag_of_chips());
}

// static
void SyncerProtoUtil::SetProtocolVersion(ClientToServerMessage* msg) {
  const int current_version =
      ClientToServerMessage::default_instance().protocol_version();
  msg->set_protocol_version(current_version);
}

// static
bool SyncerProtoUtil::PostAndProcessHeaders(ServerConnectionManager* scm,
                                            sessions::SyncSession* session,
                                            const ClientToServerMessage& msg,
                                            ClientToServerResponse* response) {
  ServerConnectionManager::PostBufferParams params;
  DCHECK(msg.has_protocol_version());
  DCHECK_EQ(msg.protocol_version(),
            ClientToServerMessage::default_instance().protocol_version());
  msg.SerializeToString(&params.buffer_in);

  ScopedServerStatusWatcher server_status_watcher(scm, &params.response);
  // Fills in params.buffer_out and params.response.
  if (!scm->PostBufferWithCachedAuth(&params, &server_status_watcher)) {
    LOG(WARNING) << "Error posting from syncer:" << params.response;
    return false;
  }

  std::string new_token = params.response.update_client_auth_header;
  if (!new_token.empty()) {
    SyncEngineEvent event(SyncEngineEvent::UPDATED_TOKEN);
    event.updated_token = new_token;
    session->context()->NotifyListeners(event);
  }

  if (response->ParseFromString(params.buffer_out)) {
    // TODO(tim): This is an egregious layering violation (bug 35060).
    switch (response->error_code()) {
      case sync_pb::SyncEnums::ACCESS_DENIED:
      case sync_pb::SyncEnums::AUTH_INVALID:
      case sync_pb::SyncEnums::USER_NOT_ACTIVATED:
        // Fires on ScopedServerStatusWatcher
        params.response.server_status = HttpResponse::SYNC_AUTH_ERROR;
        return false;
      default:
        return true;
    }
  }

  return false;
}

base::TimeDelta SyncerProtoUtil::GetThrottleDelay(
    const ClientToServerResponse& response) {
  base::TimeDelta throttle_delay =
      base::TimeDelta::FromSeconds(kSyncDelayAfterThrottled);
  if (response.has_client_command()) {
    const sync_pb::ClientCommand& command = response.client_command();
    if (command.has_throttle_delay_seconds()) {
      throttle_delay =
          base::TimeDelta::FromSeconds(command.throttle_delay_seconds());
    }
  }
  return throttle_delay;
}

void SyncerProtoUtil::HandleThrottleError(
    const SyncProtocolError& error,
    const base::TimeTicks& throttled_until,
    ThrottledDataTypeTracker* tracker,
    sessions::SyncSession::Delegate* delegate) {
  DCHECK_EQ(error.error_type, THROTTLED);
  if (error.error_data_types.Empty()) {
    // No datatypes indicates the client should be completely throttled.
    delegate->OnSilencedUntil(throttled_until);
  } else {
    tracker->SetUnthrottleTime(error.error_data_types, throttled_until);
  }
}

namespace {

// Helper function for an assertion in PostClientToServerMessage.
bool IsVeryFirstGetUpdates(const ClientToServerMessage& message) {
  if (!message.has_get_updates())
    return false;
  DCHECK_LT(0, message.get_updates().from_progress_marker_size());
  for (int i = 0; i < message.get_updates().from_progress_marker_size(); ++i) {
    if (!message.get_updates().from_progress_marker(i).token().empty())
      return false;
  }
  return true;
}

SyncProtocolErrorType ConvertSyncProtocolErrorTypePBToLocalType(
    const sync_pb::SyncEnums::ErrorType& error_type) {
  switch (error_type) {
    case sync_pb::SyncEnums::SUCCESS:
      return SYNC_SUCCESS;
    case sync_pb::SyncEnums::NOT_MY_BIRTHDAY:
      return NOT_MY_BIRTHDAY;
    case sync_pb::SyncEnums::THROTTLED:
      return THROTTLED;
    case sync_pb::SyncEnums::CLEAR_PENDING:
      return CLEAR_PENDING;
    case sync_pb::SyncEnums::TRANSIENT_ERROR:
      return TRANSIENT_ERROR;
    case sync_pb::SyncEnums::MIGRATION_DONE:
      return MIGRATION_DONE;
    case sync_pb::SyncEnums::UNKNOWN:
      return UNKNOWN_ERROR;
    case sync_pb::SyncEnums::USER_NOT_ACTIVATED:
    case sync_pb::SyncEnums::AUTH_INVALID:
    case sync_pb::SyncEnums::ACCESS_DENIED:
      return INVALID_CREDENTIAL;
    default:
      NOTREACHED();
      return UNKNOWN_ERROR;
  }
}

ClientAction ConvertClientActionPBToLocalClientAction(
    const sync_pb::SyncEnums::Action& action) {
  switch (action) {
    case sync_pb::SyncEnums::UPGRADE_CLIENT:
      return UPGRADE_CLIENT;
    case sync_pb::SyncEnums::CLEAR_USER_DATA_AND_RESYNC:
      return CLEAR_USER_DATA_AND_RESYNC;
    case sync_pb::SyncEnums::ENABLE_SYNC_ON_ACCOUNT:
      return ENABLE_SYNC_ON_ACCOUNT;
    case sync_pb::SyncEnums::STOP_AND_RESTART_SYNC:
      return STOP_AND_RESTART_SYNC;
    case sync_pb::SyncEnums::DISABLE_SYNC_ON_CLIENT:
      return DISABLE_SYNC_ON_CLIENT;
    case sync_pb::SyncEnums::UNKNOWN_ACTION:
      return UNKNOWN_ACTION;
    default:
      NOTREACHED();
      return UNKNOWN_ACTION;
  }
}

SyncProtocolError ConvertErrorPBToLocalType(
    const ClientToServerResponse::Error& error) {
  SyncProtocolError sync_protocol_error;
  sync_protocol_error.error_type = ConvertSyncProtocolErrorTypePBToLocalType(
      error.error_type());
  sync_protocol_error.error_description = error.error_description();
  sync_protocol_error.url = error.url();
  sync_protocol_error.action = ConvertClientActionPBToLocalClientAction(
      error.action());

  if (error.error_data_type_ids_size() > 0) {
    // THROTTLED is currently the only error code that uses |error_data_types|.
    DCHECK_EQ(error.error_type(), sync_pb::SyncEnums::THROTTLED);
    for (int i = 0; i < error.error_data_type_ids_size(); ++i) {
      sync_protocol_error.error_data_types.Put(
          GetModelTypeFromSpecificsFieldNumber(
              error.error_data_type_ids(i)));
    }
  }

  return sync_protocol_error;
}

// TODO(lipalani) : Rename these function names as per the CR for issue 7740067.
SyncProtocolError ConvertLegacyErrorCodeToNewError(
    const sync_pb::SyncEnums::ErrorType& error_type) {
  SyncProtocolError error;
  error.error_type = ConvertSyncProtocolErrorTypePBToLocalType(error_type);
  if (error_type == sync_pb::SyncEnums::CLEAR_PENDING ||
      error_type == sync_pb::SyncEnums::NOT_MY_BIRTHDAY) {
      error.action = DISABLE_SYNC_ON_CLIENT;
  }  // There is no other action we can compute for legacy server.
  return error;
}

}  // namespace

// static
SyncerError SyncerProtoUtil::PostClientToServerMessage(
    const ClientToServerMessage& msg,
    ClientToServerResponse* response,
    SyncSession* session) {

  CHECK(response);
  DCHECK(!msg.get_updates().has_from_timestamp());  // Deprecated.
  DCHECK(!msg.get_updates().has_requested_types());  // Deprecated.
  DCHECK(msg.has_store_birthday() || IsVeryFirstGetUpdates(msg))
      << "Must call AddRequestBirthday to set birthday.";
  DCHECK(msg.has_bag_of_chips())
      << "Must call AddBagOfChips to set bag_of_chips.";

  syncable::Directory* dir = session->context()->directory();

  LogClientToServerMessage(msg);
  session->context()->traffic_recorder()->RecordClientToServerMessage(msg);
  if (!PostAndProcessHeaders(session->context()->connection_manager(), session,
                             msg, response)) {
    // There was an error establishing communication with the server.
    // We can not proceed beyond this point.
    const HttpResponse::ServerConnectionCode server_status =
        session->context()->connection_manager()->server_status();

    DCHECK_NE(server_status, HttpResponse::NONE);
    DCHECK_NE(server_status, HttpResponse::SERVER_CONNECTION_OK);

    return ServerConnectionErrorAsSyncerError(server_status);
  }

  LogClientToServerResponse(*response);
  session->context()->traffic_recorder()->RecordClientToServerResponse(
      *response);

  // Persist a bag of chips if it has been sent by the server.
  PersistBagOfChips(dir, *response);

  SyncProtocolError sync_protocol_error;

  // Birthday mismatch overrides any error that is sent by the server.
  if (!VerifyResponseBirthday(dir, response)) {
    sync_protocol_error.error_type = NOT_MY_BIRTHDAY;
     sync_protocol_error.action =
         DISABLE_SYNC_ON_CLIENT;
  } else if (response->has_error()) {
    // This is a new server. Just get the error from the protocol.
    sync_protocol_error = ConvertErrorPBToLocalType(response->error());
  } else {
    // Legacy server implementation. Compute the error based on |error_code|.
    sync_protocol_error = ConvertLegacyErrorCodeToNewError(
        response->error_code());
  }

  // Now set the error into the status so the layers above us could read it.
  sessions::StatusController* status = session->mutable_status_controller();
  status->set_sync_protocol_error(sync_protocol_error);

  // Inform the delegate of the error we got.
  session->delegate()->OnSyncProtocolError(session->TakeSnapshot());

  // Update our state for any other commands we've received.
  if (response->has_client_command()) {
    const sync_pb::ClientCommand& command = response->client_command();
    if (command.has_max_commit_batch_size()) {
      session->context()->set_max_commit_batch_size(
          command.max_commit_batch_size());
    }

    if (command.has_set_sync_long_poll_interval()) {
      session->delegate()->OnReceivedLongPollIntervalUpdate(
          base::TimeDelta::FromSeconds(command.set_sync_long_poll_interval()));
    }

    if (command.has_set_sync_poll_interval()) {
      session->delegate()->OnReceivedShortPollIntervalUpdate(
          base::TimeDelta::FromSeconds(command.set_sync_poll_interval()));
    }

    if (command.has_sessions_commit_delay_seconds()) {
      session->delegate()->OnReceivedSessionsCommitDelay(
          base::TimeDelta::FromSeconds(
              command.sessions_commit_delay_seconds()));
    }
  }

  // Now do any special handling for the error type and decide on the return
  // value.
  switch (sync_protocol_error.error_type) {
    case UNKNOWN_ERROR:
      LOG(WARNING) << "Sync protocol out-of-date. The server is using a more "
                   << "recent version.";
      return SERVER_RETURN_UNKNOWN_ERROR;
    case SYNC_SUCCESS:
      LogResponseProfilingData(*response);
      return SYNCER_OK;
    case THROTTLED:
      LOG(WARNING) << "Client silenced by server.";
      HandleThrottleError(sync_protocol_error,
                          base::TimeTicks::Now() + GetThrottleDelay(*response),
                          session->context()->throttled_data_type_tracker(),
                          session->delegate());
      return SERVER_RETURN_THROTTLED;
    case TRANSIENT_ERROR:
      return SERVER_RETURN_TRANSIENT_ERROR;
    case MIGRATION_DONE:
      HandleMigrationDoneResponse(response, session);
      return SERVER_RETURN_MIGRATION_DONE;
    case CLEAR_PENDING:
      return SERVER_RETURN_CLEAR_PENDING;
    case NOT_MY_BIRTHDAY:
      return SERVER_RETURN_NOT_MY_BIRTHDAY;
    default:
      NOTREACHED();
      return UNSET;
  }
}

// static
bool SyncerProtoUtil::Compare(const syncable::Entry& local_entry,
                              const sync_pb::SyncEntity& server_entry) {
  const std::string name = NameFromSyncEntity(server_entry);

  CHECK_EQ(local_entry.Get(ID), SyncableIdFromProto(server_entry.id_string()));
  CHECK_EQ(server_entry.version(), local_entry.Get(BASE_VERSION));
  CHECK(!local_entry.Get(IS_UNSYNCED));

  if (local_entry.Get(IS_DEL) && server_entry.deleted())
    return true;
  if (local_entry.Get(CTIME) != ProtoTimeToTime(server_entry.ctime())) {
    LOG(WARNING) << "ctime mismatch";
    return false;
  }

  // These checks are somewhat prolix, but they're easier to debug than a big
  // boolean statement.
  string client_name = local_entry.Get(syncable::NON_UNIQUE_NAME);
  if (client_name != name) {
    LOG(WARNING) << "Client name mismatch";
    return false;
  }
  if (local_entry.Get(PARENT_ID) !=
      SyncableIdFromProto(server_entry.parent_id_string())) {
    LOG(WARNING) << "Parent ID mismatch";
    return false;
  }
  if (local_entry.Get(IS_DIR) != IsFolder(server_entry)) {
    LOG(WARNING) << "Dir field mismatch";
    return false;
  }
  if (local_entry.Get(IS_DEL) != server_entry.deleted()) {
    LOG(WARNING) << "Deletion mismatch";
    return false;
  }
  if (!local_entry.Get(IS_DIR) &&
      (local_entry.Get(MTIME) != ProtoTimeToTime(server_entry.mtime()))) {
    LOG(WARNING) << "mtime mismatch";
    return false;
  }

  return true;
}

// static
void SyncerProtoUtil::CopyProtoBytesIntoBlob(const std::string& proto_bytes,
                                             syncable::Blob* blob) {
  syncable::Blob proto_blob(proto_bytes.begin(), proto_bytes.end());
  blob->swap(proto_blob);
}

// static
bool SyncerProtoUtil::ProtoBytesEqualsBlob(const std::string& proto_bytes,
                                           const syncable::Blob& blob) {
  if (proto_bytes.size() != blob.size())
    return false;
  return std::equal(proto_bytes.begin(), proto_bytes.end(), blob.begin());
}

// static
void SyncerProtoUtil::CopyBlobIntoProtoBytes(const syncable::Blob& blob,
                                             std::string* proto_bytes) {
  std::string blob_string(blob.begin(), blob.end());
  proto_bytes->swap(blob_string);
}

// static
const std::string& SyncerProtoUtil::NameFromSyncEntity(
    const sync_pb::SyncEntity& entry) {
  if (entry.has_non_unique_name())
    return entry.non_unique_name();
  return entry.name();
}

// static
const std::string& SyncerProtoUtil::NameFromCommitEntryResponse(
    const sync_pb::CommitResponse_EntryResponse& entry) {
  if (entry.has_non_unique_name())
    return entry.non_unique_name();
  return entry.name();
}

// static
void SyncerProtoUtil::PersistBagOfChips(syncable::Directory* dir,
    const sync_pb::ClientToServerResponse& response) {
  if (!response.has_new_bag_of_chips())
    return;
  std::string bag_of_chips;
  if (response.new_bag_of_chips().SerializeToString(&bag_of_chips))
    dir->set_bag_of_chips(bag_of_chips);
}

std::string SyncerProtoUtil::SyncEntityDebugString(
    const sync_pb::SyncEntity& entry) {
  const std::string& mtime_str =
      GetTimeDebugString(ProtoTimeToTime(entry.mtime()));
  const std::string& ctime_str =
      GetTimeDebugString(ProtoTimeToTime(entry.ctime()));
  return base::StringPrintf(
      "id: %s, parent_id: %s, "
      "version: %"PRId64"d, "
      "mtime: %" PRId64"d (%s), "
      "ctime: %" PRId64"d (%s), "
      "name: %s, sync_timestamp: %" PRId64"d, "
      "%s ",
      entry.id_string().c_str(),
      entry.parent_id_string().c_str(),
      entry.version(),
      entry.mtime(), mtime_str.c_str(),
      entry.ctime(), ctime_str.c_str(),
      entry.name().c_str(), entry.sync_timestamp(),
      entry.deleted() ? "deleted, ":"");
}

namespace {
std::string GetUpdatesResponseString(
    const sync_pb::GetUpdatesResponse& response) {
  std::string output;
  output.append("GetUpdatesResponse:\n");
  for (int i = 0; i < response.entries_size(); i++) {
    output.append(SyncerProtoUtil::SyncEntityDebugString(response.entries(i)));
    output.append("\n");
  }
  return output;
}
}  // namespace

std::string SyncerProtoUtil::ClientToServerResponseDebugString(
    const ClientToServerResponse& response) {
  // Add more handlers as needed.
  std::string output;
  if (response.has_get_updates())
    output.append(GetUpdatesResponseString(response.get_updates()));
  return output;
}

}  // namespace syncer
