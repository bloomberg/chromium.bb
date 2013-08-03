// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DOWNLOAD_H_
#define SYNC_ENGINE_DOWNLOAD_H_

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/syncer_error.h"
#include "sync/protocol/sync.pb.h"

namespace sync_pb {
class DebugInfo;
}  // namespace sync_pb

namespace syncer {

namespace sessions {
class NudgeTracker;
class SyncSession;
}  // namespace sessions

class Syncer;

// This function executes a single GetUpdate request and stores the response in
// the session's StatusController.  It constructs the type of request used to
// keep types in sync when in normal mode.
SYNC_EXPORT_PRIVATE SyncerError NormalDownloadUpdates(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types,
    const sessions::NudgeTracker& nudge_tracker);

// This function executes a single GetUpdate request and stores the response in
// the session's StatusController.  It constructs the type of request used to
// initialize a type for the first time.
SYNC_EXPORT_PRIVATE SyncerError DownloadUpdatesForConfigure(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    ModelTypeSet request_types);

// This function executes a single GetUpdate request and stores the response in
// the session's status controller.  It constructs the type of request used for
// periodic polling.
SYNC_EXPORT_PRIVATE SyncerError DownloadUpdatesForPoll(
    sessions::SyncSession* session,
    bool create_mobile_bookmarks_folder,
    ModelTypeSet request_types);

// A helper to set debug info.  Exposed only for testing.
SYNC_EXPORT_PRIVATE void AppendClientDebugInfoIfNeeded(
    sessions::SyncSession* session,
    sync_pb::DebugInfo* debug_info);

}  // namespace syncer

#endif  // SYNC_ENGINE_DOWNLOAD_H_
