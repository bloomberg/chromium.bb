// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/store_timestamps_command.h"

#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/sessions/status_controller.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/syncable.h"

namespace browser_sync {

StoreTimestampsCommand::StoreTimestampsCommand() {}
StoreTimestampsCommand::~StoreTimestampsCommand() {}

SyncerError StoreTimestampsCommand::ExecuteImpl(
    sessions::SyncSession* session) {
  syncable::Directory* dir = session->context()->directory();

  const GetUpdatesResponse& updates =
      session->status_controller().updates_response().get_updates();

  sessions::StatusController* status = session->mutable_status_controller();

  // Update the progress marker tokens from the server result.  If a marker
  // was omitted for any one type, that indicates no change from the previous
  // state.
  syncable::ModelTypeSet forward_progress_types;
  for (int i = 0; i < updates.new_progress_marker_size(); ++i) {
    syncable::ModelType model =
        syncable::GetModelTypeFromSpecificsFieldNumber(
            updates.new_progress_marker(i).data_type_id());
    if (model == syncable::UNSPECIFIED || model == syncable::TOP_LEVEL_FOLDER) {
      NOTREACHED() << "Unintelligible server response.";
      continue;
    }
    forward_progress_types.Put(model);
    dir->SetDownloadProgress(model, updates.new_progress_marker(i));
  }
  DCHECK(!forward_progress_types.Empty() ||
         updates.changes_remaining() == 0);
  if (VLOG_IS_ON(1)) {
    DVLOG_IF(1, !forward_progress_types.Empty())
        << "Get Updates got new progress marker for types: "
        << syncable::ModelTypeSetToString(forward_progress_types)
        << " out of possible: "
        << syncable::ModelTypeSetToString(status->updates_request_types());
  }
  if (updates.has_changes_remaining()) {
    int64 changes_left = updates.changes_remaining();
    DVLOG(1) << "Changes remaining: " << changes_left;
    status->set_num_server_changes_remaining(changes_left);
  }

  return SYNCER_OK;
}

}  // namespace browser_sync
