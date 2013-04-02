// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNCER_PROTO_UTIL_H_
#define SYNC_ENGINE_SYNCER_PROTO_UTIL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/blob.h"

namespace sync_pb {
class ClientToServerMessage;
class ClientToServerResponse;
class ClientToServerResponse_Error;
class CommitResponse_EntryResponse;
class EntitySpecifics;
class SyncEntity;
}

namespace syncer {

class ThrottledDataTypeTracker;
class ServerConnectionManager;

namespace sessions {
class SyncProtocolError;
class SyncSessionContext;
}

namespace syncable {
class Directory;
class Entry;
}

// Returns the types to migrate from the data in |response|.
SYNC_EXPORT_PRIVATE ModelTypeSet GetTypesToMigrate(
    const sync_pb::ClientToServerResponse& response);

// Builds a SyncProtocolError from the data in |error|.
SYNC_EXPORT_PRIVATE SyncProtocolError ConvertErrorPBToLocalType(
    const sync_pb::ClientToServerResponse_Error& error);

class SYNC_EXPORT_PRIVATE SyncerProtoUtil {
 public:
  // Posts the given message and fills the buffer with the returned value.
  // Returns true on success.  Also handles store birthday verification: will
  // produce a SyncError if the birthday is incorrect.
  // NOTE: This will add all fields that must be sent on every request, which
  // includes store birthday, protocol version, client chips, api keys, etc.
  static SyncerError PostClientToServerMessage(
      sync_pb::ClientToServerMessage* msg,
      sync_pb::ClientToServerResponse* response,
      sessions::SyncSession* session);

  // Compares a syncable Entry to SyncEntity, returns true iff the data is
  // identical.
  //
  // TODO(sync): The places where this function is used are arguable big causes
  // of the fragility, because there's a tendency to freak out the moment the
  // local and server values diverge. However, this almost always indicates a
  // sync bug somewhere earlier in the sync cycle.
  static bool Compare(const syncable::Entry& local_entry,
                      const sync_pb::SyncEntity& server_entry);

  static bool ShouldMaintainPosition(const sync_pb::SyncEntity& sync_entity);

  // Utility methods for converting between syncable::Blobs and protobuf byte
  // fields.
  static void CopyProtoBytesIntoBlob(const std::string& proto_bytes,
                                     syncable::Blob* blob);
  static bool ProtoBytesEqualsBlob(const std::string& proto_bytes,
                                   const syncable::Blob& blob);
  static void CopyBlobIntoProtoBytes(const syncable::Blob& blob,
                                     std::string* proto_bytes);

  // Extract the name field from a sync entity.
  static const std::string& NameFromSyncEntity(
      const sync_pb::SyncEntity& entry);

  // Extract the name field from a commit entry response.
  static const std::string& NameFromCommitEntryResponse(
      const sync_pb::CommitResponse_EntryResponse& entry);

  // Persist the bag of chips if it is present in the response.
  static void PersistBagOfChips(
      syncable::Directory* dir,
      const sync_pb::ClientToServerResponse& response);

  // EntitySpecifics is used as a filter for the GetUpdates message to tell
  // the server which datatypes to send back.  This adds a datatype so that
  // it's included in the filter.
  static void AddToEntitySpecificDatatypesFilter(ModelType datatype,
      sync_pb::EntitySpecifics* filter);

  // Get a debug string representation of the client to server response.
  static std::string ClientToServerResponseDebugString(
      const sync_pb::ClientToServerResponse& response);

  // Get update contents as a string. Intended for logging, and intended
  // to have a smaller footprint than the protobuf's built-in pretty printer.
  static std::string SyncEntityDebugString(const sync_pb::SyncEntity& entry);

  // Pull the birthday from the dir and put it into the msg.
  static void AddRequestBirthday(syncable::Directory* dir,
                                 sync_pb::ClientToServerMessage* msg);

  // Pull the bag of chips from the dir and put it into the msg.
  static void AddBagOfChips(syncable::Directory* dir,
                            sync_pb::ClientToServerMessage* msg);


  // Set the protocol version field in the outgoing message.
  static void SetProtocolVersion(sync_pb::ClientToServerMessage* msg);

 private:
  SyncerProtoUtil() {}

  // Helper functions for PostClientToServerMessage.

  // Verifies the store birthday, alerting/resetting as appropriate if there's a
  // mismatch. Return false if the syncer should be stuck.
  static bool VerifyResponseBirthday(
      const sync_pb::ClientToServerResponse& response,
      syncable::Directory* dir);

  // Post the message using the scm, and do some processing on the returned
  // headers. Decode the server response.
  static bool PostAndProcessHeaders(ServerConnectionManager* scm,
                                    sessions::SyncSession* session,
                                    const sync_pb::ClientToServerMessage& msg,
                                    sync_pb::ClientToServerResponse* response);

  static base::TimeDelta GetThrottleDelay(
      const sync_pb::ClientToServerResponse& response);

  static void HandleThrottleError(
      const SyncProtocolError& error,
      const base::TimeTicks& throttled_until,
      ThrottledDataTypeTracker* tracker,
      sessions::SyncSession::Delegate* delegate);

  friend class SyncerProtoUtilTest;
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, AddRequestBirthday);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, PostAndProcessHeaders);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, VerifyResponseBirthday);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, HandleThrottlingNoDatatypes);
  FRIEND_TEST_ALL_PREFIXES(SyncerProtoUtilTest, HandleThrottlingWithDatatypes);

  DISALLOW_COPY_AND_ASSIGN(SyncerProtoUtil);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNCER_PROTO_UTIL_H_
