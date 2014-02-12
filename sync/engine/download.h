// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DOWNLOAD_H_
#define SYNC_ENGINE_DOWNLOAD_H_

#include "sync/base/sync_export.h"
#include "sync/engine/get_updates_processor.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"

namespace sync_pb {
class DebugInfo;
}  // namespace sync_pb

namespace syncer {

namespace sessions {
class DebugInfoGetter;
class StatusController;
class SyncSession;
}  // namespace sessions

namespace download {

// Generic initialization of a GetUpdates message.
SYNC_EXPORT_PRIVATE void InitDownloadUpdatesContext(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    sync_pb::ClientToServerMessage* message);

// Sends the specified message to the server and stores the response in a member
// of the |session|'s StatusController.
SYNC_EXPORT_PRIVATE SyncerError
    ExecuteDownloadUpdates(ModelTypeSet request_types,
                           sessions::SyncSession* session,
                           GetUpdatesProcessor* get_updates_processor,
                           sync_pb::ClientToServerMessage* msg);

// Helper function for processing responses from the server.
// Defined here for testing.
SYNC_EXPORT_PRIVATE SyncerError ProcessResponse(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet proto_request_types,
    GetUpdatesProcessor* get_updates_processor,
    sessions::StatusController* status);

// Helper function to copy client debug info from debug_info_getter to
// debug_info.  Defined here for testing.
SYNC_EXPORT_PRIVATE void CopyClientDebugInfo(
    sessions::DebugInfoGetter* debug_info_getter,
    sync_pb::DebugInfo* debug_info);

}  // namespace download

}  // namespace syncer

#endif  // SYNC_ENGINE_DOWNLOAD_H_
