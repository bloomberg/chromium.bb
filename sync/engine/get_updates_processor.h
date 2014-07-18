// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_GET_UPDATES_PROCESSOR_H
#define SYNC_ENGINE_GET_UPDATES_PROCESSOR_H

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/model_type_registry.h"

namespace sync_pb {
class GetUpdatesMessage;
class GetUpdatesResponse;
}  // namespace sync_pb

namespace syncer {

namespace sessions {
class StatusController;
class SyncSession;
class SyncSessionContext;
class DebugInfoGetter;
}  // namespace sessions

namespace syncable {
class Directory;
}  // namespace syncable

class GetUpdatesDelegate;

// This class manages the set of per-type syncer objects.
//
// It owns these types and hides the details of iterating over all of them.
// Most methods allow the caller to specify a subset of types on which the
// operation is to be applied.  It is a logic error if the supplied set of types
// contains a type which was not previously registered with the manager.
class SYNC_EXPORT_PRIVATE GetUpdatesProcessor {
 public:
  explicit GetUpdatesProcessor(UpdateHandlerMap* update_handler_map,
                               const GetUpdatesDelegate& delegate);
  ~GetUpdatesProcessor();

  // Downloads and processes a batch of updates for the specified types.
  //
  // Returns SYNCER_OK if the download succeeds, SERVER_MORE_TO_DOWNLOAD if the
  // download succeeded but there are still some updates left to fetch on the
  // server, or an appropriate error value in case of failure.
  SyncerError DownloadUpdates(
      ModelTypeSet request_types,
      sessions::SyncSession* session,
      bool create_mobile_bookmarks_folder);

  // Applies any downloaded and processed updates.
  void ApplyUpdates(
      ModelTypeSet gu_types,
      sessions::StatusController* status_controller);

 private:
  // Populates a GetUpdates request message with per-type information.
  void PrepareGetUpdates(
      ModelTypeSet gu_types,
      sync_pb::ClientToServerMessage* message);

  // Sends the specified message to the server and stores the response in a
  // member of the |session|'s StatusController.
  SyncerError ExecuteDownloadUpdates(ModelTypeSet request_types,
                                     sessions::SyncSession* session,
                                     sync_pb::ClientToServerMessage* msg);

  // Helper function for processing responses from the server.  Defined here for
  // testing.
  SyncerError ProcessResponse(const sync_pb::GetUpdatesResponse& gu_response,
                              ModelTypeSet proto_request_types,
                              sessions::StatusController* status);

  // Processes a GetUpdates responses for each type.
  syncer::SyncerError ProcessGetUpdatesResponse(
      ModelTypeSet gu_types,
      const sync_pb::GetUpdatesResponse& gu_response,
      sessions::StatusController* status_controller);

  static void CopyClientDebugInfo(
      sessions::DebugInfoGetter* debug_info_getter,
      sync_pb::DebugInfo* debug_info);

  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, BookmarkNudge);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NotifyMany);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, InitialSyncRequest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, ConfigureTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, PollTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, RetryTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NudgeWithRetryTest);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, InvalidResponse);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, MoreToDownloadResponse);
  FRIEND_TEST_ALL_PREFIXES(GetUpdatesProcessorTest, NormalResponseTest);
  FRIEND_TEST_ALL_PREFIXES(DownloadUpdatesDebugInfoTest,
                           VerifyCopyClientDebugInfo_Empty);
  FRIEND_TEST_ALL_PREFIXES(DownloadUpdatesDebugInfoTest, VerifyCopyOverwrites);

  // A map of 'update handlers', one for each enabled type.
  // This must be kept in sync with the routing info.  Our temporary solution to
  // that problem is to initialize this map in set_routing_info().
  UpdateHandlerMap* update_handler_map_;

  const GetUpdatesDelegate& delegate_;

  DISALLOW_COPY_AND_ASSIGN(GetUpdatesProcessor);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_GET_UPDATES_PROCESSOR_H_
