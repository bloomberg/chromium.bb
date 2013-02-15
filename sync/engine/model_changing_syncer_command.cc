// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_changing_syncer_command.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "sync/sessions/status_controller.h"
#include "sync/sessions/sync_session.h"

namespace syncer {

SyncerError ModelChangingSyncerCommand::ExecuteImpl(
    sessions::SyncSession* session) {
  work_session_ = session;
  SyncerError result = SYNCER_OK;

  const std::set<ModelSafeGroup>& groups_to_change =
      GetGroupsToChange(*work_session_);
  for (size_t i = 0; i < session->context()->workers().size(); ++i) {
    ModelSafeWorker* worker = session->context()->workers()[i];
    ModelSafeGroup group = worker->GetModelSafeGroup();
    // Skip workers whose group isn't active.
    if (groups_to_change.count(group) == 0u) {
      DVLOG(2) << "Skipping worker for group "
               << ModelSafeGroupToString(group);
      continue;
    }

    sessions::StatusController* status =
        work_session_->mutable_status_controller();
    sessions::ScopedModelSafeGroupRestriction r(status, group);
    WorkCallback c = base::Bind(
        &ModelChangingSyncerCommand::StartChangingModel,
        // We wait until the callback is executed. So it is safe to use
        // unretained.
        base::Unretained(this));

    SyncerError this_worker_result = worker->DoWorkAndWaitUntilDone(c);
    // TODO(rlarocque): Figure out a better way to deal with errors from
    // multiple models at once.  See also: crbug.com/109422.
    if (this_worker_result != SYNCER_OK)
      result = this_worker_result;
  }

  return result;
}

}  // namespace syncer
