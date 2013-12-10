// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DOWNLOAD_H_
#define SYNC_ENGINE_DOWNLOAD_H_

#include "sync/base/sync_export.h"
#include "sync/engine/sync_directory_update_handler.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"

namespace sync_pb {
class DebugInfo;
}  // namespace sync_pb

namespace syncer {

namespace sessions {
class DebugInfoGetter;
class NudgeTracker;
class StatusController;
class SyncSession;
}  // namespace sessions

namespace download {

// This function executes a single GetUpdate request and stores the response in
// the session's StatusController.  It constructs the type of request used to
// keep types in sync when in normal mode.
SYNC_EXPORT_PRIVATE void BuildNormalDownloadUpdates(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    const sessions::NudgeTracker& nudge_tracker,
    sync_pb::ClientToServerMessage* client_to_server_message);

// Helper function.  Defined here for testing.
SYNC_EXPORT_PRIVATE void BuildNormalDownloadUpdatesImpl(
    ModelTypeSet proto_request_types,
    UpdateHandlerMap* update_handler_map,
    const sessions::NudgeTracker& nudge_tracker,
    sync_pb::GetUpdatesMessage* get_updates);

// This function executes a single GetUpdate request and stores the response in
// the session's StatusController.  It constructs the type of request used to
// initialize a type for the first time.
SYNC_EXPORT_PRIVATE void BuildDownloadUpdatesForConfigure(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    ModelTypeSet request_types,
    sync_pb::ClientToServerMessage* client_to_server_message);

// Helper function.  Defined here for testing.
SYNC_EXPORT_PRIVATE void BuildDownloadUpdatesForConfigureImpl(
    ModelTypeSet proto_request_types,
    UpdateHandlerMap* update_handler_map,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sync_pb::GetUpdatesMessage* get_updates);

// This function executes a single GetUpdate request and stores the response in
// the session's status controller.  It constructs the type of request used for
// periodic polling.
SYNC_EXPORT_PRIVATE void BuildDownloadUpdatesForPoll(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    sync_pb::ClientToServerMessage* client_to_server_message);

// Helper function.  Defined here for testing.
SYNC_EXPORT_PRIVATE void BuildDownloadUpdatesForPollImpl(
    ModelTypeSet proto_request_types,
    UpdateHandlerMap* update_handler_map,
    sync_pb::GetUpdatesMessage* get_updates);

// Sends the specified message to the server and stores the response in a member
// of the |session|'s StatusController.
SYNC_EXPORT_PRIVATE SyncerError
    ExecuteDownloadUpdates(ModelTypeSet request_types,
                           sessions::SyncSession* session,
                           sync_pb::ClientToServerMessage* msg);

// Helper function for processing responses from the server.
// Defined here for testing.
SYNC_EXPORT_PRIVATE SyncerError ProcessResponse(
    const sync_pb::GetUpdatesResponse& gu_response,
    ModelTypeSet proto_request_types,
    UpdateHandlerMap* handler_map,
    sessions::StatusController* status);

// Helper function to copy client debug info from debug_info_getter to
// debug_info.  Defined here for testing.
SYNC_EXPORT_PRIVATE void CopyClientDebugInfo(
    sessions::DebugInfoGetter* debug_info_getter,
    sync_pb::DebugInfo* debug_info);

}  // namespace download

}  // namespace syncer

#endif  // SYNC_ENGINE_DOWNLOAD_H_
